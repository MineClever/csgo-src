# Source Engine 武器挂载实现原理

本文整理本仓库中 Source/CS:GO 武器“挂载”的主要实现链路。这里的挂载不是单一 API，而是几层关系共同完成：

- 库存挂载：武器实体进入角色的 `m_hMyWeapons`，并通过 `m_hOwner` / `SetOwnerEntity()` 指向持有者。
- 激活挂载：角色的 `m_hActiveWeapon` 指向当前武器，切换时执行 `Holster()` / `Deploy()`。
- 第一人称挂载：玩家持有的 `viewmodel` 或 `predicted_viewmodel` 设置当前武器模型和动画。
- 第三人称挂载：`weaponworldmodel` 跟随玩家或武器实体，通过 bonemerge 与玩家骨骼/动画同步。
- 网络挂载：上述关键句柄和模型索引作为网络字段同步到客户端。

## 总体链路

1. 武器脚本被解析为 `FileWeaponInfo_t` / `CCSWeaponInfo`，其中 `viewmodel`、`playermodel`、掉落模型、动画前缀和 CS:GO 专用参数成为运行时武器数据。
2. 武器实体 `Spawn()` / `Precache()` 时读取脚本数据，预缓存第一人称模型、第三人称模型和掉落模型，并在服务器侧创建 `weaponworldmodel`。
3. 玩家碰到世界中的武器时，触发 `DefaultTouch()`，再进入 `BumpWeapon()` 和 `Weapon_Equip()`。
4. `Weapon_Equip()` 把武器写入角色库存数组 `m_hMyWeapons`，随后 `CBaseCombatWeapon::Equip()` 让武器跟随 owner，设置 owner 句柄和 owner entity。
5. 如果游戏规则允许自动切换，`Weapon_Switch()` 会先 `Holster()` 旧武器，再把 `m_hActiveWeapon` 指向新武器并调用新武器 `Deploy()`。
6. `Deploy()` 调用 `DefaultDeploy()`，里面通过 `SetViewModel()` 让玩家 viewmodel 载入 `GetViewModel()` 返回的模型，并播放 draw 动画。
7. 对第三人称表现，持有状态下原始 weapon entity 会被隐藏，`weaponworldmodel` 负责渲染手中武器；掉落后武器停止 follow，恢复碰撞、重力和拾取 touch。

## 关键对象

### `CBaseCombatCharacter`

`CBaseCombatCharacter` 是武器挂载的 owner 侧基础类，负责库存和当前武器。

- `m_hMyWeapons[MAX_WEAPONS]`：角色持有的武器句柄数组。
- `m_hActiveWeapon`：当前激活武器。
- `Weapon_Equip()`：把武器放入库存，给默认弹药，调用 `pWeapon->Equip(this)`。
- `Weapon_Switch()`：处理切换条件、旧武器 `Holster()`、新武器 `Deploy()`。
- `Weapon_Drop()` / `Weapon_Detach()`：从库存和 active handle 中解除武器。

### `CBaseCombatWeapon`

`CBaseCombatWeapon` 是武器实体本身，负责 owner 关系、模型选择、部署/收起和掉落状态。

- `m_hOwner`：持有者句柄。
- `m_nViewModelIndex`：使用哪个 viewmodel 槽，常规主手为 0。
- `m_iViewModelIndex`、`m_iWorldModelIndex`、`m_iWorldDroppedModelIndex`：预缓存后的模型索引。
- `m_hWeaponWorldModel`：手持第三人称武器模型实体。
- `Equip()`：`FollowEntity(pOwner)`，设置 `m_hOwner` 和 `SetOwnerEntity(pOwner)`。
- `SetViewModel()`：找到 owner 的 viewmodel，调用 `vm->SetWeaponModel(GetViewModel(...), this)`。
- `Deploy()` / `DefaultDeploy()`：创建/隐藏 world model，设置 viewmodel，播放部署动画，显示 viewmodel。
- `Holster()`：播放收起动画，隐藏 world model / viewmodel。
- `Drop()`：停止 follow，清空 owner，恢复物理、碰撞和拾取。

### `CBaseViewModel` / `CPredictedViewModel`

Viewmodel 是第一人称手臂/武器显示载体。

- `m_hOwner`：拥有该 viewmodel 的玩家。
- `m_hWeapon`：当前挂载到该 viewmodel 的武器。
- `m_nViewModelIndex`：viewmodel 槽位。
- `SetWeaponModel()`：记录武器句柄并设置 viewmodel 模型。
- attachment 查询会在武器要求时委托给武器本身，用于 muzzle、shell eject 等效果位置。
- CS:GO 的 `CCSPlayer::CreateViewModel()` 创建的是 `predicted_viewmodel`，用于客户端预测下的第一人称表现。

### `CBaseWeaponWorldModel`

`weaponworldmodel` 是第三人称手中武器的渲染实体。

