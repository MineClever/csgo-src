# BoneMerge 与 Attachment 跨引擎实现指南

本文面向负责移植功能的 Agent，描述 Source 风格的骨骼合并与挂点机制，并提供引擎无关的数据契约、矩阵公式、算法、求值顺序与验收条件。示例是设计伪代码，不是可直接编译的 SDK。

## 1. 功能与边界

| 机制 | 输入 | 输出 | 适用场景 |
|---|---|---|---|
| Attachment / Socket | 骨骼最终姿态、骨骼局部挂点偏移 | 一个命名坐标系 | 枪口、弹壳、相机、粒子、道具挂接 |
| Attachment Parenting | 父挂点世界矩阵、子实体局部偏移 | 子实体世界矩阵 | 手持道具、背部武器、挂接 UI |
| BoneMerge | 父子骨骼映射、父骨骼最终矩阵 | 子模型部分骨骼的最终矩阵 | 手套、袖子、模块化角色、武器世界模型 |

Attachment 查询本身不建立实体父子关系，也不修改骨骼。BoneMerge 以骨骼为粒度复制姿态；它不会自动完成不同体型之间的动画重定向。

基础 BoneMerge 的规则：同名且被选中合并的子骨骼采用父模型结果，子模型独有骨骼继续播放自身动画。不能把已经合并的同名骨骼再用子动画覆盖，否则破坏严格跟随契约。

## 2. 实现前固定数学约定

本文使用列向量，矩阵乘法从右往左作用，`T_XY` 表示将 Y 空间坐标变换到 X 空间。矩阵存储的行主序/列主序与这一数学约定是两回事。

```text
E             = 实体/模型空间到世界空间
L[i]          = 骨骼 i 到其父骨骼的本帧局部矩阵
W[i]          = 骨骼 i 到世界空间的最终矩阵
A             = attachment 到绑定骨骼空间的局部矩阵
B[i]          = 绑定姿态下骨骼 i 到网格模型空间的矩阵

根骨骼:       W[i] = E * L[i]
普通子骨骼:   W[i] = W[parent(i)] * L[i]
挂点:         W_attachment = W[bone] * A
```

采用行向量的引擎应系统性反转乘法顺序。资产导入时统一长度单位、坐标轴、左右手系、四元数与旋转顺序。优先保留矩阵，不要在每次挂接中往返转换欧拉角。

最小版本建议只支持刚体变换或一致缩放。非一致缩放、负缩放可能产生剪切，不能默认用 TRS 分解后再组合而保持等价；需要保留仿射矩阵，或在资产校验时明确拒绝。

## 3. 数据契约

```cpp
struct BoneDef {
    StableName name;
    int parent;                 // -1 表示根
    Mat4 bindLocal;
    Mat4 inverseBind;           // 网格模型空间 -> 骨骼绑定空间
};

struct AttachmentDef {
    StableName name;
    int bone;
    Mat4 localToBone;
    bool worldAlign;
};

struct SkeletonAsset {
    AssetId id;
    uint64 revision;
    Array<BoneDef> bones;
    Array<int> parentFirstOrder;
    Array<AttachmentDef> attachments;
    NameMap boneLookup;
    NameMap attachmentLookup;
};

struct PoseSnapshot {
    uint64 poseRevision;
    SampleTime sampleTime;
    Mat4 entityWorld;
    Array<Mat4> localPose;
    Array<Mat4> boneWorld;
    BitSet validBones;
};

struct MergePair { int childBone; int parentBone; };

struct BoneMergeBinding {
    EntityHandle source;        // 被跟随的父实体
    Array<MergePair> pairs;
    BitSet mergedBones;
    BitSet requiredParentBones;
    BindingCacheKey key;
};
```

实体句柄应包含 generation，避免删除后复用 ID 造成错误绑定。名称匹配必须定义大小写规则，并检查规范化后的重复名称及 hash 冲突。不能只靠 hash 相等判定名称相等。

Attachment 索引建议内部使用 0-based，`-1` 无效；若兼容 Source，对外包装成 1-based，`0` 无效。禁止在算法内部混用两种索引。

