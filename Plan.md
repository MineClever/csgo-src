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
- 状态：进行中（基础模型回归已稳定；动画导入导出已具备最小闭环；剩余工作集中在动画宿主回归、材质网络、facial/rig 语义与通用层收口）
- 优先级：中
- 最新回归结果（2026-04-09，2026-04-11 代码复核通过）：
  - 基础样例（6/6）：`simple_hierarchy` ✅ `simple_blendshape` ✅ `simple_mesh` ✅ `simple_skinned_mesh` ✅ `complex_chr_mesh` ✅ `MostComplexSampleSet/chr_mesh` ✅
  - Ellis/DMX 核心模型样例（9/9）：
    - `head_morphs_sfm.dmx` ✅（keyvalues2，无蒙皮）
    - `mechanic_morphs_sfm.dmx` ✅（keyvalues2，无蒙皮）
    - `upper_teeth.dmx` ✅（binary v3，修复后通过）
    - `lower_teeth.dmx` ✅（binary v3，修复后通过）
    - `teeth_sfm.dmx` ✅（binary v4，修复后通过）
    - `mechanic_model.dmx` ✅（binary v4，修复后通过，含 applyAxisCorrection=0 变体）
    - `head_morphs.dmx` ✅（binary v4，修复后通过）
    - `mechanic_model_merged.dmx` ✅（mesh/skin/blendshape 全 ok，含 applyAxisCorrection=0 变体）
    - `head_morphs_game.dmx` ✅（mesh/skin/blendshape 全 ok）