- `m_hCombatWeaponParent`：指回逻辑武器实体。
- `SetOwningWeapon()`：设置模型、记录 parent weapon，并 `FollowEntity()`。
- `ValidateParent()`：如果武器有玩家 owner，则 world model 跟随玩家；否则跟随武器实体。
- `ShowWorldModel()`：通过 `EF_NODRAW` 控制可见性。
- `ShouldDraw()`：客户端侧确认 owner、active weapon、观察者模式等条件，决定是否绘制。
- `SetupBones()`：当跟随玩家时限制骨骼计算范围，同时保留 attachment/sticker 等需要的骨骼数据。

## CS:GO 覆盖层

CS:GO 在通用武器系统上增加了经济物品、槽位规则、皮肤材质、掉落限制和预测 viewmodel。

- `CWeaponCSBase` 继承 `CBaseCombatWeapon`，覆盖 `DefaultDeploy()`、`Deploy()`、`Holster()`、`Drop()`、`DefaultTouch()`、`GetViewModel()` 等。
- `CCSPlayer::Weapon_Equip()` 对步枪/手枪槽位执行先丢弃旧武器的规则，对手雷和 stackable item 执行叠加/只补弹逻辑。
- `CCSPlayer::BumpWeapon()` 增加 C4、盾牌、Taser、购买/拾取限制、前 owner 拾取延迟等规则。
- `CCSPlayer::CreateViewModel()` 创建 `predicted_viewmodel`，而不是基础 `viewmodel`。
- `FileWeaponInfo_t::GetViewModel()` / `GetWorldModel()` 会被 econ item override 覆盖，皮肤、贴纸和展示模型可来自物品定义而不是纯脚本字段。

## 关键流程

### 拾取到挂载

1. 世界武器有 touch 回调，玩家触碰后进入 `CBaseCombatWeapon::DefaultTouch()` 或 `CWeaponCSBase::DefaultTouch()`。
2. `DefaultTouch()` 调用玩家的 `BumpWeapon()`。
3. `BumpWeapon()` 检查 owner、规则、可见性、槽位和特殊模式。
4. 检查通过后，武器先被设为不可碰撞/不可见，再调用 `Weapon_Equip()`。
5. `Weapon_Equip()` 把武器写入 `m_hMyWeapons`，随后 `Equip()` 建立 owner 和 follow 关系。
6. 自动切换时，`Weapon_Switch()` 将其设为 active weapon 并部署。

### 切换到第一人称挂载

1. `Weapon_Switch(newWeapon)` 确认可部署、旧武器可收起、弹药条件通过。
2. 旧武器执行 `Holster(newWeapon)`。
3. `m_hActiveWeapon = newWeapon`。
4. 新武器执行 `Deploy()`。
5. `DefaultDeploy()` 调用 `SetViewModel()`。
6. `SetViewModel()` 找到 `owner->GetViewModel(m_nViewModelIndex)`。
7. `CBaseViewModel::SetWeaponModel(model, weapon)` 设置 viewmodel 模型和 `m_hWeapon`。
8. `SendWeaponAnim(ACT_VM_DRAW)` 播放部署动画。

### 第三人称手中武器

1. 武器 `Spawn()` / `Deploy()` 确保服务器侧存在 `weaponworldmodel`。
2. `CBaseWeaponWorldModel::SetOwningWeapon()` 使用 `GetWorldModel()` 设模型，并记录 parent weapon。
3. `ValidateParent()` 根据 owner 选择跟随对象：有玩家 owner 时跟随玩家，否则跟随 weapon entity。
4. 持有者是玩家时，原始 `CBaseCombatWeapon` 实体通常 `EF_NODRAW`，实际可见手中枪由 `weaponworldmodel` 绘制。
5. 客户端 `ShouldDraw()` 要求 parent weapon 是 owner 的真实 active weapon，避免旁观、回放或第一人称观察时重复绘制。

### 掉落与解除挂载

1. `Weapon_Drop()` 计算掉落位置和速度。
2. 武器 `Drop()` 停止 follow，设置 `MOVETYPE_FLYGRAVITY`，清空 owner，恢复可见和 touch。
3. `Weapon_Detach()` 从 `m_hMyWeapons` 移除该武器。
4. 如果被丢弃的是 active weapon，`ClearActiveWeapon()` 清空 `m_hActiveWeapon`。
5. CS:GO 的 `CWeaponCSBase::Drop()` 额外记录前 owner、拾取延迟、加入 dropped weapon 列表，并恢复上下文模型。

## 网络同步

挂载状态主要靠 EHandle 和模型索引同步：

