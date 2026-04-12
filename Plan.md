# 项目计划

此文件用于跟踪在仓库操作过程中确认的问题、待修复事项和后续工作。

更新规则：
- 每次任务结束后，都要检查本次是否发现了新的确认问题、解决了已有问题，或调整了优先级。
- 将此文件作为仓库范围内的统一修复计划，不要把关键待办只留在对话记录中。
- 仅记录可执行、可追踪的问题，并提供足够的上下文，方便后续快速接手。
- 在 Maya 插件开发过程中，如果遇到 Maya API 行为、参数语义或版本兼容边界不清晰的实现点，优先查阅 Autodesk 官方 C++ API 参考文档：https://help.autodesk.com/view/MAYAUL/2024/CHS/?guid=MAYA_API_REF_cpp_ref_index_html ，确认后再决定实现方式。

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
  - 插件独立构建目录：`dcc_plugin\build`
  - 最终项目级回归样本目录：`D:\_Code_Here\Git\csgo-src\dcc_plugin\samples\Ellis\DMX`
  - 宿主环境查询入口：[QueryMayaValidationEnv.bat](dcc_plugin/QueryMayaValidationEnv.bat)、[QueryMayaValidationEnv.ps1](dcc_plugin/tools/QueryMayaValidationEnv.ps1)
  - 宿主环境说明文档：[MayaValidationEnv.md](dcc_plugin/docs/MayaValidationEnv.md)