- 目标目录：[dcc_plugin](dcc_plugin)
- 开发环境：
  - Maya 2022.5 DevKit：`D:\_Code_Here\Maya\Autodesk_Maya_2022_5_Update_DEVKIT_Windows\devkitBase`
  - Maya 2022.5 安装目录：`C:\Program Files\Autodesk\Maya2022`
  - Maya 默认宿主执行入口：`C:\Program Files\Autodesk\Maya2022\`
  - 插件独立构建目录：`build\maya_dmx`
  - 最终项目级回归样本目录：`D:\_Code_Here\Git\csgo-src\dcc_plugin\samples\Ellis\DMX`
  - 宿主环境查询入口：[QueryMayaValidationEnv.bat](dcc_plugin/QueryMayaValidationEnv.bat)、[QueryMayaValidationEnv.ps1](dcc_plugin/tools/QueryMayaValidationEnv.ps1)
  - 宿主环境说明文档：[MayaValidationEnv.md](dcc_plugin/docs/MayaValidationEnv.md)

- 当前概况：
  - 构建、部署与 Maya module 安装链路已稳定，`cmake --build build\maya_dmx --config Release` 可生成 [maya_dmx.mll](dcc_plugin/bin/Release/maya_dmx.mll)，[InstallPluginModuleToMaya.bat](dcc_plugin/InstallPluginModuleToMaya.bat) 负责同步 `.mll`、`.pdb` 与 MEL 脚本。
  - MEL 真源已统一到 [src/mel](dcc_plugin/src/mel)，`maya_module/scripts` 仅作为安装产物。
  - 实现约束：避免通过拼装 MEL 字串实现核心功能，优先使用 Maya C++ API；只有 file type specific options、option box 或 Maya 原生脚本入口确实要求 MEL 时，才保留最小必要脚本桥接。
  - 插件主线已形成“DMX 基础层 + importer/exporter + Maya UI/workflow + batch 回归 + 交互宿主验证”的最小闭环，但复杂角色样例 roundtrip 和完整 facial/animation/export 仍未收口。
  - 2026-04-11 已通过宿主环境查询脚本确认当前机器具备 Maya 2022、mayapy、DevKit、插件二进制、MEL 脚本和回归入口的基础路径条件；查询报告输出到 `build\maya_dmx\maya_validation_env_report.md`。

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
    - 2026-04-11 使用 `MAYA_SKIP_USERSETUP_PY=1` 的干净宿主环境重新运行整套批回归，`simple_hierarchy`、`simple_blendshape`、`simple_mesh`、`simple_skinned_mesh`（含 `applyAxisCorrection=0`）、`complex_chr_mesh`（含 `applyAxisCorrection=0`）、`MostComplexSampleSet/chr_mesh`（含 `applyAxisCorrection=0`）、`simple_ngon_mesh`、`MostComplexSampleSet/vcaanim_VertexAnim`、`simple_float_animation`、`simple_blendshape_animation` 全部通过。
    - `simple_hierarchy`、`simple_blendshape`、`simple_mesh`、`simple_skinned_mesh`、`complex_chr_mesh`、`MostComplexSampleSet/chr_mesh` 六组样例已通过基础 `导入 -> 导出 text/binary -> 再导入 -> mesh diff` 闭环；`type_stability_check` 也已在真实 Maya 宿主下跑通。
    - [dcc_plugin/samples/Ellis/DMX](dcc_plugin/samples/Ellis/DMX) 目录下已确认存在实际项目级 DMX 资产，如 [mechanic_model.dmx](dcc_plugin/samples/Ellis/DMX/mechanic_model.dmx)、[mechanic_model_merged.dmx](dcc_plugin/samples/Ellis/DMX/mechanic_model_merged.dmx)、[head_morphs.dmx](dcc_plugin/samples/Ellis/DMX/head_morphs.dmx) 及 `animation/` 子目录；这批样本后续作为最终回归测试集，而不是日常最小样例。
  - 动画与 facial 最小支持：
    - 已重新观察 Valve datamodel，可确认动画主干包含 `DmeAnimationList / DmeChannelsClip / DmeChannel / DmeTimeFrame / Dme*Log / Dme*LogLayer`。
    - importer 已支持 `DmeVector3Log / DmeQuaternionLog`，可把 `position` / `orientation` 通道写成 Maya `animCurveTL/animCurveTA`；[vcaanim_VertexAnim.dmx](dcc_plugin/samples/MostComplexSampleSet/vcaanim_VertexAnim.dmx) 已验证能导入骨骼平移与旋转关键帧。
    - importer 已支持基础 `DmeFloatLog` 标量动画；[simple_float_animation.dmx](dcc_plugin/samples/simple_float_animation.dmx) 已验证在 `|float_anim_root|float_anim_joint.scaleX` 上生成 3 个关键帧。
    - importer 已支持最小 `flexWeight` facial 通道：同一条 `DmeFloatLog` 现在可同时驱动已导入 `blendShape` 权重与最小 `DmeCombinationOperator` 控制节点属性；[simple_blendshape_animation.dmx](dcc_plugin/samples/simple_blendshape_animation.dmx) 已验证会同时生成 `combinationOperator_controls.smile` 与 `blendshapeAnimMeshShape_blendShape.smile` 两条 3 帧动画曲线。
    - exporter 已开始补最小动画导出。当前 [DmxExportTranslator.cpp](dcc_plugin/src/exporter/DmxExportTranslator.cpp) 已支持导出单个 `animationList`，覆盖 `position`、`orientation`、transform 标量通道以及最小 `flexWeight` 通道；私有 binary serializer 也已补上 `time/time_array`。用 [MayaBatchRegression.py](dcc_plugin/tools/MayaBatchRegression.py) 实测时，[simple_float_animation.dmx](dcc_plugin/samples/simple_float_animation.dmx) 的 text/binary 动画 roundtrip 已通过。
    - translator 结构拆分已开始：（旧 DmxExportTextModel 已删除，见下）把 importer 的 DMX 查询、数值解析和命名清洗 helper 拆到 [DmxImportUtils.h](dcc_plugin/src/importer/DmxImportUtils.h) / [DmxImportUtils.cpp](dcc_plugin/src/importer/DmxImportUtils.cpp)，为后续继续拆 animation / mesh / material / skin 逻辑打基础。
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

- 代码对照结论（2026-04-11）：
  - `Plan.md` 里先前“DMX 动画当前仍只覆盖 importer”的表述已过时；当前代码已在 [DmxExportAnimation.cpp](dcc_plugin/src/exporter/DmxExportAnimation.cpp) 实现最小 `DmeAnimationList / DmeChannelsClip / DmeChannel / DmeTimeFrame` 导出，`simple_float_animation` 的 text/binary roundtrip 也已在批回归中覆盖。
  - `Plan.md` 里先前“.hpp 拆分待做”的表述已过时；当前 `dcc_plugin/src/importer` 与 `dcc_plugin/src/exporter` 相关拆分文件已经落成 `.h/.cpp` 形式。
  - `Plan.md` 里先前“FindAttribute* / ParseNumberList 并入公共层待做”的表述已过时；当前实现已落在 [SimpleDmxDocument.h](dcc_plugin/src/common/SimpleDmxDocument.h) / [SimpleDmxDocument.cpp](dcc_plugin/src/common/SimpleDmxDocument.cpp)。
  - workflow 侧的“损坏 batch 报错”已部分具备；[MayaDmxWorkflow.cpp](dcc_plugin/src/workflow/MayaDmxWorkflow.cpp) 已对损坏条目、旧 optionVar fallback、文件读写失败做错误返回，但仍缺显式清理/迁移入口。

- 已确认剩余问题：
  - 材质网络恢复深度仍不足。`AssignFaceSetMaterials()` 已走 API 路径，但当前仍只覆盖基础 shader 图，尚未补 `place2dTexture`、utility 链、分层材质以及更完整的 Valve/Maya 材质语义映射。
  - `exportMetadata=0` 当前只裁掉 Maya 专用 metadata 和材质 inline metadata，不会改写核心 mesh/skin/delta 数据；如果后续希望进一步削减文件大小，还需要继续评估哪些非 `maya*` 字段也可选裁剪而不破坏回读。
  - `SimpleDmx*` 仍是插件定制层，不是通用 DMX DOM/codec。继续扩大 Valve DMX 兼容范围时，未知字段保真、顺序保真和引用语义都会成为重构阻力。
  - 当前 `SimpleDmx*` 已能在 text 路径上保留 unknown field 与属性顺序，但“未知 declared type 的 text -> binary” 仍不成立：例如 [simple_unknown_order.dmx](dcc_plugin/samples/simple_unknown_order.dmx) 中的 `mystery_type` / `mystery_array` 目前仍会在 binary 写出阶段被拒绝，因为标准 binary DMX 没有可直接承载这类未知自定义类型名的 type code。
  - 未知 declared type 的 binary 保真目前不存在低成本“原样写回”方案。根据 [dmattributetypes.h](src/public/datamodel/dmattributetypes.h) 的 `DmAttributeType_t`，binary DMX 只有固定 type code 集，没有用于任意自定义 declared type 名的扩展槽位；因此现阶段如果继续要求 Valve 兼容的 binary DMX，未知类型只能选择显式报错、导出时强制回退 text、或引入插件私有旁带保真方案，不能像 text DMX 那样无损直接写出。
  - [vcaanim_VertexAnim.dmx](dcc_plugin/samples/MostComplexSampleSet/vcaanim_VertexAnim.dmx) 仍更适合作为纯骨骼动画回归入口，而不是 `skin + blendShape` 复合样例入口。
  - 动画导入导出虽已形成最小闭环；2026-04-11 已为骨骼动画、标量动画、facial 动画样例补上“初始导入动画门槛”，但复杂动画样例覆盖仍不足，后续仍需继续扩展。
  - facial 动画链路尚未完全收口。当前 [simple_blendshape_animation.dmx](dcc_plugin/samples/simple_blendshape_animation.dmx) 已不再丢动画，但 text roundtrip 后仍存在 `blendshapeAnimMeshShape at vertex 2` 的 mesh point diff，说明最小 `flexWeight -> blendShape` 已接通，但 blendShape 几何保真仍需继续修。
  - 更高层 facial rig 语义尚未落地。当前 importer 只支持最小 `DmeCombinationOperator` 控制器节点与 `controlValues` 绑定，离完整 controls/controlValues 到 rig 控制网络的语义仍有距离。
  - `skin + deltaStates` 最小闭环已经可用，但 `sculptTarget -regenerate` 这条 fallback 还没有在更复杂的多 target / 多 mesh history 样例上完成稳定性确认。
  - workflow 已支持文件化 batch manifest、损坏条目报错、旧 optionVar fallback，以及显式的 legacy batch 迁移/清理入口；剩余工作主要收敛为更细的用户侧诊断体验优化。
  - 2026-04-11 直接裸跑 `mayapy` 时暴露宿主环境存在第三方 `userSetup.py` 干扰（`VaccineKiller.mod` 权限错误）；按当前文档与包装脚本约定，批回归必须保持 `MAYA_SKIP_USERSETUP_PY=1`，后续宿主验证默认沿用该约束。

- 已确认的新问题（Ellis/DMX 回归，2026-04-09）：
  - ~~**Binary v3/v4 解析不兼容**~~：✅ 已修复（2026-04-09）
    - 根本原因：v3 scalar string 值 = 内联 cstring；v4 scalar string 值 = int16 字符串表索引；v5 scalar string 值 = int32 索引；StringArray 在所有版本均已正确使用内联 cstring。
    - 修复：`SimpleDmxBinary.cpp` `ValueType::String` 分支对 `encodingVersion <= 3` 改为 `ReadCString()`；v3 string table 用 int16 count 读取；element dict v3 用 inline cstring name，v4/v5 用索引。
    - 验证：`upper_teeth.dmx`、`lower_teeth.dmx`（v3）、`teeth_sfm.dmx`、`mechanic_model.dmx`、`head_morphs.dmx`（v4）五个文件均通过回归（含 `mechanic_model` 的 applyAxisCorrection=0 变体）。
  - ~~**含 n-gon 网格拓扑不匹配**~~：✅ 已修复（2026-04-09），见第一优先级。

- 下一阶段计划：
  - 第一优先级：
    - ✅ ~~补独立动画宿主回归，至少把 [vcaanim_VertexAnim.dmx](dcc_plugin/samples/MostComplexSampleSet/vcaanim_VertexAnim.dmx)、[simple_float_animation.dmx](dcc_plugin/samples/simple_float_animation.dmx)、[simple_blendshape_animation.dmx](dcc_plugin/samples/simple_blendshape_animation.dmx) 变成单独门槛，避免后续修改退回“只导骨架、不导关键帧”。~~（已完成，2026-04-11）：[MayaBatchRegression.py](dcc_plugin/tools/MayaBatchRegression.py) 新增 `ANIMATION_GATE_EXPECTATIONS` 与 `validate_animation_gate()`，会在样例初始导入后直接校验动画绑定是否存在；[RunMayaBatchRegression.bat](dcc_plugin/RunMayaBatchRegression.bat) 的说明文本也已同步更新。
    - 收口 `simple_blendshape_animation` 的 blendShape 几何 diff，完成最小 facial 动画 roundtrip 稳定化。
    - 选 1 到 2 组更接近 Valve 角色资产的复合样例，验证 `skin + deltaStates + animation` 以及 `sculptTarget -regenerate` fallback 在复杂场景下仍稳定。
  - 第二优先级：
    - 继续补材质网络，扩到 `place2dTexture`、utility 节点链、更多 shader 类型和更稳定的贴图路径还原。
    - 继续补组合型面部控制器和更完整 deformer / rig metadata，逐步接近 Valve 角色资产的 facial 工作流。
    - 继续评估 `exportMetadata=0` 的裁剪边界，明确哪些字段可以在不破坏回读的前提下继续瘦身。
  - 中期重构方向：
    - 继续拆分 [DmxExportTranslator.cpp](dcc_plugin/src/exporter/DmxExportTranslator.cpp) 与 [DmxImportTranslator.cpp](dcc_plugin/src/importer/DmxImportTranslator.cpp) 中仍留在实现文件里的上下文结构、流程拼装和 Maya 侧 helper，优先把 animation、mesh、material、skin 的剩余主路径再收窄。
    - 识别 importer/exporter 双侧共用的类型、工具函数和辅助结构，继续移入 [dcc_plugin/src/common](dcc_plugin/src/common)，减少跨侧重复定义。
    - 在 [SimpleDmxDocument.h](dcc_plugin/src/common/SimpleDmxDocument.h) 与 [SimpleDmxTypes.h](dcc_plugin/src/common/SimpleDmxTypes.h) 这一层稳定后，继续补完整 attribute type、unknown field 保真和 text/binary 对称。
    - 在 text 路径 unknown field / 顺序保真已落地的前提下，继续评估未知 declared type 的 text -> binary 降级策略、旁带保真或显式能力边界，避免 binary exporter 对自定义类型直接硬失败。
    - 为动画 DMX 单独整理 importer/exporter 路线图，明确最小骨骼动画、标量动画、facial 动画和更高层 clip / sequence 语义的边界，不再混写进静态 mesh / skin 主计划。

- importer 并入公共 codec 评估（2026-04-09）：
  - **结论：importer 不存在”私有 DOM”问题，已直接消费 `simple_dmx::Document`，不需要对称的整体迁移。**
  - exporter 迁移的核心价值在于删除了整套私有 `DmxTextBuilder/DmxElement` DOM 并消除 text→parse→binary 中转；importer 从一开始就直接在 `document.Parse()` 后遍历 `simple_dmx::Element*`，没有这层独立 DOM。
  - 2026-04-11 复核：`FindAttributeElement/Array/String/StringArray` 与 `ParseNumberList` 已并入 [SimpleDmxDocument.h](dcc_plugin/src/common/SimpleDmxDocument.h) / [SimpleDmxDocument.cpp](dcc_plugin/src/common/SimpleDmxDocument.cpp)，这一条不再是待办。
  - **不并入**：`SanitizeNodeName`、`SetVector3Plug`、`EnsureDependencyNode` 等 Maya API 调用，以及 `ImportContext/ImportOptions`——这些是插件专用逻辑，与 codec 无关。
  - 任务同步（2026-04-11）：本次已按“剩余开发计划”重新核对 DMX 插件状态；当前未完成主线仍集中在动画宿主回归、材质网络补全、facial/rig 语义扩展、workflow 历史 batch 清理，以及中期的 translator/common 层重构，现有优先级不变。
  - 任务同步（2026-04-11，回归门槛补齐）：DMX 批回归已补上动画专项门槛，当前未完成主线更新为 `simple_blendshape_animation` 几何保真、复杂复合动画样例验证、材质网络补全、facial/rig 语义扩展、workflow 历史 batch 清理，以及中期的 translator/common 层重构。
  - 任务同步（2026-04-11，宿主环境查询与批回归）：已新增宿主环境查询脚本与文档，确认当前机器满足基础验证条件；在 `MAYA_SKIP_USERSETUP_PY=1` 条件下重新运行 Maya 批回归，全套当前样例通过。
  - 任务同步（2026-04-11，workflow 收尾）：已为 `mayaDmxWorkflow` 新增 `-listLegacyBatches`、`-migrateLegacyBatches`、`-cleanupBatchStorage` 三个入口；batch 文件读取错误已带 manifest 路径/行号，批量导出错误已带 entry index。`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过；Maya standalone 下已实测 legacy optionVar batch 可迁移到文件存储，且无效 `.batch` 文件可被清理。
  - 任务同步（2026-04-11，显式失败返回整理）：已将 `dcc_plugin/src/plugin` 与 `dcc_plugin/src/workflow` 中 `if (!status) { return status; }` 形式的返回整理为显式 `return MStatus::kFailure;`；`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，继续整理显式失败返回）：已继续将 `dcc_plugin/src/importer` 中 `if (!status) { return status; }` 形式的返回整理为显式 `return MStatus::kFailure;`；`dcc_plugin/src/importer` 下已无 `return status;` 残留，`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，最小 OO 重构第 1 步）：已将 workflow 内部职责拆为 [BatchManifestStore](dcc_plugin/src/workflow/BatchManifestStore.h)、[WorkflowPresetStore](dcc_plugin/src/workflow/WorkflowPresetStore.h)、[WorkflowExecutor](dcc_plugin/src/workflow/WorkflowExecutor.h) 三个对象，并补出 [WorkflowSupport](dcc_plugin/src/workflow/WorkflowSupport.h) 作为共享支撑层；[MayaDmxWorkflow.cpp](dcc_plugin/src/workflow/MayaDmxWorkflow.cpp) 已收缩为对外 API 转发层，`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，最小 OO 重构第 2 步）：已新增 [ImportSession](dcc_plugin/src/importer/ImportSession.h)，把 DMX 文件读取、解析、`ImportContext` 构建、scene root 建立、层级/shape 导入与动画应用主流程从 [DmxImportTranslator.cpp](dcc_plugin/src/importer/DmxImportTranslator.cpp) 中收拢到 session；当前 translator 已收缩为 Maya 文件翻译器入口转发层。`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，最小 OO 重构第 3 步）：已新增 [ExportSession](dcc_plugin/src/exporter/ExportSession.h)，把导出选项解析、export root 收集、`DocumentBuilder` 主流程、动画列表挂接、序列化与文件写出从 [DmxExportTranslator.cpp](dcc_plugin/src/exporter/DmxExportTranslator.cpp) 中收拢到 session；当前 exporter translator 也已收缩为 Maya 文件翻译器入口转发层。`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，最小 OO 重构第 4 步）：已收窄 [MayaDmxWorkflowCommand.cpp](dcc_plugin/src/workflow/MayaDmxWorkflowCommand.cpp) 的命令分发层，新增列表结果输出、preset 参数填充、batch entry 收集及 `save/load/run/export` 子流程 helper；当前 `doIt()` 已主要保留 flag 检测和高层调度。`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，workflow 命令层再收口）：已去除 [MayaDmxWorkflowCommand.cpp](dcc_plugin/src/workflow/MayaDmxWorkflowCommand.cpp) 中的匿名 namespace，把 flag 常量、结果拼接、列表执行、preset/batch 处理 helper 收回 `MayaDmxWorkflowCommand` 类内；`MArgDatabase`、工作中的 `ExportPreset`、batch entries、名称和输出路径也已改为成员状态，减少函数间重复传参。`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，animation 子域对象化）：已在 [DmxImportAnimation.h](dcc_plugin/src/importer/DmxImportAnimation.h) / [DmxImportAnimation.cpp](dcc_plugin/src/importer/DmxImportAnimation.cpp) 中新增 `AnimationImporter`，把动画 log 查找、关键帧写入、float/flexWeight 目标收集和 combination controls 构建收拢到 `ImportContext` 持有对象；同时在 [DmxExportAnimation.h](dcc_plugin/src/exporter/DmxExportAnimation.h) / [DmxExportAnimation.cpp](dcc_plugin/src/exporter/DmxExportAnimation.cpp) 中新增 `AnimationExporter`，把 channel 收集、flex target 去重、clip duration 累积和 `DmeAnimationList` 构建收拢到 `builder/context/exportRoots` 持有对象。对外的 `ApplyChannelsClipAnimation()`、`CreateCombinationControls()`、`BuildAnimationListElement()` 仍保留为薄包装层，调用方暂不需要改动；本轮继续避免使用匿名 namespace。`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，animation 成员化继续收口）：已继续把 [DmxImportAnimation.cpp](dcc_plugin/src/importer/DmxImportAnimation.cpp) / [DmxExportAnimation.cpp](dcc_plugin/src/exporter/DmxExportAnimation.cpp) 中反复透传的上下文收进成员。importer 侧新增 lookup roots 与当前 channel 状态成员，`FindAnimationList()` / `FindCombinationOperator()` 已改为直接消费成员上下文；exporter 侧新增当前 DAG 名称、当前 transform element 和当前 DAG path 成员，并把 position/rotation/scale 三段动画收集拆成基于成员上下文的方法。当前本轮仍避免使用匿名 namespace，`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，重构后批回归验证）：已在 `MAYA_SKIP_USERSETUP_PY=1` 条件下直接使用 `mayapy` 重新运行 [MayaBatchRegression.py](dcc_plugin/tools/MayaBatchRegression.py) 的当前样例集；`simple_hierarchy`、`simple_blendshape`、`simple_mesh`、`simple_skinned_mesh`（含 `applyAxisCorrection=0`）、`complex_chr_mesh`（含 `applyAxisCorrection=0`）、`MostComplexSampleSet/chr_mesh`（含 `applyAxisCorrection=0`）、`simple_ngon_mesh`、`MostComplexSampleSet/vcaanim_VertexAnim`、`simple_float_animation`、`simple_blendshape_animation` 全部通过，说明本轮 workflow / session / animation OO 重构后核心导入导出功能仍正常。
  - 任务同步（2026-04-11，deformer 子域对象化）：已在 [DmxImportDeformers.h](dcc_plugin/src/importer/DmxImportDeformers.h) / [DmxImportDeformers.cpp](dcc_plugin/src/importer/DmxImportDeformers.cpp) 中新增 `DeformerImporter`，把 skinCluster 创建、skin 设置恢复、deltaState 导入过程中的 `vertexData/document/mesh/basePoints` 等高频上下文收为成员；同时在 [DmxExportDeformers.h](dcc_plugin/src/exporter/DmxExportDeformers.h) / [DmxExportDeformers.cpp](dcc_plugin/src/exporter/DmxExportDeformers.cpp) 中新增 `DeformerExporter`，把 skin/blendShape 导出时的 `meshPath/meshPoints/builder/vertexDataElement/deltaStateElements` 收为成员。对外的 `AppendSkinningData()`、`AppendBlendShapeDeltaStates()`、`ApplySkinning()`、`ApplyDeltaStates()` 仍保留为薄包装层，调用方暂不需要改动；本轮继续避免使用匿名 namespace，`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，deformer 成员化继续收口）：已继续把 [DmxImportDeformers.cpp](dcc_plugin/src/importer/DmxImportDeformers.cpp) / [DmxExportDeformers.cpp](dcc_plugin/src/exporter/DmxExportDeformers.cpp) 中反复透传的 mesh 局部上下文改成成员。importer 侧新增 `bindMeshContext()`，把 `meshDagPath_ / meshParentPath_` 收为成员，`createSkinClusterWithApi()` 已不再重复接收 mesh path 参数；exporter 侧新增 `bindMeshContext()`，并收纳 `currentSkinClusterObject_ / currentBlendShapeObject_ / currentBlendShapeNodeName_`，进一步减少 skin/blendShape 导出流程中的局部重复状态。本轮继续避免使用匿名 namespace，`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，session 命名收敛）：已将过泛的 `ImportSession / ExportSession` 统一更名为 [DmxImportSession](dcc_plugin/src/importer/DmxImportSession.h) / [DmxExportSession](dcc_plugin/src/exporter/DmxExportSession.h)，并同步更新对应 `.cpp` 文件名、translator include、构建脚本与调用点；当前 session 命名已与 DMX 插件域保持一致，`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，import utils 并回 internals）：已将 importer 侧原本只剩 `SanitizeNodeName()` 的 [DmxImportUtils.h](dcc_plugin/src/importer/DmxImportUtils.h) / `.cpp` 合并回 [DmxImportInternals.h](dcc_plugin/src/importer/DmxImportInternals.h) / `.cpp`，并从 importer 构建清单中删除独立 `DmxImportUtils` 文件；当前 importer 工具入口已统一收敛到 internals 层。由于增量构建没有自动重编所有依赖源文件，本轮额外触发了相关 importer 源重编，`cmake --build build\maya_dmx --config Release --target maya_dmx` 最终通过。
  - 任务同步（2026-04-11，import session helper 并回 internals）：已将 [DmxImportSession.cpp](dcc_plugin/src/importer/DmxImportSession.cpp) 中的导入选项解析、文件读取、axis 名规范化与 root axis correction 计算 helper 全部并回 [DmxImportInternals.h](dcc_plugin/src/importer/DmxImportInternals.h) / `.cpp`，同时去除了 `DmxImportSession.cpp` 中的匿名 namespace；当前 importer 入口实现只保留 session 主流程，辅助工具已统一由 internals 层提供。`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，export session helper 并回 internals）：已将 [DmxExportSession.cpp](dcc_plugin/src/exporter/DmxExportSession.cpp) 中的导出 debug log、binary/text 导出判定、选项解析与 bool 选项解析 helper 全部并回 [DmxExportInternals.h](dcc_plugin/src/exporter/DmxExportInternals.h) / `.cpp`，同时去除了 `DmxExportSession.cpp` 中的匿名 namespace；当前 exporter 入口实现也只保留 session 主流程，辅助工具已统一由 internals 层提供。`cmake --build build\maya_dmx --config Release --target maya_dmx` 复编通过。
  - 任务同步（2026-04-11，internals 收口后二次批回归）：已在 `MAYA_SKIP_USERSETUP_PY=1` 条件下再次运行 [MayaBatchRegression.py](dcc_plugin/tools/MayaBatchRegression.py) 当前样例集；`simple_hierarchy`、`simple_blendshape`、`simple_mesh`、`simple_skinned_mesh`（含 `applyAxisCorrection=0`）、`complex_chr_mesh`（含 `applyAxisCorrection=0`）、`MostComplexSampleSet/chr_mesh`（含 `applyAxisCorrection=0`）、`simple_ngon_mesh`、`MostComplexSampleSet/vcaanim_VertexAnim`、`simple_float_animation`、`simple_blendshape_animation` 全部通过，说明 importer/exporter internals 收口后核心导入导出与动画路径仍稳定。
  - 任务同步（2026-04-11，debug log 注释校正）：当前 `AppendImportDebugLog()` 与 exporter 侧 `AppendDebugLog()` 的实现都已位于各自的 internals `.cpp` 中，translator 不再承载这类基础工具；本轮同步修正了 [DmxImportInternals.h](dcc_plugin/src/importer/DmxImportInternals.h) 中过时的实现位置注释，并再次确认 `cmake --build build\maya_dmx --config Release --target maya_dmx` 通过。

### 10. 基于现有 DMX 插件规格探索 Maya SMD 导入导出插件
- 状态：进行中（2026-04-11 已完成独立 `maya_smd.mll` 骨架、SMD 文本 parser、最小 `nodes + skeleton + triangles + skin` 导入导出、最小骨骼动画导出，以及 Maya 专项回归脚本接线；宿主实跑验证仍未收口）
- 优先级：中
- 目标目录：优先沿用 [dcc_plugin](dcc_plugin) 现有 Maya 插件工程骨架与目录组织经验，但 **SMD 必须编译为独立的 `.mll` 插件**，不能继续并入现有 `maya_dmx.mll`

- 立项依据：
  - 现有 DMX 插件已经沉淀出可复用的 Maya 宿主集成资产：构建/部署脚本、module 安装、file translator 注册、MEL option box、workflow 命令、batch 回归入口、交互宿主验证入口；这些基础设施可以复用，但二进制产物必须与 DMX 插件解耦。
  - 仓库现有 `studiomdl` 已明确保留 SMD 一线解析入口；[v1support.cpp](src/utils/studiomdl/v1support.cpp) 的 `Load_SMD()` 当前直接支持 `version / nodes / skeleton / triangles / vertexanimation` 五类主段落，可作为 SMD Maya 插件的最低兼容规格基线。
  - 仓库样例集中已存在可直接复用的 SMD/VTA 资产：`dcc_plugin/samples/MostComplexSampleSet/chr_mesh.smd`、`dcc_plugin/samples/MostComplexSampleSet/vcaanim_VertexAnim.smd`、`dcc_plugin/samples/Ellis/DMX/RAGDOLL.smd`、`dcc_plugin/samples/MostComplexSampleSet/flex.vta`，足以支撑第一阶段导入验证与后续最小 roundtrip 回归。
  - [dmemodel.h](src/public/movieobjects/dmemodel.h) 已明确说明 `upAxis=Z` 的 DmeModel 与典型 SMD 坐标系兼容；这意味着 SMD 插件与 DMX 插件之间可以共享一部分坐标系策略，但不能直接假设使用 Maya 的默认 `Y-up` 数据布局。
  - 当前 [dcc_plugin/CMakeLists.txt](dcc_plugin/CMakeLists.txt) 仍以单一 DMX 插件工程为中心；如果把 SMD 继续塞进同一 `.mll`，translator 注册、默认 options、安装产物、调试入口和故障隔离都会与 DMX 互相污染。

- 规格探索结论：
  - SMD 更适合定义为“比 DMX 更窄、更旧、更偏编译输入格式”的插件目标，而不是 DMX 插件的简化开关模式。
  - SMD 插件应作为 **独立加载、独立发布、独立回归** 的 Maya 插件存在；共享的是源码层 helper 与宿主接入经验，不是统一 `.mll`。
  - 第一阶段应聚焦 `skeleton + mesh + basic skin + transform animation`，并把 `vertexanimation` 视为单独能力面；不要一开始就试图复制 DMX 插件已有的材质网络、blendShape、metadata、workflow 语义深度。
  - SMD 是文本格式，适合优先实现稳定的 parser / writer / roundtrip；没有必要先做类似 `SimpleDmx*` 那样的通用 DOM 大层。
  - facial/flex 不应混入首批 SMD 范围。Valve 资产链路里 flex 更接近 `VTA + QC` 协同而不是“单靠 SMD 一把梭”，因此应作为第二阶段扩展，不要在首期计划里与 mesh/skeleton 主线绑定。

- 建议实现边界（第一阶段）：
  - importer：
    - `nodes` -> Maya joint / transform 层级。✅（当前已按 joint 层级导入）
    - `skeleton` bind pose -> Maya 局部 transform。✅（当前已支持首帧 pose 应用）
    - `triangles` -> Maya mesh、法线、UV、按材质名分组的基础 shading assignment。
    - 顶点骨骼权重 -> `skinCluster` 最小恢复。✅（当前已支持最小权重恢复）
    - 单文件骨骼动画 SMD -> Maya `animCurveTL / animCurveTA` 最小导入。✅（当前已为多帧 `skeleton` 生成最小平移/旋转曲线）
  - exporter：
    - Maya joint hierarchy -> `nodes`。✅（当前已支持）
    - bind pose 或当前 pose -> `skeleton` 首帧。✅（当前已支持最小静态导出）
    - polygon mesh -> `triangles`。✅（当前已支持最小静态网格导出）
    - 基础 skin 权重导出，与 `studiomdl` 可接受的骨骼索引/权重格式对齐。✅（当前已支持最小权重导出）
    - 独立动画导出先只覆盖骨骼位移/旋转，不混入 facial、材质、约束和复杂 rig metadata。✅（当前已支持最小多帧 `skeleton` 导出）
  - UI / workflow：
    - 继续沿用 translator + MEL option box + workflow command 结构，但使用独立的 SMD translator 名称、独立的 options key、独立的 plugin entry，避免和 DMX 设置串扰。
    - batch 回归入口复用 [MayaBatchRegression.py](dcc_plugin/tools/MayaBatchRegression.py) 的整体框架，新增 SMD 专用 sample gate，而不是另写一套 mayapy 测试脚本。
    - 安装与加载层要能同时存在 `maya_dmx.mll` 与未来的 `maya_smd.mll`，并支持分别加载、分别验证、分别卸载。

- 明确不纳入第一阶段的内容：
  - 完整材质网络、Hypershade 图恢复、Valve 专有 metadata。
  - `DmeCombinationOperator`、blendShape/facial rig 语义对齐。
  - QC 解析、QC authoring、SMD/VTA/QC 联动工作流。
  - 通用 DMX/SMD 统一 DOM 或“一个 translator 同时吞多种 Valve 格式”的大一统架构。
  - 把 SMD translator 直接编进现有 `maya_dmx.mll` 的混合方案。

- 主要技术风险：
  - SMD 实际兼容面最终要以 `studiomdl` 的历史容忍度为准，而不是仅凭网络上常见民间 SMD 说明；后续实现时需要继续对照 `Grab_Nodes`、`Grab_Animation`、`Grab_Triangles`、`Grab_Vertexanimation` 的实际读取规则补齐细节。
  - 坐标系风险高于 DMX。SMD 常见约定与 Maya `Y-up` 不同，而仓库现有注释已说明“兼容典型 SMD 的 Z-up”与“引擎空间”仍不是一回事，导入导出都必须显式定义轴修正策略。
  - SMD 的材质语义很薄，通常只有 triangle material 名；如果导出端过早绑定 Maya 材质网络，将导致回归标准模糊并拖慢主线。
  - `vertexanimation` 与 `VTA/flex` 边界容易混淆；首阶段必须先把两者拆开，否则测试样例和代码结构都会失焦。
  - 如果继续共享单一 `.mll`，任何 SMD 回归失败、注册异常或 Maya 加载崩溃都会直接拖累已稳定的 DMX 插件，因此二进制隔离本身就是风险控制要求。

- 分阶段计划：
  - 第一阶段：完成 SMD 规格固化与脚手架接入。
    - 在 `dcc_plugin` 内增加独立的 SMD importer/exporter translator、options script、默认参数和样例登记。
    - 新增独立插件目标与产物命名，预期形态为 `maya_smd.mll`；不要把目标继续挂在 `maya_dmx` 下。
    - 规划独立安装路径、独立 module 清单项或独立 plugin 注册入口，确保 SMD/DMX 可以并存加载。
    - 整理最小样例矩阵：`chr_mesh.smd`、`vcaanim_VertexAnim.smd`、`RAGDOLL.smd`。
    - 明确坐标系、骨骼索引、法线/UV、材质名的回归判据。
  - 第二阶段：完成 mesh/skeleton 最小闭环。
    - 跑通 `导入 -> Maya 导出 -> 再导入` 的 mesh topology、joint 层级、skin influence 数量三重回归。
    - 让 SMD 回归成为独立门槛，避免后续改动退回“只能导 mesh，不能导骨架/权重”。
  - 第三阶段：补动画闭环。
    - 用 `vcaanim_VertexAnim.smd` 或等价纯骨骼动画样例建立独立 animation gate。
    - 明确动画 SMD 与静态 mesh SMD 是否拆分导出入口，避免 UI 与回归路径混杂。
  - 第四阶段：评估扩展项。
    - 再决定是否引入 `vertexanimation`、`VTA`、更强的材质名约束、以及和 DMX workflow 的更深层共用抽象。

- 落地约束：
  - 复用现有 [MayaDmxCommon.h](dcc_plugin/src/common/MayaDmxCommon.h) 一类宿主接入模式与 [MayaBatchRegression.py](dcc_plugin/tools/MayaBatchRegression.py) 的回归框架，但必须为 SMD 建立独立的 plugin target、独立的 `initializePlugin/uninitializePlugin` 入口和独立的二进制命名。
  - 若后续开始实现，优先拆出 `src/common` 下与格式无关的 Maya helper，再新增 `src/common_smd`、`src/plugin_smd`、`src/importer_smd`、`src/exporter_smd` 或等价清晰边界；不要把 SMD 逻辑继续堆进现有 DMX translator 文件，也不要让两者共享同一个 plugin main。
  - 计划默认以 Maya 2022.5、Win32/Win64 宿主限制、当前 `dcc_plugin` 工程约束为准，不额外引入 Python-only 导出器作为主线方案。
  - DMX 与 SMD 的安装脚本、module 部署、MEL 脚本命名、optionVar 前缀、workflow 命令名都要预留隔离命名空间，避免双插件并存时出现覆盖或串配置。
  - 目录命名保持一致性：SMD 公共层目录统一使用 `common_smd`，不要再引入 `smd_common` 这类倒置命名。
  - 任务同步（2026-04-11，SMD 骨架起步）：已在 `dcc_plugin` 内新增独立的 `maya_smd` 构建目标、[common_smd](dcc_plugin/src/common_smd)、[importer_smd](dcc_plugin/src/importer_smd)、[exporter_smd](dcc_plugin/src/exporter_smd)、[plugin_smd](dcc_plugin/src/plugin_smd) 四组骨架目录；当前 `cmake -S dcc_plugin -B build\maya_dmx -A x64` 与 `cmake --build build\maya_dmx --config Release --target maya_smd` 已通过，产物输出为 [maya_smd.mll](dcc_plugin/bin/Release/maya_smd.mll)。当前 importer/exporter session 仍只做扩展名校验与 stub 失败返回，下一步再补真实 SMD parser / writer。
  - 任务同步（2026-04-11，SMD importer 最小落地）：已在 [common_smd](dcc_plugin/src/common_smd) 新增 [SimpleSmdDocument.h](dcc_plugin/src/common_smd/SimpleSmdDocument.h) / [SimpleSmdDocument.cpp](dcc_plugin/src/common_smd/SimpleSmdDocument.cpp)，支持 `version / nodes / skeleton / triangles / vertexanimation` 的文本解析与基础序列化；在 [importer_smd](dcc_plugin/src/importer_smd) 新增 [SmdSceneImporter.h](dcc_plugin/src/importer_smd/SmdSceneImporter.h) / [SmdSceneImporter.cpp](dcc_plugin/src/importer_smd/SmdSceneImporter.cpp)，当前已能把 `nodes` 创建为 Maya joint 层级、应用 `skeleton` 首帧 pose，并为多帧 `skeleton` 写入最小平移/旋转动画曲线。`triangles` 与 `vertexanimation` 当前仅完成解析并在导入时给出显式 warning，mesh、skin、材质和 exporter 主流程仍待继续实现。`cmake --build build\maya_dmx --config Release --target maya_smd` 已复编通过。
  - 任务同步（2026-04-11，SMD mesh importer 与最小 exporter 起步）：已在 [importer_smd](dcc_plugin/src/importer_smd) 新增 [SmdMeshImporter.h](dcc_plugin/src/importer_smd/SmdMeshImporter.h) / [SmdMeshImporter.cpp](dcc_plugin/src/importer_smd/SmdMeshImporter.cpp)，当前可按材质名把 `triangles` 拆成基础 Maya mesh，并恢复点位、法线、UV；顶点权重当前仅检测并告警，skinCluster 尚未恢复。与此同时已在 [exporter_smd](dcc_plugin/src/exporter_smd) 新增 [SmdSceneExporter.h](dcc_plugin/src/exporter_smd/SmdSceneExporter.h) / [SmdSceneExporter.cpp](dcc_plugin/src/exporter_smd/SmdSceneExporter.cpp)，当前已能从 Maya DAG 收集最小 joint/transform 层级并写出 `nodes + skeleton` 文本 SMD，也已能把 Maya mesh 按面三角化后导出为最小 `triangles` 段，恢复基础材质名、点位、法线与 UV。skin 权重导出和多帧动画导出仍待继续实现。`cmake --build build\maya_dmx --config Release --target maya_smd` 已复编通过。
  - 任务同步（2026-04-11，SMD exporter 对象状态收口）：已将 [SmdSceneExporter](dcc_plugin/src/exporter_smd/SmdSceneExporter.h) 的 `simple_smd::Document` 从 `Build()` 的外部引用参数改为类成员 `document_`，并新增只读访问入口 `document()`；当前 exporter 已改为“内部构建状态 + 外部读取序列化结果”的对象模式，减少 `nodes / skeleton / triangles` 构建阶段的来回透传。`cmake --build build\maya_dmx --config Release --target maya_smd` 已复编通过。
  - 任务同步（2026-04-11，SMD export session 成员化继续收口）：已将 [SmdExportSession](dcc_plugin/src/exporter_smd/SmdExportSession.h) 内部的导出流程对象化，新增成员 `sceneExporter_` 与 `serialized_`，并把 `Run()` 拆成 `buildDocument()`、`serialize()`、`writeOutput()` 三段；当前 session 不再依赖 `Run()` 内的局部 exporter/serialized 临时变量，后续继续补导出选项、动画导出或错误上下文时可以直接挂到 session 状态上。`cmake --build build\maya_dmx --config Release --target maya_smd` 已复编通过。
  - 任务同步（2026-04-11，SMD 最小 skin importer 接通）：已继续扩展 [SmdMeshImporter](dcc_plugin/src/importer_smd/SmdMeshImporter.cpp)，当前在按材质组建出基础 Maya mesh 后，会从 SMD vertex links 收集活跃骨骼、创建最小 `skinCluster`，并通过 `MFnSkinCluster::setWeights()` 恢复顶点权重。当前方案优先保证 `mesh + skeleton + weights` 最小链路可用，尚未扩展到更复杂的 bind pose extras、权重裁剪策略或 skin 导出。`cmake --build build\maya_dmx --config Release --target maya_smd` 已复编通过。
  - 任务同步（2026-04-11，SMD 最小 skin exporter 接通）：已继续扩展 [SmdSceneExporter](dcc_plugin/src/exporter_smd/SmdSceneExporter.cpp)，当前在导出 `triangles` 时会检测上游 `skinCluster`，通过 `MFnSkinCluster::getWeights()` 收集 mesh 顶点权重，并按当前 exporter 的 `nodeIndexByPath_` 映射写回 SMD vertex links。当前方案优先保证 `mesh + skeleton + weights` 最小闭环成立，尚未扩展到多帧骨骼动画导出、复杂 bind pose 元数据或更激进的权重裁剪策略。`cmake --build build\maya_dmx --config Release --target maya_smd` 已复编通过。
  - 任务同步（2026-04-11，SMD 最小骨骼动画导出接通）：已继续扩展 [SmdSceneExporter](dcc_plugin/src/exporter_smd/SmdSceneExporter.cpp)，当前会收集导出节点上 `translateX/Y/Z` 与 `rotateX/Y/Z` 的 animCurve 关键帧时间并集，并按帧采样写出多帧 `skeleton` time blocks；无动画时仍回退为单帧静态 skeleton。当前方案优先保证 `vcaanim_VertexAnim.smd` 这类纯骨骼动画样例进入导出闭环，尚未扩展到更高层 clip/sequence 语义或更复杂的动画压缩/去噪策略。`cmake --build build\maya_dmx --config Release --target maya_smd` 已复编通过。
  - 任务同步（2026-04-11，SMD 样例接入 Maya 专项回归）：已更新 [MayaBatchRegression.py](dcc_plugin/tools/MayaBatchRegression.py)，把回归入口从“仅 DMX”扩展为按格式驱动的 importer/exporter 选择：DMX 继续跑 text/binary 双变体，SMD 新增 text 变体，并支持 `--plugin-smd`。同时已更新 [RunMayaBatchRegression.bat](dcc_plugin/RunMayaBatchRegression.bat)，当前会同时要求 `maya_dmx.mll` 与 `maya_smd.mll`，并把 `MostComplexSampleSet/chr_mesh.smd`、`MostComplexSampleSet/vcaanim_VertexAnim.smd`、`Ellis/DMX/RAGDOLL.smd` 三个 SMD 样例加入 Maya 批回归 case 列表。当前已完成脚本语法校验（`python -m py_compile dcc_plugin\tools\MayaBatchRegression.py`）与插件复编；宿主 mayapy 实跑结果仍需在真实 Maya 环境下补验证。
  - 任务同步（2026-04-11，SMD 真实宿主回归排障中）：已在真实 `mayapy` 宿主下复现并修正两类 importer/exporter 问题。其一，`chr_mesh.smd` 初次导入失败已定位并修复为法线 face-vertex 索引错误，以及 `skinCluster -tsb` 命令式创建不稳定；当前 [SmdMeshImporter.cpp](dcc_plugin/src/importer_smd/SmdMeshImporter.cpp) 已改为更细粒度报错、修正法线索引，并切换为 API 方式创建 `skinCluster`。其二，SMD exporter 先前在 `cmds.file(... exportSelected ...)` 返回前出现宿主级中止，当前已在 [SmdImportTranslator.cpp](dcc_plugin/src/importer_smd/SmdImportTranslator.cpp) / [SmdExportTranslator.cpp](dcc_plugin/src/exporter_smd/SmdExportTranslator.cpp) 补上 C++ 异常边界后恢复为可回到 Python 调用侧。基于真实回归结果，当前又确认了两条 roundtrip 差异：一是 exporter 需要优先保留原始 SMD `materialName`，目前已通过 `mayaSmdMaterialName` 自定义属性在 importer/exporter 间回传；二是 exporter 的“骨架节点收集”与“mesh 搜索根”仍未完全收口，`chr_mesh.smd` 现阶段在 `nodes`/`triangles` 联合回归上仍处于排障中，后续优先继续稳定 `meshRoots_` 路径并消除导出期宿主中止。
  - 任务同步（2026-04-11，SMD 导入自定义欧拉旋转评估）：已评估“用户在导入界面为 SMD 指定自定义 XYZ 欧拉旋转值”的可行性，结论是适合做，而且应作为 SMD importer 独立选项实现，而不是写死在坐标系修正里。当前 [SmdImportTranslator.cpp](dcc_plugin/src/importer_smd/SmdImportTranslator.cpp) -> [SmdImportSession.cpp](dcc_plugin/src/importer_smd/SmdImportSession.cpp) 的 `options` 通道已经存在，但 `maya_smd` 还没有像 DMX 那样的独立 MEL option box；后续应补 `mayaSmdTranslatorImport` 脚本、SMD 专用 `optionVar` 命名空间，以及 `rotateX/rotateY/rotateZ` 三个角度选项。实现边界建议为“把用户输入的 XYZ 欧拉角作为额外导入旋转偏移，统一作用在 import root 或显式导入偏移节点上”，优先不要直接逐 joint 改写 bind pose，以免破坏动画帧语义和 skin/bone 层级回归。该选项应默认 `0,0,0`，并进入 Maya batch regression 的 import option 变体，用于验证 `nodes / skeleton / triangles / skin` 在自定义旋转偏移下仍能稳定导入。
  - 任务同步（2026-04-11，SMD 导入自定义欧拉旋转已接通）：已在 [plugin_smd/PluginMain.cpp](dcc_plugin/src/plugin_smd/PluginMain.cpp) 为 `Valve SMD Import` 注册独立 import options script，默认选项串为 `rotateX=0;rotateY=0;rotateZ=0`；已新增 [performSmdImport.mel](dcc_plugin/maya_module/scripts/performSmdImport.mel) 与 [mayaSmdTranslatorImport.mel](dcc_plugin/maya_module/scripts/mayaSmdTranslatorImport.mel)，当前 Maya 文件导入界面可填写自定义 `XYZ` 欧拉角。Importer 侧已在 [SmdImportSession.cpp](dcc_plugin/src/importer_smd/SmdImportSession.cpp) 解析 `rotateX/rotateY/rotateZ`，并在 [SmdSceneImporter.cpp](dcc_plugin/src/importer_smd/SmdSceneImporter.cpp) 把该偏移作为额外旋转应用到 `smd_import_root#`，不直接改写 joint bind pose。当前已验证 `cmds.file(..., options='rotateX=10;rotateY=20;rotateZ=30')` 导入后，根节点旋转确实为 `(10, 20, 30)`；MEL option box 的交互路径尚待在完整 Maya UI 下做一次人工确认。
  - 任务同步（2026-04-11，插件批处理脚本已扩展到编译与部署双插件）：已更新 [BuildPlugin.bat](dcc_plugin/BuildPlugin.bat) 使其显式构建 `maya_dmx` 与 `maya_smd` 两个 target，并在完成后同时汇报两个 `.mll` 产物；已更新 [CreatePluginSolution.bat](dcc_plugin/CreatePluginSolution.bat) 的文案，使其面向整套 Maya 插件工程而不再只指向 DMX；已更新 [InstallPluginModuleToMaya.bat](dcc_plugin/InstallPluginModuleToMaya.bat) 以同时检查、复制、汇报 `maya_dmx.mll` 与 `maya_smd.mll`，并同步部署对应 `.pdb`。同时修正了 `maya_module/scripts` 作为源码目录时不能先清空再复制的问题，避免安装脚本误删仓库内脚本。当前已实际跑通 `BuildPlugin.bat Release x64`，确认会同时产出两个插件；真正写入用户 `Documents\maya\modules` 的安装步骤仍需在当前环境外部执行。
  - 任务同步（2026-04-11，MEL 源目录与部署目录重新厘清）：已按仓库约定把 SMD 相关 MEL 源脚本 [performSmdImport.mel](dcc_plugin/src/mel/performSmdImport.mel) 与 [mayaSmdTranslatorImport.mel](dcc_plugin/src/mel/mayaSmdTranslatorImport.mel) 从 `maya_module/scripts` 迁回统一源码目录 [src/mel](dcc_plugin/src/mel)，并删除误放到部署目录的副本。与此同时已修正 [InstallPluginModuleToMaya.bat](dcc_plugin/InstallPluginModuleToMaya.bat)，当前重新把 `src/mel` 作为唯一 MEL 源目录，并恢复“清空 `maya_module/scripts` 后统一复制源码 MEL”的部署模型，避免后续继续混淆“仓库内源码”和“module 部署产物”。
  - 任务同步（2026-04-11，SMD 导入 UI MEL 兼容性修正）：已修正 [performSmdImport.mel](dcc_plugin/src/mel/performSmdImport.mel) 中 `floatFieldGrp` 的非法 `-label1/-label2/-label3` 参数，改为 Maya 兼容的基础三字段写法，并已重新执行 [InstallPluginModuleToMaya.bat](dcc_plugin/InstallPluginModuleToMaya.bat) 刷新部署目录。当前如果 Maya 之前已经打开，需要重启后再重新打开 SMD 导入界面，避免继续使用旧脚本缓存。
  - 任务同步（2026-04-11，SMD importer 最小材质绑定接通）：已继续扩展 [SmdMeshImporter.cpp](dcc_plugin/src/importer_smd/SmdMeshImporter.cpp)，当前在按 `triangles.materialName` 拆分 mesh 后，会为每个材质组创建或复用最小 `lambert` shader 与对应 `shadingEngine`，并把导入 mesh 绑定到该 shading group。当前方案优先保证 Maya 内能看到基础材质绑定和按材质组的最小可视化，不涉及纹理文件解析、复杂材质网络恢复或更高层材质语义。已在真实 `mayapy` 宿主验证 `chr_mesh.smd` 导入后，mesh 分别连接到 `tex_d_bmp_SG` 与 `tex_i_bmp_SG`，并生成对应 `lambert` 材质节点。

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
