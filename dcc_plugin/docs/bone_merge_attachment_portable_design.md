# BoneMerge 与 Attachment 跨引擎实现指南

本文把 Source Engine 中 BoneMerge 和 Attachment 的机制抽象成可移植设计，目标是让 Agent 在任意引擎、渲染框架或 ECS/面向对象架构中实现等价功能。

## 核心概念

### Attachment

Attachment 是模型资产里定义的命名挂点。它不是一根真实骨骼，而是一条元数据：

```text
Attachment {
  name: string
  parentBone: bone index
  localTransform: matrix relative to parentBone
  flags: e.g. worldAlign
}
```

运行时查询 attachment 时，先确保模型骨骼已经完成本帧求值，然后计算：

```text
attachmentWorld = parentBoneWorld * attachmentLocal
```

如果 attachment 带 `worldAlign` 语义，则通常只继承骨骼位置，不继承骨骼旋转：

```text
position = TransformPoint(parentBoneWorld, attachmentLocal.translation)
rotation = identity 或外部指定的世界朝向
```

Attachment 适合用于枪口火焰、弹壳出口、眼睛位置、UI 面板挂点、粒子控制点、实体父子挂接等。

### BoneMerge

BoneMerge 是“子模型骨骼矩阵从父模型同名骨骼复制”的机制。它不是挂到单个 attachment，而是建立多根骨骼的名称映射：

```text
childBone[i].name == parentBone[j].name
=> childBoneWorld[i] = parentBoneWorld[j]
```

子模型中没有匹配到父骨骼的骨骼仍使用自己的动画求值。如果这些未匹配骨骼的父级是已 merge 的骨骼，它们会在父骨骼的已复制矩阵基础上继续播放局部动画。

BoneMerge 适合用于手套、袖子、可穿戴装备、第三人称武器 world model、角色外观附件等需要跟随完整骨架的模型。

## 两者差异

| 维度 | Attachment | BoneMerge |
|---|---|---|
| 绑定粒度 | 单个命名挂点 | 多根同名骨骼 |
| 资产要求 | 模型中定义 attachment 元数据 | 父子模型有同名骨骼 |
| 子模型动画 | 整体跟随挂点，内部动画可独立播放 | 同名骨骼被父骨骼覆盖，未匹配骨骼可播放 |
| 常见用途 | 粒子、弹道、道具挂点、相机点 | 可穿戴物、手中武器、共享骨架附件 |
| 运行成本 | 一次骨骼矩阵与局部矩阵相乘 | 父骨架求值 + 多根矩阵复制 + 子骨架局部求值 |
| 失败回退 | 找不到挂点时回退实体原点或失败 | 没有同名骨骼时退化为普通跟随/自身动画 |

## 数据模型

### SkeletonModel

任意引擎需要在模型资产层保存这些数据：

```cpp
struct Bone {
    string name;
    int parentIndex;      // -1 for root
    Matrix4 bindLocal;
    uint32 usageFlags;    // Render, Hitbox, Attachment, BoneMerge, etc.
};

struct AttachmentPoint {
    string name;
    int parentBone;
    Matrix4 localTransform;
    bool worldAlign;
};

struct SkeletonModel {
    vector<Bone> bones;
    vector<AttachmentPoint> attachments;
    unordered_map<string, int> boneNameToIndex;
    unordered_map<string, int> attachmentNameToIndex;
};
```

### AnimatedEntity

运行时实体至少需要：

```cpp
struct AnimatedEntity {
    SkeletonModel* model;
    Transform entityToWorld;

    vector<Transform> localPose;      // 本帧动画局部姿态
    vector<Matrix4> boneToWorld;      // 本帧骨骼世界矩阵
    BitSet computedBones;

    Entity* moveParent;
    int parentAttachment;             // 0 / -1 表示无 attachment
    Transform localToParent;          // 子实体相对父实体或父 attachment 的局部偏移

    BoneMergeComponent* boneMerge;
    vector<AttachmentCache> attachmentCache;
};
```