## 4. Attachment 算法

### 4.1 查找和求值

名称查找只访问资产，不应触发动画求值。解析后的句柄绑定资产 ID 和 revision，模型替换后重新解析。

```cpp
Result<Mat4> EvaluateAttachment(const PoseSnapshot& pose,
                                const AttachmentDef& socket) {
    if (!pose.validBones.test(socket.bone))
        return Error::PoseNotReady;

    Mat4 result = pose.boneWorld[socket.bone] * socket.localToBone;
    if (socket.worldAlign) {
        Vec3 position = Translation(result);
        result = TranslationMatrix(position);
    }
    return result;
}
```

Source 的 `WORLD_ALIGN` 分支保留变换后的位置，以世界单位坐标轴构建旋转矩阵。若目标引擎希望继承缩放或指定其他朝向，应作为独立策略声明。

`GetAttachmentLocal()` 常表示相对实体空间，而不是资产中的骨骼局部偏移：

```text
attachmentEntityLocal = inverse(E) * W_attachment
assetAttachmentLocal  = A
```

两者不同，API 名称应体现空间。逆矩阵失败时返回错误，不能继续传播 NaN。

### 4.2 挂接子实体

设 `O` 是子实体相对挂点的偏移：

```text
childEntityWorld = W_attachment * O
```

三种操作语义：

- Snap：`O = identity`，子实体原点对齐挂点。
- KeepWorld：`O = inverse(W_attachment) * oldChildWorld`，挂接瞬间世界姿态不变。
- KeepLocal：保留原有 `O`，明确它会被解释到新的父空间中。

换挂点即使父实体相同，也必须更新绑定。先校验目标和依赖环，再在同一次事务里发布父句柄、挂点句柄、偏移矩阵，并使派生缓存失效。

如果子物体自身有 grip 挂点，且 grip 相对子实体的矩阵为 `G`，要让 grip 对齐父 socket，应使用：

```text
childEntityWorld = W_parentSocket * desiredGripOffset * inverse(G)
```

如果 `G` 会随子动画变化，要把它加入求值依赖，避免同帧循环。

### 4.3 速度和解除挂接

同一时间域内可由相邻样本计算位置速度：`v = (p_now - p_prev) / dt`。角速度应由四元数最短弧差得到，不能直接相减欧拉角。

传送、重新绑定、时间倒退、零 dt 时重置速度历史。掉落时可继承挂点线速度；若挂点只提供刚体中心速度，则补上 `omega cross r`。若已由挂点位置差分得到完整速度，不能再次叠加相同的旋转贡献。

解绑时保留最终世界姿态，移除约束，再把控制权交给物理系统。避免物理积分和挂接约束同时写同一个 transform。

## 5. BoneMerge 算法

### 5.1 映射缓存

Source 通过子骨骼名称查询父模型同名骨骼，缓存索引对和合并位集合。移植版本可以支持显式白名单或别名映射，但必须区分默认行为和扩展行为。

```text
for each childBone:
    parentBone = FindParentBone(childBone.name)
    if found and allowedByMergePolicy:
        pairs.add(childBone, parentBone)
        mergedBones.set(childBone)
        requiredParentBones.add(parentBone and all its ancestors)
```

在父骨架拓扑、子骨架拓扑、模型 revision、源实体 generation、映射策略变化时重建。空映射和失效源也要清空旧位集合，不能遗留上次的 mergedBones。

推荐把所需骨骼集合存放在实例绑定上。Source 为求值需要会标记模型骨骼 usage flags；在共享不可变资产的现代引擎中不宜照搬运行时修改共享资产的方式。

### 5.2 最终世界矩阵合并

