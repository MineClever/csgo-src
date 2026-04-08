# 项目计划

此文件用于跟踪在仓库操作过程中确认的问题、待修复事项和后续工作。

更新规则：
- 每次任务结束后，都要检查本次是否发现了新的确认问题、解决了已有问题，或调整了优先级。
- 将此文件作为仓库范围内的统一修复计划，不要把关键待办只留在对话记录中。
- 仅记录可执行、可追踪的问题，并提供足够的上下文，方便后续快速接手。

## 当前构建问题

### 1. `client_csgo` 链接失败，阻塞完整 Release 构建
- 状态：未解决
- 优先级：高
- 目标文件：[src/game/client/CMakeLists.txt](src/game/client/CMakeLists.txt)
- 现象：
  - 当前 [build/temp_build_log.log](build/temp_build_log.log) 中已经不再出现旧的 protobuf `_MSC_VER` 不匹配错误，说明源码构建 protobuf 已生效。
  - `base_gcmessages.pb.obj` / `cstrike15_gcmessages.pb.obj` 仍然存在 `protobuf_AddDesc_steammessages_2eproto` 未解析符号。
  - `client_csgo` 仍然缺少大量外部符号，包括 `RandomInt`、`RandomFloat`、`RandomSeed`、`g_pThreadPool`、`KeyValuesSystem`。
- 可能原因：
  - protobuf 工具链不匹配问题已经消除，但仍有部分生成的 protobuf 源文件没有参与最终链接。
  - `client_csgo` 目标缺少一个或多个原始 VPC 工程里存在的依赖库，用于提供 tier0/tier1/vstdlib、随机数、线程池、KeyValues 等符号。
- 后续步骤：
  - 保持 `prebuilt::libprotobuf` 指向新的源码构建目标，重点排查剩余 protobuf 生成代码缺失问题。
  - 对比原始 VPC 工程与当前 CMake 的链接库列表，找出 `client_csgo` 缺失的库。
  - 确认 `steammessages.proto` 对应的生成文件应该由哪个目标编译并参与链接。

### 2. `togl_togl` 在当前 Windows SDK 的 OpenGL 头文件上编译失败
- 状态：未解决
- 优先级：高
- 目标目录：[src/togl](src/togl)
- 现象：
  - 编译在 Windows SDK `um/GL/gl.h` 约第 1171 行附近失败，出现大量 `C2086`、`C2144`、`C2182`、`C4430`、`C1003` 错误。
- 可能原因：
  - 在包含 `gl.h` 之前，已有宏或类型定义与 OpenGL 头冲突。
  - 头文件包含顺序或平台宏与旧版 Source 构建环境不一致。
- 后续步骤：
  - 检查 `src/togl` 的头文件包含链，并与旧 Valve 工程环境对比。
  - 确认是否有 `APIENTRY`、`WINGDIAPI`、`GLAPI` 等宏在 `gl.h` 之前被重定义。
  - 如有必要，增加兼容包装头，隔离新版 Windows SDK 的 OpenGL 头差异。

### 3. `tracker_AdminServer` 与当前基础库 API 不兼容
- 状态：未解决
- 优先级：中
- 目标文件：
  - [RemoteServer.cpp](src/tracker/AdminServer/RemoteServer.cpp)
  - [serverinfopanel.cpp](src/tracker/AdminServer/serverinfopanel.cpp)
- 现象：
  - `RemoteServer.cpp(154)` 调用 `CUtlBuffer::GetString` 时参数签名不匹配（`C2660`）。
  - `serverinfopanel.cpp(249)` 使用了不存在的标识符 `use_V_isspace_instead_of_isspace`（`C3861`）。
- 可能原因：
  - 这部分代码来源于另一个代码基，与当前 tier1 辅助函数接口存在漂移。
- 后续步骤：
  - 将 `CUtlBuffer::GetString` 调用改为当前仓库接口所支持的形式。
  - 用仓库现有的字符串/ctype 包装函数替换直接或过时的字符判断用法。

### 4. 多个工具目标缺少平台库或外部依赖
- 状态：未解决
- 优先级：高
- 目标目录：
  - [src/utils/binlaunch](src/utils/binlaunch)
  - [src/utils/vfont](src/utils/vfont)
  - [src/utils/phonemeextractor](src/utils/phonemeextractor)
- 现象：
  - `binlaunch.exe` 缺少 `Plat_IsInDebugSession`、`Plat_ExitProcess`、`WriteMiniDump`、`g_pMemAlloc` 等符号。
  - `vfont.exe` 和 `vfont_decompiler.exe` 缺少日志系统、平台接口、内存分配相关符号。
  - `phonemeextractor` 无法打开 `sapi.lib`，`phonemeextractor_ims.cpp` 无法包含 `ims_helper/ims_helper.h`。
- 可能原因：
  - 目标未正确链接 tier0/vstdlib 或崩溃处理相关库。
  - 这些工具依赖当前仓库中未完整提供的外部 SDK 或第三方组件。