建议 attachment 索引内部使用 0-based，外部 API 可以按引擎习惯选择 0-based 或 1-based，但必须明确“无效索引”的表示。Source 中 attachment 查询对外常见是 1-based，`0` 表示 invalid。

## Attachment 实现

### 查找

```cpp
int LookupAttachment(AnimatedEntity& entity, string name) {
    if (!entity.model) return InvalidAttachment;
    auto it = entity.model->attachmentNameToIndex.find(name);
    return it == end ? InvalidAttachment : it->second;
}
```

查找应只依赖资产元数据，不应触发昂贵动画求值。

### 计算 attachment world matrix

```cpp
bool GetAttachmentMatrix(AnimatedEntity& entity, int attachmentIndex, Matrix4& out) {
    if (!entity.model || !IsValidAttachment(entity.model, attachmentIndex)) {
        out = entity.entityToWorld.ToMatrix();
        return false;
    }

    const AttachmentPoint& a = entity.model->attachments[attachmentIndex];

    // 只要求 attachment 相关骨骼，避免每次查枪口都完整求全骨架。
    EnsureBones(entity, BoneMask::Attachment);

    Matrix4 boneWorld = entity.boneToWorld[a.parentBone];

    if (!a.worldAlign) {
        out = boneWorld * a.localTransform;
    } else {
        Vec3 p = TransformPoint(boneWorld, GetTranslation(a.localTransform));
        out = Matrix4::Identity();
        SetTranslation(out, p);
    }

    return true;
}
```

### 缓存与速度

如果 attachment 会被粒子、音效、弹道和子实体频繁查询，建议缓存：

```cpp
struct AttachmentCache {
    Matrix4 world;
    Vec3 linearVelocity;
    int lastFrame = -1;
    bool anglesComputed = false;
    Quat cachedRotation;
};
```

每次写入新 attachment world matrix 时，如果上一帧也有结果：

```text
linearVelocity = (currentPosition - previousPosition) / deltaTime
```

这个速度可用于拖尾、beam、粒子继承速度和 attachment-parented 子实体的绝对速度。

### 实体挂到 attachment

实体父子系统应支持“相对父实体原点”和“相对父 attachment”两种父空间。

```cpp
Matrix4 GetParentToWorld(Entity& child) {
    Entity* parent = child.moveParent;
    if (!parent) return Identity;

    if (child.parentAttachment != InvalidAttachment) {
        Matrix4 attachmentWorld;
        if (GetAttachmentMatrix(*parent->AsAnimated(), child.parentAttachment, attachmentWorld)) {
            return attachmentWorld;
        }
    }

    return parent->EntityToWorld();
}

void UpdateChildWorld(Entity& child) {
    child.world = GetParentToWorld(child) * child.localToParent;
}
```

设置 parent attachment 时通常支持两种模式：

- `snap`：子实体局部 transform 清零，直接贴到 attachment。
- `maintainOffset`：保持当前世界 transform，反算相对 attachment 的局部 transform。

```cpp
void SetParentAttachment(Entity& child, Entity& parent, string attachmentName, bool maintainOffset) {
    int idx = LookupAttachment(*parent.AsAnimated(), attachmentName);
    if (!IsValid(idx)) return Fail();

    Matrix4 attachWorld;
    GetAttachmentMatrix(*parent.AsAnimated(), idx, attachWorld);

    child.moveParent = &parent;
    child.parentAttachment = idx;

    if (maintainOffset) {
        child.localToParent = Inverse(attachWorld) * child.world;
    } else {
        child.localToParent = Identity;
    }
}
```

## BoneMerge 实现

### BoneMergeComponent