```cpp
void BuildMergedPose(const SkeletonAsset& childAsset,
                     const PoseSnapshot& source,
                     const Array<Mat4>& childLocalPose,
                     const Mat4& childEntityWorld,
                     const BoneMergeBinding& binding,
                     const BitSet& requiredChildBones,
                     Array<Mat4>& outWorld) {
    // 调用方已验证映射有效、父姿态时间一致，且计算了祖先闭包。
    Array<int> sourceForChild = BuildIndexMap(binding.pairs);
    for (int i : childAsset.parentFirstOrder) {
        if (!requiredChildBones.test(i)) continue;
        int sourceBone = sourceForChild[i];
        if (sourceBone >= 0) {
            Require(source.validBones.test(sourceBone));
            outWorld[i] = source.boneWorld[sourceBone];
        } else {
            int parent = childAsset.bones[i].parent;
            outWorld[i] = (parent < 0 ? childEntityWorld : outWorld[parent])
                          * childLocalPose[i];
        }
    }
}
```

`sourceForChild` 应随绑定缓存，而不是每帧创建。传入的 requiredChildBones 必须包含渲染、挂点、碰撞、合并消费者所需骨骼及祖先。骨骼数组不保证按拓扑排序，所以显式使用 parentFirstOrder。

没有匹配的骨骼使用自身局部动画。若一个已匹配后代跨过未匹配中间骨骼，它仍直接采用父模型结果，可能与自身骨架局部链不连续；资产检查应报告这种结构供制作者确认。

### 5.3 子动画的作用范围

```text
child.hand       -> 映射 parent.hand，完全复制其世界矩阵
child.hand.bolt  -> 父模型无同名 bolt，使用自身局部动画
child.magazine  -> 若也映射到 parent.magazine，则自身动画同样被覆盖
```

是否能播放子动画取决于最终映射，不能仅凭“这是子骨骼”判断。若要让已映射骨骼播放额外运动，可增加以下显式模式：

- 排除该骨骼及必要子树，使其使用子动画。
- 合并矩阵后再应用 additive delta：`W_child = W_source * D`，明确 D 的空间。
- 加权约束或动画重定向。

后两项属于扩展，不是严格 Source BoneMerge 的默认矩阵复制语义。

### 5.4 与蒙皮管线衔接

不能把骨骼世界矩阵误当成已经乘过 inverse bind 的蒙皮矩阵。若渲染器在子网格模型空间蒙皮，提交：

```text
skinPalette_child[i] = inverse(E_child) * W_child[i] * inverseBind_child[i]
worldVertex = E_child * sum(weight[i] * skinPalette_child[i] * bindVertex)
```

使用子网格自身的 inverse bind；不要复制父模型最终 skin palette，因为父子绑定姿态可能不同。若渲染器直接输出世界空间顶点，可提交 `W_child * inverseBind_child`，同时取消额外实体 world 乘法，防止变换两次。

只能写 local pose 的引擎可先生成目标世界矩阵，再按子骨架层级转换：`L_child[i] = inverse(W_child[parent]) * W_child[i]`；根部使用 `inverse(E_child)`。在最终约束阶段发布结果，防止随后 Animator 再覆盖。

严格复制不会修复体型、骨轴、绑定姿态不兼容引起的拉伸。需要保留绑定偏移时，可定义独立的约束模式 `W_child = W_source * correction`；它与直接同名矩阵复制具有不同契约。

## 6. 组合求值与调度

典型链路为：角色最终骨架 -> 武器 BoneMerge -> 武器独有骨骼动画 -> 武器 muzzle attachment -> 粒子或子实体。

用依赖图调度，节点可以细分为局部姿态、最终骨架、挂点、实体变换。一个实体级排序无法充分描述武器驱动手部 IK 的复杂情况。

```text
动画时间/状态更新
    -> 局部动画混合、root motion、父角色 IK/物理修正
    -> 发布父最终骨架
    -> 合并子骨骼，计算未合并骨骼
    -> 发布子最终骨架
    -> 计算挂点
    -> 更新挂点子实体、特效、渲染蒙皮矩阵
```

手部 IK 与武器互相驱动时必须分阶段：先用预约束武器姿态求 IK 目标，再解角色骨架，最后跟随/合并武器；或选择另一套明确的单向所有权。禁止通过递归 GetAttachment 隐式解决环。