- 当前概况：
  - 构建、部署与 Maya module 安装链路已稳定，`cmake --build dcc_plugin\build --config Release` 可生成 [maya_dmx.mll](dcc_plugin/bin/Release/maya_dmx.mll)，[InstallPluginModuleToMaya.bat](dcc_plugin/InstallPluginModuleToMaya.bat) 负责同步 `.mll`、`.pdb` 与 MEL 脚本。
  - MEL 真源已统一到 [src/mel](dcc_plugin/src/mel)，`maya_module/scripts` 仅作为安装产物。
  - 实现约束：避免通过拼装 MEL 字串实现核心功能，优先使用 Maya C++ API；只有 file type specific options、option box 或 Maya 原生脚本入口确实要求 MEL 时，才保留最小必要脚本桥接。
  - 插件主线已形成“DMX 基础层 + importer/exporter + Maya UI/workflow + batch 回归 + 交互宿主验证”的最小闭环，但复杂角色样例 roundtrip 和完整 facial/animation/export 仍未收口。
  - 2026-04-11 已通过宿主环境查询脚本确认当前机器具备 Maya 2022、mayapy、DevKit、插件二进制、MEL 脚本和回归入口的基础路径条件；查询报告输出到 `dcc_plugin\build\maya_validation_env_report.md`。

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
    - ✅ ~~统一 `dcc_plugin` 独立构建目录到 `dcc_plugin\build`，并把 module 安装链路继续固定为同时复制 `.mll` 与 `.pdb`，避免插件脚本与仓库根 `build\` 混用。~~（已完成，2026-04-12）：[BuildPlugin.bat](dcc_plugin/BuildPlugin.bat)、[CreatePluginSolution.bat](dcc_plugin/CreatePluginSolution.bat)、[RunSampleRegression.bat](dcc_plugin/RunSampleRegression.bat)、[RunMayaBatchRegression.bat](dcc_plugin/RunMayaBatchRegression.bat)、[QueryMayaValidationEnv.bat](dcc_plugin/QueryMayaValidationEnv.bat) 与 [QueryMayaValidationEnv.ps1](dcc_plugin/tools/QueryMayaValidationEnv.ps1) 已统一切到 `dcc_plugin\build`；[InstallPluginModuleToMaya.bat](dcc_plugin/InstallPluginModuleToMaya.bat) 的 `.pdb` 同步链路已复核保留，并同步更新了文档说明。
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
  - 任务同步（2026-04-12，插件构建目录与符号部署约束收口）：已将 `dcc_plugin` 独立包装脚本统一到 `dcc_plugin\build`，不再写入仓库根 `build\maya_dmx`；宿主环境查询报告默认也改写到 `dcc_plugin\build\maya_validation_env_report.md`。同时复核 [InstallPluginModuleToMaya.bat](dcc_plugin/InstallPluginModuleToMaya.bat) 仍会随 `.mll` 一起复制 `maya_dmx.pdb` / `maya_smd.pdb`，并补齐了 README 与宿主环境文档中的显式说明。
  - 任务同步（2026-04-12，插件 PDB 输出目录修正）：在 [MayaPluginSupport.cmake](dcc_plugin/cmake/MayaPluginSupport.cmake) 为插件目标补上 `PDB_OUTPUT_DIRECTORY` 与 `COMPILE_PDB_OUTPUT_DIRECTORY`，统一把 `maya_dmx` / `maya_smd` 的调试符号落到 `dcc_plugin\bin\Release`；这样 [BuildPlugin.bat](dcc_plugin/BuildPlugin.bat) 构建完成后，`InstallPluginModuleToMaya.bat` 复制 `.pdb` 的前提才真正成立。后续应以“`dcc_plugin\bin\Release` 同时存在 `.mll` 与 `.pdb`”作为插件构建完成的门槛。
  - 任务同步（2026-04-12，Release PDB 链接开关补齐）：进一步复核生成的 `maya_dmx.vcxproj` / `maya_smd.vcxproj` 后，确认 Release 配置里 `ProgramDataBaseFile` 虽已指向 `dcc_plugin\bin\Release`，但 `GenerateDebugInformation` 仍是 `false`，导致不会实际生成 `.pdb`。现已在 [MayaPluginSupport.cmake](dcc_plugin/cmake/MayaPluginSupport.cmake) 为插件目标补上 MSVC `Release/MinSizeRel` 的 `/DEBUG:FULL`，要求 `BuildPlugin.bat` 的 Release 构建也产出可部署的 `.pdb`。
  - 任务同步（2026-04-12，PDB 构建开关分流）：已新增 CMake 选项 `MAYA_DMX_BUILD_PDB`。当前 [CreatePluginSolution.bat](dcc_plugin/CreatePluginSolution.bat) 会用 `-DMAYA_DMX_BUILD_PDB=ON` 生成工程，保留可调试的 `.pdb` 输出；[BuildPlugin.bat](dcc_plugin/BuildPlugin.bat) 则固定用 `-DMAYA_DMX_BUILD_PDB=OFF` 做快速构建，并在构建前清理 `dcc_plugin\bin\%CONFIG%\maya_*.pdb`，避免旧符号残留误判。后续插件链路默认约束更新为：命令行快速构建不带 `.pdb`，Visual Studio 工程构建带 `.pdb`。
  - 任务同步（2026-04-12，PDB 开关执行验证）：已在当前环境实测 [BuildPlugin.bat](dcc_plugin/BuildPlugin.bat) 的 `Release` 快速构建路径，确认脚本会以 `-DMAYA_DMX_BUILD_PDB=OFF` 重新配置，并在成功收尾后移除插件 `.pdb`，最终输出列表只保留 `.mll`。同时已复核 [CreatePluginSolution.bat](dcc_plugin/CreatePluginSolution.bat) 生成的 `Release` `.vcxproj` 在 `MAYA_DMX_BUILD_PDB=ON` 下会写入 `GenerateDebugInformation=DebugFull` 与 `ProgramDataBaseFile=...maya_*.pdb`；受当前沙箱/真实工作区输出路径混用影响，这一支的“最终磁盘上确有 `.pdb` 文件”未能在本轮环境里稳定二次复现，但工程配置层已切到产出 `.pdb` 的状态。
  - 任务同步（2026-04-12，共用导入工作流继续落地）：已把 `importMode=update` 从“仅解析 + warning”提升到复用现有 DMX/SMD 共用“命中已有对象并复用”的路径。当前 [ImportPolicy.h](dcc_plugin/src/common/ImportPolicy.h) 新增 `UsesExistingObjectMerge()`，DMX 侧 [DmxImportDag.cpp](dcc_plugin/src/importer/DmxImportDag.cpp)、[DmxImportMesh.cpp](dcc_plugin/src/importer/DmxImportMesh.cpp)、[DmxImportAnimation.cpp](dcc_plugin/src/importer/DmxImportAnimation.cpp)、[DmxImportDeformers.cpp](dcc_plugin/src/importer/DmxImportDeformers.cpp) 与 SMD 侧 [SmdSceneImporter.cpp](dcc_plugin/src/importer_smd/SmdSceneImporter.cpp)、[SmdMeshImporter.cpp](dcc_plugin/src/importer_smd/SmdMeshImporter.cpp) 已统一改为让 `update/append` 共用已有层级、mesh group、blendShape/control 节点的复用逻辑；session 层 warning 也已同步更新为“当前先复用已有对象，但完整 overwrite/update 语义仍未实现”。`cmake --build dcc_plugin\build --config Release --target maya_dmx` 与 `maya_smd` 已复编通过。

### 10. 基于现有 DMX 插件规格探索 Maya SMD 导入导出插件
- 状态：进行中
- 优先级：中
- 总目标：
  - 在 [dcc_plugin](dcc_plugin) 内形成一套清晰的 Valve Maya 插件路线：`maya_dmx.mll` 负责 DMX，`maya_smd.mll` 负责 SMD，二者共享宿主基础设施，但独立构建、独立加载、独立回归、独立发布。
  - 让 DMX / SMD 后续都具备“可单文件导入导出”和“可混合并入现有场景”两条工作流，并把后者作为中期主线能力，而不是只停留在单次导入包装根模式。

- 范围边界：
  - 当前主线只覆盖 `nodes / skeleton / triangles / basic skin / basic animation`。
  - 不把 `vertexanimation`、`VTA/flex`、复杂材质网络、blendShape/facial rig、QC 联动、统一 DMX/SMD DOM 纳入当前主线里程碑。
  - SMD 必须保持独立 `.mll`，不能并入 `maya_dmx.mll`。

- 统一设计约束：
  - DMX / SMD 都要允许读取 Maya 当前名称空间设置。
  - DMX / SMD importer 后续都要支持两种根策略：
    - 导入到包装根
    - 直接导入到当前场景
  - 一旦“场景根导入”和“自动旋转 / 轴线修正 / 自定义欧拉旋转”冲突，优先保证场景混合导入正确；必要时关闭或弱化旋转/轴向兼容。
  - 导入策略应向 FBX 导入器靠拢，至少预留：
    - 更新当前场景
    - 仅附加当前场景不存在的对象
    - 仅导入动画
    - 将动画写入动画层
  - 在“附加”模式下，如果当前场景已存在同名父级，则应在该父级下继续创建缺失子层级，而不是重新起一套平级树。
  - 如果当前场景已存在同名对象，动画导入优先走动画层，而不是直接覆盖 base animation。

- 关键风险：
  - SMD 的真实兼容面最终要以 `studiomdl` 的读取容忍度为准，不能只按民间 SMD 描述实现。
  - SMD 坐标系和 Maya `Y-up` 的冲突比 DMX 更尖锐，且会与“场景根导入”直接打架。
  - “更新当前场景 / 附加缺失对象 / 动画层导入”会把同名判断、命名空间映射、层级匹配复杂度显著抬高，必须先统一匹配规则。
  - SMD 材质语义薄，当前只适合做最小绑定，不适合过早承诺完整材质网络恢复。

- 里程碑：
  - 里程碑 M1：插件骨架与独立二进制
    - 目标：建立 `maya_smd.mll`，与 DMX 二进制隔离，并复用现有 Maya 插件工程宿主基础设施。
    - 出口条件：
      - `maya_smd.mll` 可独立构建、独立加载、独立卸载
      - 构建/部署脚本同时支持 DMX 和 SMD
      - SMD 有独立 translator、独立 MEL 入口、独立 options 命名空间
    - 状态：已基本完成

  - 里程碑 M2：SMD 最小格式闭环
    - 目标：打通 SMD 的最小导入导出主线。
    - 出口条件：
      - importer 支持 `nodes / skeleton / triangles / basic skin / basic animation`
      - exporter 支持 `nodes / skeleton / triangles / basic skin / basic animation`
      - SMD 在 Maya 内具备最小材质绑定与基本可视化
      - `chr_mesh.smd`、`vcaanim_VertexAnim.smd`、`RAGDOLL.smd` 能进入批回归
    - 状态：大部分完成，真实宿主 roundtrip 仍未收口

  - 里程碑 M3：真实宿主回归收口
    - 目标：把当前“能导入 / 能导出”推进到“能稳定 roundtrip”。
    - 出口条件：
      - `chr_mesh.smd` 的 `nodes / triangles / skin / materialName` roundtrip 稳定
      - `vcaanim_VertexAnim.smd` 的最小动画 roundtrip 稳定
      - SMD exporter 不再在真实宿主下出现导出期中止或 root/mesh 遍历歧义
      - SMD 批回归有明确 pass/fail 门槛
    - 当前阻塞：
      - `chr_mesh.smd` 的 `nodes / triangles` 联合回归仍在排障
      - exporter 的骨架节点收集与 mesh 搜索根仍需继续收口

  - 里程碑 M4：统一导入模式升级
    - 目标：把 DMX / SMD 的导入模型从“单次导入”升级到“可并入现有场景”。
    - 出口条件：
      - DMX / SMD 都支持“包装根导入”和“场景根导入”
      - 允许读取 Maya 当前名称空间设置
      - 支持“更新当前场景 / 仅附加缺失对象”的基础对象合并策略
      - 对层级对象的同名父级附加行为可预测、可回归
    - 优先级说明：
      - 此里程碑优先级高于旋转/轴线兼容增强
      - 若与自动旋转冲突，优先保留场景混合导入

  - 里程碑 M5：动画合并与动画层
    - 目标：让导入动画可以安全并入已有场景，而不是只覆盖。
    - 出口条件：
      - 支持“仅导入动画”
      - 当前场景存在同名对象时，可将导入动画写入动画层
      - 动画层至少提供一种明确模式，并预留扩展为多模式
      - 动画层导入进入 DMX / SMD 统一回归

  - 里程碑 M6：扩展项评估
    - 目标：在主线稳定后，再评估高复杂度能力。
    - 评估项：
      - `vertexanimation`
      - `VTA / flex`
      - 更强材质约束
      - 更接近 FBX 的 scene-merge 策略模板
      - 与 DMX workflow 的更深层共用抽象

- 当前完成面：
  - `maya_smd.mll` 已独立落地，构建、部署、回归脚本已接通。
  - SMD parser / writer 已支持 `version / nodes / skeleton / triangles / vertexanimation` 文本主段落。
  - SMD importer 已支持：
    - `nodes`
    - `skeleton`
    - `triangles`
    - 最小 `skinCluster`
    - 最小动画导入
    - 最小材质绑定
    - 自定义导入根欧拉旋转
  - SMD exporter 已支持：
    - `nodes`
    - `skeleton`
    - `triangles`
    - 最小 skin
    - 最小骨骼动画
  - SMD 样例已接入 Maya batch regression。

- 当前未收口项：
  - `chr_mesh.smd` 的真实宿主 roundtrip 仍未完全通过。
  - exporter 的 root/node/mesh 遍历边界仍需继续整理。
  - DMX / SMD 统一的“场景根导入 / 名称空间 / 更新或附加策略 / 动画层导入”仍停留在计划层，尚未开始实现。

- 下一阶段执行顺序：
  - 第一步：先收口 SMD 当前 roundtrip
    - 目标：先把 `chr_mesh.smd` 在真实宿主下的 `nodes / triangles / skin / materialName` roundtrip 跑通。
    - 具体动作：
      - 继续整理 exporter 的骨架节点收集与 mesh 搜索根逻辑
      - 固化 `mayaSmdMaterialName` 回传路径
      - 把当前真实宿主错误转成可稳定复现的专项 case
    - 完成标准：
      - `MostComplexSampleSet/chr_mesh.smd` 批回归通过

  - 第二步：补 SMD 动画专项回归
    - 目标：把 `vcaanim_VertexAnim.smd` 从“可导入导出”提升到“可稳定回归”。
    - 具体动作：
      - 补动画 key 数量、时间范围、源节点映射检查
      - 明确静态 SMD 与动画 SMD 在导入 UI 上的最小选项边界
    - 完成标准：
      - `MostComplexSampleSet/vcaanim_VertexAnim.smd` 动画 gate 通过

  - 第三步：给 DMX / SMD 抽统一 importer 策略对象
    - 目标：不要在 DMX 和 SMD 各自临时散落实现“场景根 / 名称空间 / 更新或附加”逻辑。
    - 具体动作：
      - 新增统一 importer options 模型
      - 明确同名对象匹配规则
      - 明确“包装根导入”与“场景根导入”的共享决策入口
    - 完成标准：
      - DMX / SMD importer 都能消费同一套高层导入策略字段

  - 第四步：落地“使用场景作为根节点”
    - 目标：让 DMX / SMD 都能直接并入当前场景。
    - 具体动作：
      - 增加导入选项与默认策略
      - 在 scene-root 模式下禁用或绕过有冲突的轴向/旋转修正
      - 建立多次连续导入的层级与命名回归
    - 完成标准：
      - 连续导入多个文件时，不会强制生成额外包装根，也不会串挂已有层级

  - 第五步：落地 FBX 风格“更新 / 附加”对象策略
    - 目标：让 importer 从“只会新增”升级为“会合并场景”。
    - 具体动作：
      - 支持“更新当前场景”
      - 支持“仅附加当前场景不存在的对象”
      - 对层级对象支持“命中同名父级时继续补子层级”
    - 完成标准：
      - 同一 skeleton / mesh 二次导入时，行为可预测且可回归

  - 第六步：落地动画层导入
    - 目标：避免导入动画直接破坏当前场景已有动画。
    - 具体动作：
      - 增加“仅导入动画”选项
      - 对命中同名对象的动画支持写入动画层
      - 增加动画层模式选项，并先固定一个默认模式
    - 完成标准：
      - 同名对象动画导入默认不破坏 base animation

  - 第七步：回头评估扩展项
    - 目标：在主线稳定后，再看高复杂度能力是否值得推进。
    - 具体动作：
      - 评估 `vertexanimation / VTA / flex`
      - 评估更强材质恢复
      - 评估更细的 scene-merge 策略模板
    - 完成标准：
      - 扩展项有明确“纳入 / 暂缓 / 放弃”结论

- 最近任务同步：
  - 2026-04-11：完成 `maya_smd.mll` 独立骨架、`common_smd` 重命名、最小 importer/exporter 主线、批处理构建/部署脚本扩展。
  - 2026-04-11：完成 SMD 导入最小材质绑定、自定义导入根欧拉旋转、SMD MEL 导入界面与 module 部署修正。
  - 2026-04-11：完成 SMD 样例接入 Maya 专项回归，并在真实 `mayapy` 宿主下开始收敛 `chr_mesh.smd` roundtrip。
  - 2026-04-11：明确后续 DMX / SMD 共用导入路线，新增“场景根导入、名称空间兼容、更新/附加策略、动画层导入”统一规划。
  - 2026-04-11：完成 `dcc_plugin` 内部一轮面向对象收口，已将 DMX/SMD importer/exporter 中仍使用类成员引用保存 `ImportContext / ExportContext / DocumentBuilder / Document` 的 helper 统一改为智能指针句柄，避免在类内部继续保留引用成员。
  - 2026-04-11：完成 SMD 三个专项样例在真实 `mayapy` 宿主下的最小回归验证，`MostComplexSampleSet/chr_mesh.smd`、`MostComplexSampleSet/vcaanim_VertexAnim.smd`、`Ellis/DMX/RAGDOLL.smd` 当前均可完成导入、导出、再导入闭环。
  - 2026-04-11：开始落地 DMX / SMD 共用 importer 策略层，已抽出统一 `SceneImportPolicy` 解析，并接通 `useSceneRoot=1`。当前 DMX 与 SMD 都支持跳过包装根直接导入到 Maya 场景根；当该模式与根旋转/轴向修正冲突时，当前实现优先保留 scene-root 导入并跳过相关修正。
  - 2026-04-11：统一新引入公共头的 include 约束，`ImportPolicy.h` 已改为 `<common/ImportPolicy.h>` 非相对引用形式；后续新增公共头引用时，避免继续使用 `../` 相对路径 include。
  - 2026-04-11：继续扩展 DMX / SMD 共用 importer 策略模型，已统一加入 `importMode(update/append/animationOnly)`、`importAnimationToLayer`、`animationLayerMode`、当前 Maya 名称空间捕获等解析入口；现阶段这些高阶模式已可被宿主识别并给出明确 warning，但对象合并与动画层实际行为仍待后续实现。
  - 2026-04-11：补上 `importMode=append` 的首批真实行为。DMX 与 SMD 现在都已支持“命中同名父级时复用已有层级节点，并避免重复创建已有 mesh carrier / mesh group”；其中 SMD 已通过 `mayapy` 连续两次导入 `MostComplexSampleSet/chr_mesh.smd` 验证顶层骨架与材质 mesh group 不再重复生成。当前 append 仍未覆盖权重合并、动画合并、名称空间冲突解算等更完整场景合并逻辑。
  - 2026-04-11：继续收紧 `importMode=append` 语义。当前 DMX 与 SMD 在复用已有层级节点时，已不会再回写这些已有节点的 bind pose / transform 动画；已通过 `mayapy` 验证二次 append 导入后，手工修改过的 `|pelvis.translateX` 与 `|joint_root.translateX` 仍被保留。当前尚未覆盖的仍是动画层写入、已有 skin/shape 精细合并、名称空间命中策略等更高阶合并行为。
  - 2026-04-11：整理新增 helper 的落点，已将 `append` 相关的新判定/查找逻辑从匿名 namespace 收回 `AnimationImporter`、`SmdSceneImporter` 等类内，优先复用类上下文并避免继续扩散文件级匿名 helper。
  - 2026-04-11：补上 `append` 的名称空间兼容。当前 DMX / SMD 在 `readNamespaceFromScene=1` 下，已可结合当前 Maya 名称空间命中 `ns:node` 形式的现有对象并继续复用；已通过 `mayapy` 验证 `|testns:joint_root`、`|testns:pelvis`、`|testns:tex_d_bmp_grp1` 等对象在二次 append 导入时不会重复生成。
  - 2026-04-11：继续推进 DMX `append` 的动画控制器合并。当前 [DmxImportAnimation.cpp](dcc_plugin/src/importer/DmxImportAnimation.cpp) 已支持在 `CreateCombinationControls()` 中复用已有 `*_controls` 节点、只补缺失控制属性、避免覆盖已有 control 值，并对 `importedControlPaths / importedScalarTargets` 做去重；已通过 `mayapy` 验证 [simple_blendshape_animation.dmx](dcc_plugin/samples/simple_blendshape_animation.dmx) 二次 `append` 尝试后场景内仍只有一个 `|combinationOperator_controls`，且手工改写的 `smile=0.75` 未被回退。当前新的明确阻塞点是：同一样例在第二次 `append` 时仍会在更后面的 blendShape / deformer 合并路径失败，说明后续需要继续实现 DMX 侧已有 blendShape/shape 的显式合并策略。
  - 2026-04-11：已继续收口 DMX `append` 的 blendShape / scalar animation 合并。当前 [DmxImportDeformers.cpp](dcc_plugin/src/importer/DmxImportDeformers.cpp) 会复用 mesh 上游已有同名 blendShape 节点、识别已有 alias target、只补缺失 target，并在 [DmxImportAnimation.cpp](dcc_plugin/src/importer/DmxImportAnimation.cpp) 中对 append 模式下已存在输入连接的标量 plug 跳过二次 animCurve 写入；已通过 `mayapy` 验证 [simple_blendshape_animation.dmx](dcc_plugin/samples/simple_blendshape_animation.dmx) 连续两次以 `useSceneRoot=1;importMode=append` 导入后不再报错，场景中仍只有一个 `|combinationOperator_controls` 和一个 `blendshapeAnimMeshShape_blendShape`，且手工改写的 `smile=0.75` 保持不变。本轮同时参考 Autodesk 官方 API 文档确认 `MFnDependencyNode::setName(name, createNamespace=false, ...)` 的名称空间语义在较新几代 Maya 中保持一致，因此 deformer 路径里相关 `setName` 调用已改为显式 `createNamespace=false`。
  - 2026-04-12：已把 `append` 专项门槛接入 [MayaBatchRegression.py](dcc_plugin/tools/MayaBatchRegression.py)。当前新增 `APPEND_GATE_EXPECTATIONS`，并在标准 roundtrip 之后对声明过的样例额外执行“双次 append 导入”检查，验证对象不重复生成且保留手工改值；已用 `mayapy` smoke 跑通 [simple_blendshape_animation.dmx](dcc_plugin/samples/simple_blendshape_animation.dmx) 与 [MostComplexSampleSet/chr_mesh.smd](dcc_plugin/samples/MostComplexSampleSet/chr_mesh.smd)。本轮同时修正了回归脚本的一个旧问题：每个导出变体现在都会重新导入原始样例再执行 `exportSelected`，不再依赖前一个 roundtrip 后残留的场景选择状态。
  - 2026-04-12：已收口 SMD importer 的一条 skin 权重 warning。当前 [SmdMeshImporter.cpp](dcc_plugin/src/importer_smd/SmdMeshImporter.cpp) 在 `setWeights()` 前会显式关闭 `maintainMaxInfluences`、临时关闭 `normalizeWeights`、并按实际顶点影响数设置 `maxInfluences`，避免 Maya 在写权重阶段提前裁剪影响；已通过 `mayapy` 验证 [MostComplexSampleSet/chr_mesh.smd](dcc_plugin/samples/MostComplexSampleSet/chr_mesh.smd) 的单次导入和 `useSceneRoot=1;importMode=append` 双次导入均不再出现 `Some weights could not be set to the specified value` warning。
  - 2026-04-12：继续把 `importMode=update` 和 `append` 拉开。当前 [DmxImportMesh.cpp](dcc_plugin/src/importer/DmxImportMesh.cpp) 与 [SmdMeshImporter.cpp](dcc_plugin/src/importer_smd/SmdMeshImporter.cpp) 在命中已存在 mesh transform / material group 时，`append` 仍保持“只复用、不覆盖”，而 `update` 已改为先清理旧 mesh 的 construction history 与已有 shape，再重建新的 mesh shape；这使 DMX / SMD 共用导入工作流至少在 mesh 主体层面具备了可预期的 overwrite 行为。已通过 `cmake --build dcc_plugin\build --config Release --target maya_dmx -- /m:1` 与 `maya_smd -- /m:1` 编译验证。当前仍未完成的是：已有 skinCluster / blendShape 的精细保留或就地更新、以及 `update` 模式下更细粒度的 deformer 合并语义。
  - 2026-04-12：继续收紧 DMX deformer 的 `append/update` 分界。[DmxImportDeformers.cpp](dcc_plugin/src/importer/DmxImportDeformers.cpp) 当前只在 `append` 下复用已存在的 blendShape 节点；`update` 模式下不再命中旧 blendShape，而是配合前面的 mesh/history 清理走“重建 mesh 后重新创建 blendShape”路径，避免出现 mesh 已覆盖但 deformer 仍沿用旧节点的半更新状态。已通过 `cmake --build dcc_plugin\build --config Release --target maya_dmx -- /m:1` 编译验证。当前剩余主缺口仍是：`update` 下 skinCluster / blendShape 的更细粒度增量更新与更完整回归样例。

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