```cpp
struct BoneMergePair {
    int childBone;
    int parentBone;
};

struct BoneMergeComponent {
    AnimatedEntity* child;
    AnimatedEntity* parent;

    SkeletonModel* cachedChildModel = nullptr;
    SkeletonModel* cachedParentModel = nullptr;

    vector<BoneMergePair> pairs;
    BitSet mergedChildBones;
    uint32 parentRequiredMask;

    bool mergePoseParameters = true; // 如果引擎有 pose param / morph param
};
```

### 建立缓存

缓存应在这些条件变化时重建：

- child model 改变
- parent model 改变
- follow parent 改变
- 骨骼名表重新载入或热更新

```cpp
void RebuildBoneMergeCache(BoneMergeComponent& bm) {
    bm.pairs.clear();
    bm.mergedChildBones.clear();
    bm.parentRequiredMask = BoneMask::BoneMerge;

    if (!bm.child || !bm.parent || !bm.child->model || !bm.parent->model) return;

    for (int childBone = 0; childBone < bm.child->model->bones.size(); ++childBone) {
        const string& name = bm.child->model->bones[childBone].name;
        int parentBone = FindBoneByName(*bm.parent->model, name);
        if (parentBone < 0) continue;

        bm.pairs.push_back({ childBone, parentBone });
        bm.mergedChildBones.set(childBone);

        // 父骨骼及其祖先必须被求值，否则拷贝到的是无效矩阵。
        MarkBoneAndParentsUsedByMask(*bm.parent->model, parentBone, BoneMask::BoneMerge);
        MarkBoneAndParentsUsedByMask(*bm.child->model, childBone, BoneMask::BoneMerge);
    }

    bm.cachedChildModel = bm.child->model;
    bm.cachedParentModel = bm.parent->model;
}
```

### 求值顺序

推荐顺序：

1. 子实体准备本地动画姿态 `localPose`。
2. 如果启用 BoneMerge，确保父实体按需要的 bone mask 完成本帧骨骼世界矩阵。
3. 把父模型同名骨骼 world matrix 复制到子模型对应骨骼。
4. 子模型继续计算未被 merge 的骨骼。若未 merge 骨骼的父骨骼已 merge，则自然基于已复制的父骨骼矩阵求值。
5. 计算子模型 attachment。
6. 渲染、粒子、音效和子实体 transform 使用最终骨骼/attachment 结果。

### 伪代码

```cpp
void BuildBoneMatrices(AnimatedEntity& e, BoneMask mask) {
    EvaluateLocalPose(e, mask);

    if (e.boneMerge && e.boneMerge->parent) {
        ApplyBoneMerge(*e.boneMerge, mask);
    }

    for (int i = 0; i < e.model->bones.size(); ++i) {
        if (!BoneUsedByMask(e.model->bones[i], mask)) continue;
        if (e.boneMerge && e.boneMerge->mergedChildBones.test(i)) continue;

        Matrix4 local = LocalPoseToMatrix(e.localPose[i]);
        int parent = e.model->bones[i].parentIndex;

        if (parent < 0) {
            e.boneToWorld[i] = e.entityToWorld.ToMatrix() * local;
        } else {
            e.boneToWorld[i] = e.boneToWorld[parent] * local;
        }
    }
}

void ApplyBoneMerge(BoneMergeComponent& bm, BoneMask childMask) {
    if (NeedsCacheRebuild(bm)) {
        RebuildBoneMergeCache(bm);
    }
    if (bm.pairs.empty()) return;

    BoneMask parentMask = BoneMask::BoneMerge;
    if (bm.parent->IsPlayerLike()) {
        // 武器/粒子常需要 attachment 骨骼。
        parentMask |= childMask;
        parentMask |= BoneMask::Attachment;
    }

    EnsureBones(*bm.parent, parentMask);

    for (const BoneMergePair& p : bm.pairs) {
        if (!BoneUsedByMask(bm.child->model->bones[p.childBone], childMask)) continue;
        bm.child->boneToWorld[p.childBone] = bm.parent->boneToWorld[p.parentBone];
    }
}
```

### 子骨骼动画规则

BoneMerge 后，子模型动画是否生效由骨骼是否进入映射决定：