严格模式下循环依赖、查询计算中的姿态均应报错。若视觉系统允许上一帧回退，应显式标注样本时间；命中、发射点等逻辑查询不能静默使用陈旧结果。

多线程使用不可变 PoseSnapshot 和任务依赖。不得在渲染读骨架的同时原地写矩阵数组；用双缓冲或 revision 发布机制。

## 7. 缓存、LOD 与剔除

挂点缓存键至少包括资产 revision、pose revision、entity world revision 和采样时间。一个 frameId 无法区分同帧预测重演、多个视图或服务器子步。

只请求某个挂点时，计算其骨骼与祖先即可。LOD 可以减少视觉骨骼，但合并和逻辑挂点所需骨骼必须保留或显式重映射；不能读取被 LOD 淘汰的矩阵。

BoneMerge 后网格可能远离实体逻辑原点，因此 bounds 必须覆盖最终几何。可使用父 bounds 与附件保守 bounds 的并集，或变换各骨骼包围盒获得更精确结果。仅继承父 bounds 对长武器不一定足够。

Source 的 FASTCULL 是跟随位置/包围盒优化，不改变骨骼矩阵复制规则。移植时应单独实现剔除策略。父实体不可见不代表其骨架可以停算：可见子模型、粒子或逻辑挂点仍可能依赖它。

## 8. 资产与生命周期

- 导入骨骼后统一构建拓扑排序、名称表和 inverse bind。
- 将 DCC locator/socket 导入为 AttachmentDef，保留其相对骨骼偏移。
- 组合模型必须先将局部骨骼索引重映射到最终骨架；Source 的 GetAttachmentBone 会处理 included model 的索引映射。
- 检查非法骨骼索引、循环骨架、重复名称、奇异矩阵和单位不匹配。
- 模型切换、热重载或父实体销毁时原子替换绑定并使缓存失效。
- 为缺失挂点定义策略：返回错误、隐藏附件、解绑保留世界姿态，或明确回退实体原点。返回回退值时仍保留失败状态。
- BoneMerge 与 attachment parenting 同时作用于同一实体时，明确实体根变换与骨骼约束各自职责。完整世界矩阵合并会覆盖映射骨骼的实体偏移，不能指望额外 parenting 偏移自动再次作用。

## 9. 网络、预测与第一人称

同步关系和动画状态通常足够：实体句柄、模型 ID、挂点稳定标识、偏移、合并策略、动画状态。客户端由本地骨架重建最终结果，通常无需逐骨骼网络同步。

挂点数字索引依赖资产版本；跨版本应用稳定名称/ID 并校验模型 revision。预测回滚后应重新计算姿态和挂点，避免复用未来样本。

服务器模拟姿态、客户端插值姿态和第一人称视图姿态属于不同时间域。它们在同一时刻和相同输入下应遵守同样数学规则，但不要求视觉枪口与服务器枪口始终逐帧一致。

Viewmodel 常有独立 FOV、相机相对变换或 attachment 投影修正。必须区分视觉挂点与真实世界逻辑挂点；服务器射线/投射物不能直接使用经过第一人称显示修正的位置。

## 10. Agent 实施顺序与验收

1. 写出目标引擎的矩阵约定和骨架/蒙皮空间，确认动画管线可插入的最终约束阶段。
2. 实现资产挂点、名称查找、世界/实体局部查询及失败返回。
3. 实现 Snap、KeepWorld、KeepLocal、换挂点和解绑。
4. 实现按名称映射与拓扑骨架合并，先做无 LOD 的正确版本。
5. 接入自身未匹配骨骼动画和子网格 inverse bind 蒙皮。
6. 接入依赖图、缓存 revision、生命周期与线程发布。
7. 增加 LOD、bounds、预测和引擎特有扩展。