- `DT_BaseCombatCharacter` 同步 `m_hActiveWeapon`、`m_hMyWeapons`。
- `DT_BaseCombatWeapon` 同步 owner、viewmodel/worldmodel 索引、weapon world model 句柄和攻击时间等字段。
- `DT_BaseViewModel` 同步 `m_hWeapon`、`m_hOwner`、`m_nViewModelIndex`、模型索引、序列、皮肤、bodygroup 和效果标记。
- `DT_PredictedViewModel` 继承 viewmodel 网络表，CS:GO 用它支持预测和武器 sway/headbob。

## 关键文件地址

| 文件 | 作用 |
|---|---|
| `src/game/shared/basecombatcharacter_shared.cpp` | 通用 `Weapon_Switch()`、`Weapon_CanSwitchTo()`、`GetActiveWeapon()`、库存查询逻辑 |
| `src/game/server/basecombatcharacter.cpp` | 服务器侧 `Weapon_Equip()`、`Weapon_Drop()`、`Weapon_Detach()`、`SetActiveWeapon()` |
| `src/game/server/basecombatcharacter.h` | `m_hMyWeapons`、`m_hActiveWeapon` 网络字段声明 |
| `src/game/client/c_basecombatcharacter.cpp` | 客户端接收 `m_hMyWeapons`、`m_hActiveWeapon` |
| `src/game/shared/basecombatweapon_shared.cpp` | 通用武器 `Spawn()`、`Precache()`、`Equip()`、`Drop()`、`Deploy()`、`Holster()`、`SetViewModel()`、`CBaseWeaponWorldModel` |
| `src/game/shared/basecombatweapon_shared.h` | `m_hOwner`、模型索引、`m_hWeaponWorldModel`、`CBaseWeaponWorldModel` 声明 |
| `src/game/shared/baseviewmodel_shared.cpp` | `CBaseViewModel::SetWeaponModel()`、viewmodel 网络表、attachment 委托 |
| `src/game/shared/baseviewmodel_shared.h` | viewmodel 的 `m_hWeapon`、`m_hOwner`、`m_nViewModelIndex` 声明 |
| `src/game/server/baseviewmodel.cpp` | viewmodel 服务器发送条件，确保只发给 owner 或旁观者 |
| `src/game/server/player.cpp` | 基础玩家 `CreateViewModel()`、`BumpWeapon()`、`Weapon_Equip()`、`HideViewModels()` |
| `src/game/client/c_baseplayer.cpp` | 客户端玩家 viewmodel 句柄接收和访问 |
| `src/game/shared/predicted_viewmodel.cpp` | `CPredictedViewModel`，CS:GO 第一人称预测、sway/headbob 逻辑 |
| `src/game/shared/weapon_parse.cpp` | `FileWeaponInfo_t` 解析 `viewmodel` / `playermodel`，提供模型查询 |
| `src/game/shared/weapon_parse.h` | 武器脚本数据结构和模型查询接口 |
| `src/game/shared/cstrike15/cs_weapon_parse.cpp` | CS:GO `CCSWeaponInfo` 解析，包含盾牌、特效、动画扩展等 |
| `src/game/shared/cstrike15/cs_weapon_parse.h` | CS:GO 武器数据结构 |
| `src/game/shared/cstrike15/weapon_csbase.cpp` | CS:GO 武器部署、收起、掉落、拾取、viewmodel 选择、world model 模型索引 |
| `src/game/shared/cstrike15/weapon_csbase.h` | `CWeaponCSBase` 声明 |
| `src/game/shared/cstrike15/weapon_csbasegun.cpp` | 枪械类对 `Deploy()`、`Holster()`、`Drop()`、射击动画/特效的补充 |
| `src/game/server/cstrike15/cs_player.cpp` | CS:GO 玩家拾取、装备、切换、`predicted_viewmodel` 创建和槽位规则 |
| `src/game/server/cstrike15/cs_player.h` | CS:GO 玩家武器相关接口声明 |
| `src/game/client/cstrike15/c_cs_player.cpp` | 客户端 CS:GO 玩家 active weapon、动画和 viewmodel 相关逻辑 |

## 维护注意点

- 不要把“武器实体可见”与“手中武器可见”混为一谈。玩家持有时原始 weapon entity 通常隐藏，第三人称显示由 `weaponworldmodel` 负责。
- 修改拾取逻辑时要同时检查 `Weapon_Equip()`、`BumpWeapon()`、`DefaultTouch()` 和 CS:GO 覆盖层，否则容易造成 owner、库存和 touch 状态不同步。
- 修改 viewmodel 时要确认 `m_nViewModelIndex`、`m_hWeapon`、`m_hOwner` 的网络同步，以及 `predicted_viewmodel` 的预测路径。
- 修改 world model 时要检查 `CBaseWeaponWorldModel::ValidateParent()` 和 `ShouldDraw()`，尤其是旁观、HLTV、第一人称观察和 dormant owner 场景。
- 武器模型路径并不总来自脚本，CS:GO econ item 可以覆盖 view/world/dropped model。

配套 PlantUML：`docs/source_engine_weapon_mounting_flow.puml`。