```text
同名匹配骨骼：child world matrix 被 parent world matrix 覆盖，child 自身动画不生效。
未匹配骨骼：child 自身动画正常生效。
未匹配骨骼挂在已 merge 父骨骼下：父级跟随 parent，子级继续播放局部动画。
```

这允许常见结构：

```text
weapon_world_model
  ValveBiped.Bip01_R_Hand   <- 被 BoneMerge，跟随玩家右手
    bolt                    <- 子模型独有，可播放枪机动画
    magazine                <- 子模型独有，可播放换弹动画
```

## BoneMerge 与 Attachment 的组合

常见组合方式：

1. 父角色播放动作。
2. 武器 world model BoneMerge 到角色骨架。
3. 武器 world model 内部额外骨骼播放枪机、弹匣动画。
4. 武器 world model 上的 `muzzle_flash` attachment 基于最终骨骼结果计算。
5. 粒子系统使用该 attachment world matrix 生成第三人称枪口火焰。

第一人称也常类似：

1. Viewmodel 作为独立第一人称实体。
2. 手臂模型或饰品模型 BoneMerge 到 viewmodel。
3. 枪口、弹壳、瞄具镜片使用 viewmodel 或武器模型 attachment。

## Culling 与 Bounds

BoneMerge 子实体的逻辑 origin 可能不在实际渲染位置上。若直接用子实体原点裁剪，会出现两类问题：

- 明明可见却被剔除。
- 为避免错误剔除而扩大 bounds，导致太多不可见附件参与渲染。

推荐策略：

- BoneMerge 子实体默认继承父实体 bounds，或使用父实体 bounds 加一个可配置扩张量。
- 对武器 world model 这类已知尺寸，可自定义扩张方式，而不是固定向所有方向膨胀。
- 如果要计算更准确的 origin，可用第一根 merge 骨骼反推出子实体 world transform：

```text
childEntityWorld * childBoneLocal = parentBoneWorld
childEntityWorld = parentBoneWorld * inverse(childBoneLocal)
```

这个结果适合 culling 和调试，但如果子模型动画大幅移动第一根 merge 骨骼，需谨慎使用。

## 网络与预测

多人游戏中建议同步的是关系和状态，而不是每根骨骼：

- parent entity id
- parent attachment id 或 attachment name hash
- boneMerge enabled flag
- child model id / skin / bodygroup / material override
- child animation sequence、cycle、pose params
- 对可预测 viewmodel，客户端可本地计算 BoneMerge 和 Attachment

服务端通常只需要 attachment 查询用于命中、发射点、实体挂接和 AI；客户端负责完整视觉表现。若服务端也要验证枪口、近战或装备位置，需要同一套 attachment/BoneMerge 求值规则，避免客户端和服务端位置不一致。

## 无效化与时序

实现中最容易出错的是“父骨架正在计算时，子实体查询 attachment”。建议：

- 父实体骨骼求值期间设置 `SETTING_UP_BONES` 标志。
- 如果父实体动画导致 attachment 变化，只递归 invalidation 给 attachment 子实体或 following 子实体。
- 子实体发现父骨架正在求值时，可延迟本帧绝对 transform 更新，或使用上一帧 attachment，避免递归重入。
- BoneMerge 需要防止循环依赖：A merge B、B merge A 必须被拒绝。

推荐帧序：

```text
Update animation state
Evaluate parent skeletons
Apply bone merge for followers
Evaluate remaining child bones
Compute attachments
Update attachment-parented child transforms
Render / simulate particles / play effects
```

## 可选能力

### Pose parameter merge

如果引擎有 pose parameters、blend controls 或 additive channels，可按名称把父模型 pose 参数复制到子模型同名参数。Source 的 `CBoneMergeCache::MergeMatchingPoseParams()` 就是这个思路。

### Reverse bone merge