| 测试 | 必须满足的结果 |
|---|---|
| 骨骼旋转与实体平移后查挂点 | 等于 `W_bone * A` |
| WORLD_ALIGN | 位置正确，旋转是世界单位朝向 |
| KeepWorld | 挂接瞬间世界矩阵不变 |
| 同一父实体换 socket | 绑定和世界结果立即更新 |
| 已映射骨骼含子动画 | 严格模式下等于源骨骼矩阵 |
| 未映射 bolt 动画 | 局部运动保留并继承已合并父骨骼 |
| 子实体有非单位世界变换 | 无二次平移/旋转，蒙皮结果正确 |
| 父子 inverse bind 不同 | 使用子网格 inverse bind，结果符合资产预期 |
| 部分骨骼查询/LOD | 请求骨骼及祖先有效，无旧矩阵 |
| 模型切换、失效源、空映射 | 清理旧映射并按策略回退 |
| 父不可见、子可见 | 依赖骨架继续求值，子不误剔除 |
| 同帧多次求值/回滚 | revision 和时间正确，无未来缓存 |
| 环和并发 | 环被拒绝，消费者看不到半成品姿态 |
| 解绑后物理掉落 | 世界姿态连续，速度无重复累加 |

可用数值样例验证顺序：父骨骼世界位置 `(10,0,0)`，绕 Z 旋转 90 度，挂点局部位置 `(2,0,0)`，则挂点世界位置约为 `(10,2,0)`。合并子骨骼后，相同局部位移的未匹配子骨骼也应落在该位置。

对比矩阵使用绝对/相对容差；旋转使用四元数夹角，位置容差按项目单位定义。为调试视图绘制骨骼轴、挂点轴、映射连线、bounds，以及 pose revision/sampleTime。

## 11. Source 源码阅读索引

以下路径相对仓库根目录；通过符号定位可避免行号随版本漂移。

| 文件 | 关键符号与用途 |
|---|---|
| `src/game/shared/bone_merge_cache.cpp` | `UpdateCache`、`MergeMatchingBones`、`BuildMatricesWithBoneMerge`，名称映射与最终矩阵合并 |
| `src/game/shared/bone_merge_cache.h` | `CBoneMergeCache`、映射对、位集合 |
| `src/game/client/c_baseanimating.cpp` | `CalcBoneMerge`、`BuildTransformations`，跳过已合并骨骼；`SetupBones_AttachmentHelper`、`GetAttachment` |
| `src/game/server/baseanimating.cpp` | 服务器骨骼合并与 `LookupAttachment`、`GetAttachment`、`GetAttachmentLocal` |
| `src/public/studio.h` | `mstudioattachment_t`、`ATTACHMENT_FLAG_WORLD_ALIGN` |
| `src/public/studio.cpp` | `GetAttachmentBone`，组合模型骨骼重映射 |
| `src/bonesetup/bone_utils.cpp` | `Studio_FindAttachment`，名称查找 |
| `src/game/shared/baseentity_shared.cpp` | `FollowEntity`、子实体变换无效化 |
| `src/game/server/baseentity.cpp` | `SetParentAttachment`、`GetParentToWorldTransform` |
| `src/game/client/c_baseentity.cpp` | 父挂点空间到子实体世界空间更新 |
| `src/public/const.h` | `EF_BONEMERGE`、`EF_BONEMERGE_FASTCULL` |
| `src/game/shared/basecombatweapon_shared.cpp` | `CBaseWeaponWorldModel::ValidateParent`，武器世界模型跟随 |
| `src/game/client/c_baseanimatingoverlay.cpp` | 玩家/武器动画协作与局部姿态拷贝 |
| `src/game/server/BaseAnimatingOverlay.cpp` | 服务端动画协作与 reverse merge 路径 |

`MergeMatchingPoseParams` 按名称同步姿态参数；跨引擎实现还需校验参数范围和单位。`CopyFromFollow/CopyToFollow` 交换局部位置/四元数以服务动画协作，不等同于基础 BoneMerge 的世界矩阵复制。Reverse merge 需要单独的分阶段数据流与所有权设计。

配套流程图：[bone_merge_attachment_flow.puml](bone_merge_attachment_flow.puml)。

角色与武器序列协作的完整实现契约见 [第三人称角色与武器动画协作：独立实现规范](weapon_character_animation_portable_design.md)，包括局部姿势反向写回、武器自身动画、最终矩阵合并、无环调度及验收样例。