- 后续步骤：
  - 对比原始工程文件和当前 CMake 目标的链接库差异。
  - 对缺失外部依赖的工具增加可选门控，而不是默认强制参与完整构建。
  - 如果这些工具仍需要保留，补齐或文档化其语音/IMS 依赖。

### 5. MFC 相关工具的门控还不完整
- 状态：未解决
- 优先级：中
- 目标目录：
  - [src/utils/hlfaceposer](src/utils/hlfaceposer)
  - [src/hammer](src/hammer)
  - [src/utils/FileSystemOpenDialog](src/utils/FileSystemOpenDialog)
- 现象：
  - 配置阶段已经检测到 `hammer_dll` 和 `FileSystemOpenDialog` 因未安装 MFC 被跳过。
  - 但完整构建仍会继续进入 `hlfaceposer.rc`，并因缺少 `afxres.h` 报 `RC1015`。
- 可能原因：
  - `hlfaceposer` 没有使用与其他 MFC 目标一致的能力检测和跳过逻辑。
- 后续步骤：
  - 对所有依赖 MFC 的目标统一使用同一套 CMake 检测和跳过逻辑。
  - 在仓库中明确记录需要安装的 Visual Studio MFC 组件名称。

### 6. VScript 各语言后端实现不完整或接口已漂移
- 状态：未解决
- 优先级：中
- 目标目录：
  - [src/vscript/languages/squirrel/vsquirrel](src/vscript/languages/squirrel/vsquirrel)
  - [src/vscript/languages/python/vpython](src/vscript/languages/python/vpython)
  - [src/vscript/languages/gm/vgm](src/vscript/languages/gm/vgm)
  - [src/vscript/languages/lua/vlua](src/vscript/languages/lua/vlua)
- 现象：
  - `vsquirrel.cpp` 找不到 `init_nut.h`。
  - `vpython.cpp` 找不到 `Python.h`。
  - `vgm.cpp` 在第 744-745 行附近有语法错误。
  - `vlua.cpp` 有多处 `IScriptVM::SetValue` 重载不匹配，以及 `SetErrorCallback` 签名不匹配。
- 可能原因：
  - 部分脚本语言运行时依赖的头文件或生成文件未纳入当前仓库。
  - 脚本 VM 接口已经变化，但后端实现未同步更新。
- 后续步骤：
  - 明确哪些脚本后端应保留为可选目标，并对可选后端增加配置门控。
  - 修复计划保留后端的接口适配问题。
  - 为 Python、Squirrel 等后端增加配置阶段依赖检测。

### 7. `vgui_perftest` 缺少必要头文件依赖
- 状态：未解决
- 优先级：低
- 目标文件：[src/vgui2/vgui_perftest/vgui_perftest.cpp](src/vgui2/vgui_perftest/vgui_perftest.cpp)
- 现象：
  - `vgui_perftest.cpp(38)` 无法包含 `console_logging.h`（`C1083`）。
- 可能原因：
  - include 路径丢失，或头文件路径在重构后发生变化。
- 后续步骤：
  - 查找目标头文件位置，恢复正确的 include 目录或更新 include 语句。

### 8. `temp_build_log.log` 是当前构建错误的主跟踪日志
- 状态：持续跟踪
- 优先级：低
- 目标文件：[build/temp_build_log.log](build/temp_build_log.log)
- 现象：
  - 该日志记录了通过 [CmakeBuildSolution.bat](CmakeBuildSolution.bat) 触发的最新完整构建错误，包括 `client_csgo`、`togl_togl`、`tracker_AdminServer`、多个工具目标和 VScript 后端的失败信息。
- 后续步骤：
  - 当构建行为发生变化时，优先重新生成这份日志，再更新本计划中的问题状态。
  - 后续调整优先级或关闭问题时，尽量引用这份日志中的最新错误作为依据。

## DCC / Maya 插件计划

### 9. 为 Maya 2022.5 开发 DMX 导入导出插件
- 状态：进行中（基础回归全通过；Ellis/DMX 项目级回归已运行，发现两类新问题）
- 优先级：中
- 最新回归结果（2026-04-09）：
  - 基础样例（6/6）：`simple_hierarchy` ✅ `simple_blendshape` ✅ `simple_mesh` ✅ `simple_skinned_mesh` ✅ `complex_chr_mesh` ✅ `MostComplexSampleSet/chr_mesh` ✅
  - Ellis/DMX 项目级（2/9）：
    - `head_morphs_sfm.dmx` ✅（keyvalues2，无蒙皮）
    - `mechanic_morphs_sfm.dmx` ✅（keyvalues2，无蒙皮）
    - `upper_teeth.dmx` ❌ binary v3 解析失败（"unexpected end of string table"）
    - `lower_teeth.dmx` ❌ binary v3 解析失败
    - `teeth_sfm.dmx` ❌ binary v4 解析失败（"element dictionary referenced an invalid string"）
    - `mechanic_model.dmx` ❌ binary v4 解析失败
    - `head_morphs.dmx` ❌ binary v4 解析失败
    - `mechanic_model_merged.dmx` ❌ 网格拓扑不匹配（poly 2247: quad→tri，含 n-gon）
    - `head_morphs_game.dmx` ❌ 网格拓扑不匹配（poly 2247: quad→tri，含 n-gon）