有些角色动画系统会临时把子模型的局部姿态复制回父模型，以便武器模型参与玩家动画或 IK。这是高级能力，不是实现 BoneMerge 的最低要求。实现时要明确所有权：默认应是 parent -> child，只有特定动画节点允许 child -> parent。

### Attachment override

Viewmodel 或组合模型可能希望 attachment 查询委托给当前武器。例如 `viewmodel.LookupAttachment("muzzle")` 实际返回武器模型上的 `muzzle`。这适合第一人称特效，但要避免 name collision 和循环委托。

## 最小实现清单

要在任意引擎实现可用版本，至少需要：

- 模型资产支持骨骼名、父骨骼、attachment 列表和 attachment 本地矩阵。
- 动画实体能按 mask 求骨骼 world matrix。
- `LookupAttachment(name)` 和 `GetAttachmentMatrix(index)`。
- 父子 transform 支持挂到 parent attachment。
- BoneMerge 组件支持按同名骨骼建立缓存。
- BoneMerge 求值时先确保父骨架完成，再复制匹配骨骼矩阵。
- 未匹配子骨骼继续正常动画。
- 缓存失效条件覆盖 parent/model 变化。
- culling bounds 对 BoneMerge 子实体做特殊处理。
- 测试覆盖 attachment、BoneMerge、二者组合、模型切换和 parent 失效。

## 测试用例

1. Attachment 静态测试：模型 root 上建一个 attachment，实体平移/旋转后 attachment world matrix 正确。
2. Attachment 骨骼动画测试：attachment 绑在手骨，手骨动画后 attachment 跟随。
3. WorldAlign 测试：attachment 位置跟随骨骼，但旋转不继承骨骼。
4. Parent attachment 测试：子实体 snap 到 attachment，父模型动画时子实体跟随。
5. Maintain offset 测试：设置 parent attachment 后，子实体世界位置不跳变。
6. BoneMerge 同名骨骼测试：子模型同名骨骼矩阵等于父模型对应骨骼矩阵。
7. BoneMerge 子独有骨骼测试：子独有骨骼仍播放自身动画。
8. BoneMerge + Attachment 测试：子模型 BoneMerge 后，子模型 attachment 基于最终骨架输出正确位置。
9. 模型切换测试：父或子模型变化后，BoneMerge 缓存重建。
10. 循环保护测试：禁止 A 跟随 B 同时 B 跟随 A。
11. Culling 测试：父实体在视野中时，BoneMerge 子模型不会错误剔除。
12. 网络测试：客户端重建 parent、attachment、BoneMerge 状态后位置与服务端关键 attachment 一致。

## Source Engine 对应参考

| 主题 | 文件 |
|---|---|
| BoneMerge 缓存、同名骨骼映射、矩阵复制 | `src/game/shared/bone_merge_cache.cpp` |
| BoneMerge 数据结构 | `src/game/shared/bone_merge_cache.h` |
| 客户端 BoneMerge 调用与跳过已 merge 骨骼 | `src/game/client/c_baseanimating.cpp` |
| 服务器 BoneMerge 矩阵构建路径 | `src/game/server/baseanimating.cpp` |
| `EF_BONEMERGE` / `EF_BONEMERGE_FASTCULL` 定义 | `src/public/const.h` |
| Attachment 资产结构 `mstudioattachment_t` | `src/public/studio.h` |
| Attachment 名称查找 | `src/bonesetup/bone_utils.cpp` |
| 客户端 Attachment 缓存和计算 | `src/game/client/c_baseanimating.cpp` |
| 服务器 Attachment 查询 | `src/game/server/baseanimating.cpp` |
| 实体 FollowEntity 与 BoneMerge flag 设置 | `src/game/shared/baseentity_shared.cpp` |
| 父子实体 attachment transform | `src/game/client/c_baseentity.cpp`、`src/game/server/baseentity.cpp` |
| 武器 world model 使用 BoneMerge/FASTCULL | `src/game/shared/basecombatweapon_shared.cpp` |