- 目标目录：[dcc_plugin](dcc_plugin)
- 开发环境：
  - Maya 2022.5 DevKit：`D:\_Code_Here\Maya\Autodesk_Maya_2022_5_Update_DEVKIT_Windows\devkitBase`
  - Maya 2022.5 安装目录：`C:\Program Files\Autodesk\Maya2022`
  - Maya 默认宿主执行入口：`C:\Program Files\Autodesk\Maya2022\`
  - 插件独立构建目录：`build\maya_dmx`
  - 最终项目级回归样本目录：`D:\_Code_Here\Git\csgo-src\dcc_plugin\samples\Ellis\DMX`

- 当前概况：
  - 构建、部署与 Maya module 安装链路已稳定，`cmake --build build\maya_dmx --config Release` 可生成 [maya_dmx.mll](dcc_plugin/bin/Release/maya_dmx.mll)，[InstallPluginModuleToMaya.bat](dcc_plugin/InstallPluginModuleToMaya.bat) 负责同步 `.mll`、`.pdb` 与 MEL 脚本。
  - MEL 真源已统一到 [src/mel](dcc_plugin/src/mel)，`maya_module/scripts` 仅作为安装产物。
  - 实现约束：避免通过拼装 MEL 字串实现核心功能，优先使用 Maya C++ API；只有 file type specific options、option box 或 Maya 原生脚本入口确实要求 MEL 时，才保留最小必要脚本桥接。
  - 插件主线已形成“DMX 基础层 + importer/exporter + Maya UI/workflow + batch 回归 + 交互宿主验证”的最小闭环，但复杂角色样例 roundtrip 和完整 facial/animation/export 仍未收口。

- 已完成能力：
  - DMX 基础层：
    - [SimpleDmxDocument.h](dcc_plugin/src/common/SimpleDmxDocument.h)、[SimpleDmxText.cpp](dcc_plugin/src/common/SimpleDmxText.cpp)、[SimpleDmxBinary.cpp](dcc_plugin/src/common/SimpleDmxBinary.cpp)、[SimpleDmxWrite.cpp](dcc_plugin/src/common/SimpleDmxWrite.cpp) 已组成插件自用的最小 DOM/codec/write 层。
    - [SimpleDmxTypes.h](dcc_plugin/src/common/SimpleDmxTypes.h) 与 [SimpleDmxTypes.cpp](dcc_plugin/src/common/SimpleDmxTypes.cpp) 已统一 declared type、binary type code 和 Valve 风格 canonical type name。
    - 已支持 `time/color/qangle/matrix/binary` 及其数组类型；[simple_extended_types.dmx](dcc_plugin/samples/simple_extended_types.dmx) 已通过 text -> binary -> text roundtrip。
    - 已支持 unknown field 与属性顺序的 text 路径保真；[simple_unknown_order.dmx](dcc_plugin/samples/simple_unknown_order.dmx) 已验证未知标量、未知数组、inline element 和原始属性顺序可保留。
  - importer / exporter：
    - [DmxImportTranslator.cpp](dcc_plugin/src/importer/DmxImportTranslator.cpp) 已支持文本 DMX、插件最小二进制 DMX、层级、transform、joint、静态网格、skinCluster、deltaStates、多 UV、face-vertex normals、切线缓存、face set 材质恢复、按 `mayaBlendShapeNode` 分组的 blendShape 重建。
    - importer 已收紧 joint 判定、根节点 `upAxis` 校正、两阶段 DAG/mesh 构建、单 influence 蒙皮恢复、`jointIndices` 子集筛 influence、临时 `maxInfluences` 与 `setWeights()` 时序控制。
    - [DmxExportTranslator.cpp](dcc_plugin/src/exporter/DmxExportTranslator.cpp) 已支持 DAG/joint/mesh/skin/blendShape 的最小导出链路，包含 `bindState/baseStates/currentState`、`jointList/jointWeights/jointIndices`、`deltaStates`、`faceSets`、`texcoord$N`、切线与基础 Maya metadata。
    - exporter 已接入 `exportMetadata` 选项；关闭后会裁掉 `maya*` 重建提示和材质 inline metadata，但保留 mesh/skin/delta 核心数据。
    - 最小 `skin + deltaStates` 组合样例 [simple_skinned_blendshape.dmx](dcc_plugin/samples/simple_skinned_blendshape.dmx) 已通过 text/binary `导入 -> Maya 导出 -> 再导入` 的 `mesh + type + skin + blendShape` 四重回归。
  - Maya UI / workflow：
    - [PluginMain.cpp](dcc_plugin/src/plugin/PluginMain.cpp) 已为 `Valve DMX Import` / `Valve DMX Export` 注册 file type specific options script。
    - 导入端 [performDmxImport.mel](dcc_plugin/src/mel/performDmxImport.mel)、[doDmxImportArgList.mel](dcc_plugin/src/mel/doDmxImportArgList.mel)、[mayaDmxTranslatorImport.mel](dcc_plugin/src/mel/mayaDmxTranslatorImport.mel) 与 [DmxCreateUI.mel](dcc_plugin/src/mel/DmxCreateUI.mel) 已接成和导出端同构的 option box 流程。
    - 导出端 [performDmxExport.mel](dcc_plugin/src/mel/performDmxExport.mel)、[doDmxExportArgList.mel](dcc_plugin/src/mel/doDmxExportArgList.mel) 与 [DmxCreateUI.mel](dcc_plugin/src/mel/DmxCreateUI.mel) 已整理成 Alembic 风格 `OptionsUI / Init / Commit / Perform / ArgList` 结构。
    - [MayaDmxWorkflow.cpp](dcc_plugin/src/workflow/MayaDmxWorkflow.cpp) 与 [MayaDmxWorkflowCommand.cpp](dcc_plugin/src/workflow/MayaDmxWorkflowCommand.cpp) 已支持导出预设、batch manifest 的保存/加载/列出/删除/运行；batch manifest 已迁移到 Maya 用户目录下的 `maya_dmx_workflow/*.batch` 文件。
  - 验证与样例：
    - [RunMayaInteractiveValidation.bat](dcc_plugin/RunMayaInteractiveValidation.bat) 与 [MayaInteractiveValidation.py](dcc_plugin/tools/MayaInteractiveValidation.py) 已固定交互宿主验证入口；已人工确认验证窗口和导入/导出 option box 可正常打开。
    - [MayaBatchRegression.py](dcc_plugin/tools/MayaBatchRegression.py) 已支持 mesh diff、`transform/joint` 类型稳定检查、`skinCluster` 保留检查、`blendShape` 快照检查、动画连接/关键帧快照检查，并可直接处理 `.dmx/.dmxb/.dmxbin`。
    - `simple_hierarchy`、`simple_blendshape`、`simple_mesh`、`simple_skinned_mesh`、`complex_chr_mesh`、`MostComplexSampleSet/chr_mesh` 六组样例已通过基础 `导入 -> 导出 text/binary -> 再导入 -> mesh diff` 闭环；`type_stability_check` 也已在真实 Maya 宿主下跑通。
    - [dcc_plugin/samples/Ellis/DMX](dcc_plugin/samples/Ellis/DMX) 目录下已确认存在实际项目级 DMX 资产，如 [mechanic_model.dmx](dcc_plugin/samples/Ellis/DMX/mechanic_model.dmx)、[mechanic_model_merged.dmx](dcc_plugin/samples/Ellis/DMX/mechanic_model_merged.dmx)、[head_morphs.dmx](dcc_plugin/samples/Ellis/DMX/head_morphs.dmx) 及 `animation/` 子目录；这批样本后续作为最终回归测试集，而不是日常最小样例。
  - 动画与 facial 最小支持：
    - 已重新观察 Valve datamodel，可确认动画主干包含 `DmeAnimationList / DmeChannelsClip / DmeChannel / DmeTimeFrame / Dme*Log / Dme*LogLayer`。
    - importer 已支持 `DmeVector3Log / DmeQuaternionLog`，可把 `position` / `orientation` 通道写成 Maya `animCurveTL/animCurveTA`；[vcaanim_VertexAnim.dmx](dcc_plugin/samples/MostComplexSampleSet/vcaanim_VertexAnim.dmx) 已验证能导入骨骼平移与旋转关键帧。
    - importer 已支持基础 `DmeFloatLog` 标量动画；[simple_float_animation.dmx](dcc_plugin/samples/simple_float_animation.dmx) 已验证在 `|float_anim_root|float_anim_joint.scaleX` 上生成 3 个关键帧。
    - importer 已支持最小 `flexWeight` facial 通道：同一条 `DmeFloatLog` 现在可同时驱动已导入 `blendShape` 权重与最小 `DmeCombinationOperator` 控制节点属性；[simple_blendshape_animation.dmx](dcc_plugin/samples/simple_blendshape_animation.dmx) 已验证会同时生成 `combinationOperator_controls.smile` 与 `blendshapeAnimMeshShape_blendShape.smile` 两条 3 帧动画曲线。
    - exporter 已开始补最小动画导出。当前 [DmxExportTranslator.cpp](dcc_plugin/src/exporter/DmxExportTranslator.cpp) 已支持导出单个 `animationList`，覆盖 `position`、`orientation`、transform 标量通道以及最小 `flexWeight` 通道；私有 binary serializer 也已补上 `time/time_array`。用 [MayaBatchRegression.py](dcc_plugin/tools/MayaBatchRegression.py) 实测时，[simple_float_animation.dmx](dcc_plugin/samples/simple_float_animation.dmx) 的 text/binary 动画 roundtrip 已通过。
    - translator 结构拆分已开始：已把 exporter 文本 DOM helper 拆到 [DmxExportTextModel.h](dcc_plugin/src/exporter/DmxExportTextModel.h) / [DmxExportTextModel.cpp](dcc_plugin/src/exporter/DmxExportTextModel.cpp)，把 importer 的 DMX 查询、数值解析和命名清洗 helper 拆到 [DmxImportUtils.h](dcc_plugin/src/importer/DmxImportUtils.h) / [DmxImportUtils.cpp](dcc_plugin/src/importer/DmxImportUtils.cpp)，为后续继续拆 animation / mesh / material / skin 逻辑打基础。
    - 第二轮 translator 拆分已把 animation 相关 context/option 结构与 helper 群从主文件移出：新增 [DmxExportTranslatorTypes.h](dcc_plugin/src/exporter/DmxExportTranslatorTypes.h)、[DmxExportAnimation.hpp](dcc_plugin/src/exporter/DmxExportAnimation.hpp)、[DmxImportTranslatorTypes.h](dcc_plugin/src/importer/DmxImportTranslatorTypes.h)、[DmxImportAnimation.hpp](dcc_plugin/src/importer/DmxImportAnimation.hpp)，当前主 translator 已明显收缩，动画导入导出逻辑可在不触碰 mesh/skin 主流程的前提下继续迭代。
    - 第三轮 translator 拆分已把 deformer 相关 helper 从主文件移出：新增 [DmxExportDeformers.hpp](dcc_plugin/src/exporter/DmxExportDeformers.hpp) 与 [DmxImportDeformers.hpp](dcc_plugin/src/importer/DmxImportDeformers.hpp)，集中收纳 `skinCluster`、`deltaStates`、`blendShape` 恢复/导出相关逻辑；当前 [DmxExportTranslator.cpp](dcc_plugin/src/exporter/DmxExportTranslator.cpp) 与 [DmxImportTranslator.cpp](dcc_plugin/src/importer/DmxImportTranslator.cpp) 已主要保留主流程与 mesh/material 逻辑，复杂样例相关 deformer 代码可以在更小的边界内继续调试。
    - 第四轮 translator 拆分已开始把 mesh/material helper 从主文件移出：新增 [DmxExportMeshMaterial.hpp](dcc_plugin/src/exporter/DmxExportMeshMaterial.hpp) 与 [DmxImportMeshMaterial.hpp](dcc_plugin/src/importer/DmxImportMeshMaterial.hpp)，exporter 侧已收拢 face set 与基础材质提取逻辑，importer 侧已收拢 UV 集解析、mesh vertexData 选择与 face set 材质恢复逻辑；`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过，后续可继续把 `CreateMeshShape()` 等更大的 mesh 主路径从 translator 主文件中移出。

- 当前能力边界：
  - importer / exporter 已覆盖的核心能力：
    - 层级、transform、joint、静态网格、蒙皮、deltaStates。
    - 主 UV 与额外 UV set、face-vertex normals、切线缓存。
    - `bindState/baseStates/currentState` 三态 mesh state。
    - 基础 face set 材质槽位与最小 shader/贴图元数据。
    - 按节点分组的 blendShape 重建与基础 deformer 参数恢复。
    - 导入端 file type specific options：`importMaterials`、`importSkin`、`importDeltaStates`。
    - 导出端 file type specific options / workflow options：`encoding`、`upAxis`、`exportSkin`、`exportDeltaStates`、`exportMetadata`、`materialRoot`。
  - workflow 已覆盖的能力：
    - 单次导出预设保存/读取与命令触发导出。
    - batch manifest 的文件化持久化与批量导出执行。
    - Maya 文件对话框下方 file type specific options 的导入/导出入口。
  - 当前未覆盖或仅做最小保真的部分：
    - 完整材质网络，仅支持基础 lambert/同型 shader、颜色、透明度、diffuse/normal/bump。
    - 组合型面部控制器、`DmeCombinationOperator`、更高层 facial rig / control 网络。
    - 完整 deformer 栈、约束、历史链和更高层动画/rig 元数据。
    - 更大范围的 Valve 历史 DMX 兼容、未知字段保真、通用 DOM/codec。
    - UFE、USD、代理节点等非 DAG 混合场景对象的完整支持。

- 已修复问题（2026-04-09 本轮完成）：
  - **Maya 2022 API 兼容性**：`getAlias` → `plugsAlias`；`MFn::kSkinCluster` → `MFn::kSkinClusterFilter`；补充 `#include <maya/MDoubleArray.h>`；`setAlias` 参数类型错误修正。受影响文件：`DmxExportAnimation.cpp`、`DmxExportDeformers.cpp`、`DmxImportDeformers.cpp`。
  - **MDGModifier 崩溃**：将 blendShape target 临时节点删除从 `MDGModifier::deleteNode`（析构时触发回调崩溃）回退为 MEL `delete` 命令。受影响文件：`DmxExportDeformers.cpp`、`DmxImportDeformers.cpp`。
  - **二进制 DMX 导出**：`CollectReachableElements` 跳过 inline 元素导致 `WriteElementRef` 无法解析索引，已移除 inline 判断，遍历所有元素。受影响文件：`SimpleDmxWrite.cpp`。
  - **坐标系修正顺序**：Z-up 导入时轴修正旋转被 `ApplyTransform` 覆盖，改为将轴修正置于 `ApplyTransform` 之后执行。受影响文件：`DmxImportTranslator.cpp`。
  - **Bind Shape 顶点位置**：exporter 从 output mesh（变形后）取顶点位置，改为优先从 intermediate mesh（bind shape）取位置。受影响文件：`DmxExportDag.cpp`、`DmxExportMesh.cpp`。
  - **bindPreMatrix 顺序错位**：存储的 `mayaBindPreMatrix` 按 Maya 导出顺序排列，而导入时按 DMX 关节索引顺序分配，导致矩阵错位。改为按关节名称路径匹配查找对应矩阵。受影响文件：`DmxImportDeformers.cpp`。
  - **零权重填充索引污染**：导出时用 `{0, 0.0}` 填充空槽，导致关节 0 被误判为活跃影响，改为 `{-1, 0.0}`（负数索引由导入器忽略）。受影响文件：`DmxExportDeformers.cpp`。

- 已确认剩余问题：
  - 材质网络恢复深度仍不足。`AssignFaceSetMaterials()` 虽已改为 API 实现，但当前仍只覆盖基础 shader 图，尚未补 `place2dTexture`、utility 链、分层材质以及更完整的 Valve/Maya 材质语义映射。
  - `exportMetadata=0` 当前只裁掉 Maya 专用 metadata 和材质 inline metadata，不会改写核心 mesh/skin/delta 数据；如果后续希望进一步削减文件大小，还需要继续评估哪些非 `maya*` 字段也可选裁剪而不破坏回读。
  - `SimpleDmx*` 仍是插件定制层，不是通用 DMX DOM/codec。继续扩大 Valve DMX 兼容范围时，未知字段保真、顺序保真和引用语义都会成为重构阻力。
  - `SimpleDmx*` 虽然已经拆出公共 DOM 和 type 映射，exporter 侧 type code 也已切到公共映射，但 [DmxExportTranslator.cpp](dcc_plugin/src/exporter/DmxExportTranslator.cpp) 仍保留一套独立的 binary serializer 结构；如果不继续把这层往公共 codec 收，后续补 unknown field / 顺序保真时仍会存在实现分叉。
  - 当前 `SimpleDmx*` 已能在 text 路径上保留 unknown field 与属性顺序，但“未知 declared type 的 text -> binary” 仍不成立：例如 [simple_unknown_order.dmx](dcc_plugin/samples/simple_unknown_order.dmx) 中的 `mystery_type` / `mystery_array` 目前仍会在 binary 写出阶段被拒绝，因为标准 binary DMX 没有可直接承载这类未知自定义类型名的 type code。
  - 未知 declared type 的 binary 保真目前不存在低成本“原样写回”方案。根据 [dmattributetypes.h](src/public/datamodel/dmattributetypes.h) 的 `DmAttributeType_t`，binary DMX 只有固定 type code 集，没有用于任意自定义 declared type 名的扩展槽位；因此现阶段如果继续要求 Valve 兼容的 binary DMX，未知类型只能选择显式报错、导出时强制回退 text、或引入插件私有旁带保真方案，不能像 text DMX 那样无损直接写出。
  - [vcaanim_VertexAnim.dmx](dcc_plugin/samples/MostComplexSampleSet/vcaanim_VertexAnim.dmx) 并不是“带蒙皮和 BlendShape 的模型样例”，而是纯 `skeleton + channels/logs` 动画样例；当前它更适合作为动画支持回归入口，而不是 `skin + blendShape` 组合回归入口。
  - DMX 动画当前仍只覆盖 importer；虽然 `position/orientation`、基础 `DmeFloatLog`、最小 `flexWeight -> blendShape weight` 以及最小 `DmeCombinationOperator` 控制器属性都已能导入成 Maya 关键帧，但 controls/controlValues 到更高层 facial rig 的完整驱动语义、`DmeAnimationList` 导出、动画 roundtrip 回归和更高层 clip / sequence 语义都还没有落地。
  - 动画 roundtrip 刚开始形成闭环，但 facial 组合样例还没完全稳定。当前 [simple_float_animation.dmx](dcc_plugin/samples/simple_float_animation.dmx) 的 text/binary 动画回归已通过；而 [simple_blendshape_animation.dmx](dcc_plugin/samples/simple_blendshape_animation.dmx) 现在已经不再失败于“动画丢失”，而是前移成 text roundtrip 后 `blendshapeAnimMeshShape at vertex 2` 的 mesh point diff，说明 exporter/importer 的最小 facial 动画链已接通，但 blendShape 几何保真还要继续收口。
  - “蒙皮 + blendShape 同时存在”的最小闭环问题已完成一轮收敛。当前 [DmxImportTranslator.cpp](dcc_plugin/src/importer/DmxImportTranslator.cpp) 已改成先 `ApplySkinning()` 再 `ApplyDeltaStates()`，修复了 `skinCluster API creation failed`；[DmxExportTranslator.cpp](dcc_plugin/src/exporter/DmxExportTranslator.cpp) 也已在 `blendShapeFn.getTargets()` 拿不到 live target DAG 时，通过 `sculptTarget -regenerate` 临时重建 target 再提取 `DmeVertexDeltaData`，因此最小 `skin + deltaStates` 样例的 text/binary roundtrip 已能保住 `blendShape`。
  - 旧版错误编码产生的历史 batch 文件如果残留在 Maya 用户目录里，`listBatches` 仍可能显示脏名称；新格式已可用，但还缺一次性清理或文件头校验。

- 已确认的新问题（Ellis/DMX 回归，2026-04-09）：
  - **Binary v3/v4 解析不兼容**：
    - 现象：插件 parser 假设 v5 风格（先 int32 string count → string table → element dict 用索引引用字符串），但 v3/v4 在 element dict 内联字符串，无独立 string table。
    - v3 错误路径：读 int16 element count（如 `0x004e = 78`），解析器当成 int32 string count，尝试读 78 个字符串条目，遇到非字符串数据 → “unexpected end of string table”。
    - v4 错误路径：读 int32（251）作为 string count，读 251 条内联字符串成功，再尝试解析 element dict 时字符串索引越界 → “element dictionary referenced an invalid string”。
    - 修复难度：**高**。需要在 `SimpleDmxBinary.cpp` 按 encoding version 分叉解析路径；v3/v4 的 element dict 格式与 v5 差异显著，需要单独实现并做好单元测试。
    - 影响文件：`upper_teeth.dmx`、`lower_teeth.dmx`（v3）；`teeth_sfm.dmx`、`mechanic_model.dmx`、`head_morphs.dmx`（v4）
  - **含 n-gon 网格拓扑不匹配**：
    - 现象：keyvalues2 格式的生产级模型（含 5 顶点 n-gon polygon），roundtrip 后 poly 2247 的顶点数从 4 变为 3（quad → tri）。
    - 已确认：原始 poly 2255 为 5-verts n-gon；poly 2247 原为 quad `[7,6,10,12]`，roundtrip 后顶点数变为 3。
    - 怀疑方向：exporter 端 `MItMeshPolygon` 导出混合多边形时可能发生隐式三角化或面集合分割；或 importer 端面集恢复时 face count 误差导致拓扑移位。
    - 修复难度：**中**。需要在 exporter 端加 n-gon 覆盖测试样例，逐步定位 `polygonFaceIndices` 中的错误条目来源。
    - 影响文件：`mechanic_model_merged.dmx`、`head_morphs_game.dmx`

- 下一阶段计划：
  - 第一优先级：
    - ✅ ~~`complex_chr_mesh` 与 `MostComplexSampleSet/chr_mesh` 顶点漂移~~（已修复，2026-04-09）
    - ✅ ~~把 Ellis/DMX 纳入回归门槛并运行~~（已运行，2026-04-09，结果见上）
    - **修复含 n-gon 的网格拓扑不匹配**（影响两个 keyvalues2 生产模型）：
      - 先制作最小 n-gon 复现样例（含 5-verts polygon 的简单 Maya 网格）
      - 在 exporter 端打印 `polygonFaceIndices` 调试信息，定位 quad→tri 发生的具体位置
      - 修复后补回归用例，确保混合三角/四边/n-gon 网格 roundtrip 不改变拓扑
    - 补独立动画宿主回归，避免后续改 importer 时 [vcaanim_VertexAnim.dmx](dcc_plugin/samples/MostComplexSampleSet/vcaanim_VertexAnim.dmx)、[simple_float_animation.dmx](dcc_plugin/samples/simple_float_animation.dmx)、[simple_blendshape_animation.dmx](dcc_plugin/samples/simple_blendshape_animation.dmx) 退回”只导骨架不导关键帧”。
  - 第二优先级（技术债务，修复 n-gon 问题后再做）：
    - **修复 binary v3/v4 解析**：
      - 在 `SimpleDmxBinary.cpp` 按 `version` 分叉 `ParseDocument` 逻辑
      - v3：element count 为 int16，element dict 内联字符串，无 string table
      - v4：element count 为 int32，element dict 内联字符串，无独立 string table
      - 修复后可覆盖 `upper_teeth`、`lower_teeth`（v3）和 `teeth_sfm`、`mechanic_model`、`head_morphs`（v4）
    - 继续补材质网络，扩到 `place2dTexture`、utility 节点链、更多 shader 类型和更稳定的贴图路径还原。
    - 继续补组合型面部控制器和更完整 deformer / rig 元数据，逐步接近 Valve 角色资产的面部工作流。
    - 为 workflow 增加旧 batch 文件清理、格式校验和更明确的错误提示。
  - 中期重构方向：
    - **代码规范约束（强制执行）**：
      - 禁止使用 `.hpp` 文件；所有声明放 `.h`，所有实现放 `.cpp`，不得保留或新增 `.hpp`。
      - 禁止匿名 namespace（`namespace { ... }`）；需要限制符号可见性时，改用具名 namespace 或 `static` 修饰符。
    - **`.hpp` 拆分与 common 头文件整理**：
      - 将现有 `.hpp`（[DmxExportAnimation.hpp](dcc_plugin/src/exporter/DmxExportAnimation.hpp)、[DmxExportDeformers.hpp](dcc_plugin/src/exporter/DmxExportDeformers.hpp)、[DmxExportMeshMaterial.hpp](dcc_plugin/src/exporter/DmxExportMeshMaterial.hpp)、[DmxImportAnimation.hpp](dcc_plugin/src/importer/DmxImportAnimation.hpp)、[DmxImportDeformers.hpp](dcc_plugin/src/importer/DmxImportDeformers.hpp)、[DmxImportMeshMaterial.hpp](dcc_plugin/src/importer/DmxImportMeshMaterial.hpp)）各自拆分为同名 `.h` + `.cpp`。
      - 识别 importer/exporter 双侧共用的类型、工具函数和辅助结构，统一移入 [dcc_plugin/src/common](dcc_plugin/src/common)，避免跨侧重复声明。
      - 完成后同步更新 CMakeLists.txt，确保新增 `.cpp` 文件纳入编译。
    - 继续拆分 [DmxExportTranslator.cpp](dcc_plugin/src/exporter/DmxExportTranslator.cpp) 与 [DmxImportTranslator.cpp](dcc_plugin/src/importer/DmxImportTranslator.cpp) 中 namespace 里的数据结构和方法，优先抽离 animation、mesh、material、skin 等子模块到更小的 `.h/.cpp` 文件，减少单文件耦合并提高复用性。
    - 在 [SimpleDmxDocument.h](dcc_plugin/src/common/SimpleDmxDocument.h) 与 [SimpleDmxTypes.h](dcc_plugin/src/common/SimpleDmxTypes.h) 这一层稳定后，继续把 `SimpleDmx*` 从插件定制实现逐步拆成更通用的 DOM/codec 层，优先补完整 attribute type、未知字段保真和 text/binary 对称。
    - 在 text 路径的 unknown field / 顺序保真已落地后，继续评估未知 declared type 的 text -> binary 降级策略、旁带保真或显式能力边界，避免 binary exporter 对自定义类型直接硬失败。
    - 在 type code 已统一后，继续把 exporter 私有 serializer 往公共 codec 收，减少 `SimpleDmx*` 与 [DmxExportTranslator.cpp](dcc_plugin/src/exporter/DmxExportTranslator.cpp) 的双份维护。
    - 为动画 DMX 单独补 importer/exporter 路线图，优先支持 `DmeAnimationList / DmeChannelsClip / DmeChannel / DmeTimeFrame / DmeVector3Log / DmeQuaternionLog` 这条骨骼变换动画主干，并明确它与现有静态 mesh / skin translator 的关系。
    - 在最小骨骼变换动画 importer 已落地后，继续评估 `DmeFloatLog`、vertex animation list、flex / facial animation 和动画导出，确认是沿现有 translator 扩展还是单独拆动画工作流。
    - 在最小 `skin + deltaStates` 样例已通过后，继续找更接近 Valve 角色资产的复合样例，确认 `sculptTarget -regenerate` 这条 fallback 在复杂 blendShape / 多 target / 多 mesh history 下是否仍稳定。
    - 在通用 DMX 层稳定后，再扩主干类型覆盖范围，最后再评估更大范围的 Valve DMX rig / animation 兼容。

## 环境与工具链说明

### A. 批处理构建包装脚本已做兼容性修复
- 状态：已完成
- 结果：
  - [CmakeBuildSolution.bat](CmakeBuildSolution.bat) 现在会在构建前重新执行 CMake 配置，并避免使用当前环境下会导致早退的 `--parallel` 调用方式。

### B. 已接入 protobuf 源码构建目标
- 状态：已完成
- 结果：
  - 已新增 [src/thirdparty/protobuf-2.5.0/CMakeLists.txt](src/thirdparty/protobuf-2.5.0/CMakeLists.txt)，从仓库内 protobuf 2.5.0 源码直接构建 `libprotobuf`。
  - 已通过 [src/thirdparty/CMakeLists.txt](src/thirdparty/CMakeLists.txt) 注册，并在 [src/CMakeLists.txt](src/CMakeLists.txt) 中提前 thirdparty 注册顺序，使 `prebuilt::libprotobuf` 在消费者配置前优先绑定到源码构建目标。
  - 已通过 `cmake --build build --config Release --target valve_libprotobuf` 验证该目标可以在 VS2022/x86 下独立构建，并输出 `src/lib/public/x86/libprotobuf.lib`。
  - 已为 protobuf 2.5.0 增加 `_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS`，解决现代 MSVC 对 `<hash_map>` 的报错。

### C. 配置阶段发现的可选依赖现状
- 状态：未解决
- 说明：
  - 当前 x86 工具链下未检测到可用 Autodesk FBX SDK，因此自动回退到 OpenFBX。
  - `utils/studiomdl` 缺少 `libedgegeomtool`，当前会以无 edgegeom 支持模式构建。
  - 当前机器/工具链未安装 MFC，因此影响若干工具目标，详见上面的 MFC 相关问题。
