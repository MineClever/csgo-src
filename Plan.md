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
- 目标文件：[src/game/client/CMakeLists.txt](D:/_Code_Here/Git/csgo-src/src/game/client/CMakeLists.txt)
- 现象：
  - 当前 [build/temp_build_log.log](D:/_Code_Here/Git/csgo-src/build/temp_build_log.log) 中已经不再出现旧的 protobuf `_MSC_VER` 不匹配错误，说明源码构建 protobuf 已生效。
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
- 目标目录：[src/togl](D:/_Code_Here/Git/csgo-src/src/togl)
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
  - [RemoteServer.cpp](D:/_Code_Here/Git/csgo-src/src/tracker/AdminServer/RemoteServer.cpp)
  - [serverinfopanel.cpp](D:/_Code_Here/Git/csgo-src/src/tracker/AdminServer/serverinfopanel.cpp)
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
  - [src/utils/binlaunch](D:/_Code_Here/Git/csgo-src/src/utils/binlaunch)
  - [src/utils/vfont](D:/_Code_Here/Git/csgo-src/src/utils/vfont)
  - [src/utils/phonemeextractor](D:/_Code_Here/Git/csgo-src/src/utils/phonemeextractor)
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
  - [src/utils/hlfaceposer](D:/_Code_Here/Git/csgo-src/src/utils/hlfaceposer)
  - [src/hammer](D:/_Code_Here/Git/csgo-src/src/hammer)
  - [src/utils/FileSystemOpenDialog](D:/_Code_Here/Git/csgo-src/src/utils/FileSystemOpenDialog)
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
  - [src/vscript/languages/squirrel/vsquirrel](D:/_Code_Here/Git/csgo-src/src/vscript/languages/squirrel/vsquirrel)
  - [src/vscript/languages/python/vpython](D:/_Code_Here/Git/csgo-src/src/vscript/languages/python/vpython)
  - [src/vscript/languages/gm/vgm](D:/_Code_Here/Git/csgo-src/src/vscript/languages/gm/vgm)
  - [src/vscript/languages/lua/vlua](D:/_Code_Here/Git/csgo-src/src/vscript/languages/lua/vlua)
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
- 目标文件：[src/vgui2/vgui_perftest/vgui_perftest.cpp](D:/_Code_Here/Git/csgo-src/src/vgui2/vgui_perftest/vgui_perftest.cpp)
- 现象：
  - `vgui_perftest.cpp(38)` 无法包含 `console_logging.h`（`C1083`）。
- 可能原因：
  - include 路径丢失，或头文件路径在重构后发生变化。
- 后续步骤：
  - 查找目标头文件位置，恢复正确的 include 目录或更新 include 语句。

### 8. `temp_build_log.log` 是当前构建错误的主跟踪日志
- 状态：持续跟踪
- 优先级：低
- 目标文件：[build/temp_build_log.log](D:/_Code_Here/Git/csgo-src/build/temp_build_log.log)
- 现象：
  - 该日志记录了通过 [CmakeBuildSolution.bat](D:/_Code_Here/Git/csgo-src/CmakeBuildSolution.bat) 触发的最新完整构建错误，包括 `client_csgo`、`togl_togl`、`tracker_AdminServer`、多个工具目标和 VScript 后端的失败信息。
- 后续步骤：
  - 当构建行为发生变化时，优先重新生成这份日志，再更新本计划中的问题状态。
  - 后续调整优先级或关闭问题时，尽量引用这份日志中的最新错误作为依据。

## DCC / Maya 插件计划

### 9. 为 Maya 2022.5 开发 DMX 导入导出插件
- 状态：进行中
- 优先级：中
- 目标目录：[dcc_plugin](D:/_Code_Here/Git/csgo-src/dcc_plugin)
- 开发环境：
  - Maya 2022.5 DevKit：`D:\_Code_Here\Maya\Autodesk_Maya_2022_5_Update_DEVKIT_Windows\devkitBase`
  - Maya 2022.5 安装目录：`C:\Program Files\Autodesk\Maya2022`
  - Maya 默认宿主执行入口：`C:\Program Files\Autodesk\Maya2022\`（后续交互验证默认从该目录下调用 Maya 宿主程序）
  - 插件独立构建目录：`build\maya_dmx`

- 当前实际状态：
  - 工程与部署链已落地。插件采用独立 x64 CMake 工程构建，`cmake --build build\maya_dmx --config Release` 可生成 [maya_dmx.mll](D:/_Code_Here/Git/csgo-src/dcc_plugin/bin/Release/maya_dmx.mll)；[InstallPluginModuleToMaya.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/InstallPluginModuleToMaya.bat) 负责生成 module、清空 `maya_module` 载荷目录并复制最新 `.mll`、`.pdb` 与 MEL 脚本。
  - MEL 源码管理已统一。脚本真源位于 [src/mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel)，`maya_module/scripts` 只作为安装产物，不再手工维护双份脚本。
  - 公共 DMX 层已形成最小闭环。当前 [SimpleDmxText.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxText.cpp)、[SimpleDmxBinary.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxBinary.cpp)、[SimpleDmxWrite.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxWrite.cpp) 组成插件自用的最小 DOM/codec/write 层，已覆盖当前插件需要的 `string/int/float/bool/vector2/vector3/quaternion/vector4`、对应数组、`element` 与 `element_array`。
  - importer 已能稳定导入插件目标子集。当前 [DmxImportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/importer/DmxImportTranslator.cpp) 已支持文本 DMX、插件自用最小二进制 DMX、层级、transform、静态网格、骨架、skinCluster、deltaStates、多 UV、face-vertex normals、切线缓存、自定义 face set 材质恢复，以及按 `mayaBlendShapeNode` 分组的 blendShape 重建。
  - importer 的层级判定已进一步收紧。当前只会把显式 `DmeJoint` 创建成 Maya joint，不再因为元素出现在 `jointList` 中就把 `DmeDag` 强行提升成 joint；同时根节点的 `upAxis` 校正也会先查询 Maya 当前世界上方向，仅在源 DMX 上方向与宿主世界上方向不一致时才附加旋转修正。
  - importer 的蒙皮恢复已继续收紧。当前不再对 `jointCount == 1` 的刚性蒙皮做导入期短路，因此 `MostComplexSampleSet/chr_mesh` 中原先丢失 skinCluster 的 `hair_mesh`、`headRing_low`、`low_upper` 等单 influence 网格，导入后已经能恢复出实际 `skinCluster`。
  - importer 的建层级/建蒙皮顺序已调整为两阶段。当前 [DmxImportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/importer/DmxImportTranslator.cpp) 会先完整创建所有 `DmeDag/DmeJoint` 节点并填充 `importedDagPaths`，再第二阶段统一创建 mesh/skin；这修复了 `chr_mesh_body_bin.dmx` 一类“网格分支先于骨架分支出现”时导入后权重丢失的问题。
  - importer 的 skinCluster 写权重时序已进一步收紧。当前会在 `setWeights()` 前临时关闭 `maintainMaxInfluences` 与 `normalizeWeights`，写完后再恢复可用的 skinCluster 设置，已在 `chr_mesh_body_bin.dmx` 上压掉 `Some weights could not be set to the specified value. The weight total would have exceeded 1.0.` 这批 Maya 宿主警告，同时保留非零顶点权重。
  - exporter 已能稳定写出插件目标子集。当前 [DmxExportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/exporter/DmxExportTranslator.cpp) 已支持文本与最小二进制 DMX 写出，覆盖 DAG/joint/mesh/skin/blendShape 的最小模型链路，并写出 `positions`、`normals`、`textureCoordinates`、`texcoord$N`、`tangents`、`faceSets`、`jointList`、`jointCount`、`jointIndices`、`jointWeights`、`deltaStates`、`bindState`、`baseStates`、`currentState` 等核心字段。
  - 二进制 DMX 导入能力已显式验证。当前 importer 通过 [SimpleDmxText.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxText.cpp) 的 `ParseDocument()` 自动分流文本/二进制 DMX，`.dmx`、`.dmxb`、`.dmxbin` 三种扩展名都能被 translator 识别；已实际用 `mayapy` 成功导入 [simple_mesh.dmxb](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_mesh.dmxb) 并完成再导入验证。
  - bind pose 与 deformer 元数据已补到“最小可闭环”。skinCluster 侧当前已写入并回读 `mayaSkinClusterName`、`mayaSkinningMethod`、`mayaMaxInfluences`、`mayaMaintainMaxInfluences`、`mayaNormalizeWeights`、`mayaUseComponents`、`mayaGeomMatrix`、`mayaInfluencePaths`、`mayaBindPreMatrix`；blendShape 侧已支持 `mayaBlendShapeNode`、`mayaBlendShapeEnvelope`、`mayaBlendShapeOrigin`、`mayaWeightIndex`、`mayaTargetName` 等元数据。
  - 导出端已新增 metadata 裁剪选项。当前 [DmxExportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/exporter/DmxExportTranslator.cpp)、[performDmxExport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/performDmxExport.mel)、[mayaDmxTranslatorExport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/mayaDmxTranslatorExport.mel) 与 [MayaDmxWorkflow.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/workflow/MayaDmxWorkflow.cpp) 已支持 `exportMetadata` 开关；关闭后会裁掉 `maya*` 重建提示和 face set 材质 metadata，以缩减导出文件体积，同时保持基础 mesh/skin/delta 数据可回读。
  - 材质面集回建已切到 API。`AssignFaceSetMaterials()` 已不再拼 MEL，而是通过 `MFnSet`、`MDGModifier`、`MFnDependencyNode` 和 polygon component 直接构建/连接 shadingEngine 与基础 shader 图。
  - 工作流层已具备真实执行能力。当前 [MayaDmxWorkflow.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/workflow/MayaDmxWorkflow.cpp) 与 [MayaDmxWorkflowCommand.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/workflow/MayaDmxWorkflowCommand.cpp) 已支持导出预设保存/加载、`-exportPreset -filePath` 单次导出、batch manifest 保存/加载/列出/删除/运行；batch manifest 已从 `optionVar` 迁移到 Maya 用户目录下的 `maya_dmx_workflow/*.batch` 文件。
  - Maya 文件对话框的 file type specific options 已接通。当前 [PluginMain.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/plugin/PluginMain.cpp) 已为 `Valve DMX Import` 和 `Valve DMX Export` 注册 options script；[mayaDmxTranslatorExport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/mayaDmxTranslatorExport.mel) 与 [mayaDmxTranslatorImport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/mayaDmxTranslatorImport.mel) 已提供导入导出选项 UI 和默认选项串。
  - 导入端 option box 工作流已补齐。当前 [performDmxImport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/performDmxImport.mel)、[doDmxImportArgList.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/doDmxImportArgList.mel)、[mayaDmxTranslatorImport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/mayaDmxTranslatorImport.mel) 与 [DmxCreateUI.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/DmxCreateUI.mel) 已按导出端同构的 `OptionsUI / Setup / Callback / ArgList / Perform` 结构接通，file dialog 与 option box 现在共用同一套导入选项状态与 optionVar 持久化。
  - 交互 Maya 验证入口已固定。当前 [RunMayaInteractiveValidation.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/RunMayaInteractiveValidation.bat) 会默认从 `C:\Program Files\Autodesk\Maya2022\bin\maya.exe` 启动宿主，并通过 [MayaInteractiveValidation.py](D:/_Code_Here/Git/csgo-src/dcc_plugin/tools/MayaInteractiveValidation.py) 自动加载插件、source DMX MEL 脚本、校验 `MayaDmxShowImportOptions` / `MayaDmxShowExportSelectionOptions` / `MayaDmxShowExportAllOptions` 等入口并弹出验证窗口，减少手工交互验证时的环境漂移。
  - 交互 Maya 宿主验证已完成一次人工点检。当前已实际通过 [RunMayaInteractiveValidation.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/RunMayaInteractiveValidation.bat) 启动 Maya，确认验证窗口可见，导入/导出 option box 入口可正常打开，说明这套宿主内验证链路已可复用。
  - importer 的 influence 选择与临时 `maxInfluences` 已继续收紧。当前 [DmxImportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/importer/DmxImportTranslator.cpp) 会先按 `jointIndices` 实际引用子集筛选 influence，再按原始 `jointList` 下标映射回 Maya influence slot，避免 `jointList` 中存在未导入节点时权重错位；同时写权重前会按每顶点实际非零 influence 数设置临时 `maxInfluences`，以进一步压低复杂样例中的宿主告警面。
  - DMX 基础层已开始做边界拆分。当前 [SimpleDmxDocument.h](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxDocument.h) 已把 `Document/Element/Attribute` 这层公共 DOM 类型从 [SimpleDmxText.h](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxText.h) 里拆出，`SimpleDmxText/Binary/Write` 现在共享同一份文档模型头文件，为后续继续拆 codec / attribute typing / 未知字段保真做准备。
  - DMX 基础层的 attribute type 映射已开始统一。当前 [SimpleDmxTypes.h](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxTypes.h) 与 [SimpleDmxTypes.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxTypes.cpp) 已集中维护 declared type、binary type code、scalar/array 分类和 component count；[SimpleDmxText.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxText.cpp)、[SimpleDmxBinary.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxBinary.cpp) 与 [SimpleDmxWrite.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxWrite.cpp) 已切到这套公共映射，减少 text/binary/write 三处重复硬编码。
  - exporter 侧的 binary type 常量也已开始并到公共层。当前 [DmxExportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/exporter/DmxExportTranslator.cpp) 的 binary serializer 已不再手写 `kAttribute*` type code，而是改走 [SimpleDmxTypes.h](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxTypes.h) 的公共映射；已重新编译并通过 `simple_skinned_mesh` 的 sample tool 转换，以及 `simple_mesh` 的 `mayapy` roundtrip 回归。
  - 已重新对照 Valve 原始 datamodel / dmxloader。当前仓库内 [dmattributetypes.h](D:/_Code_Here/Git/csgo-src/src/public/datamodel/dmattributetypes.h)、[dmxattribute.cpp](D:/_Code_Here/Git/csgo-src/src/dmxloader/dmxattribute.cpp)、[dmxloader.cpp](D:/_Code_Here/Git/csgo-src/src/dmxloader/dmxloader.cpp) 与 [dmxloadertext.cpp](D:/_Code_Here/Git/csgo-src/src/dmxloader/dmxloadertext.cpp) 已确认 Valve canonical 类型名更偏向 `matrix` / `binary` / `matrix_array` / `binary_array`，binary type code 与 `DmAttributeType_t` 一一对应，text serializer 会按属性名排序写出。插件侧当前已把 [SimpleDmxTypes.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxTypes.cpp) 的 canonical 类型名切到 Valve 风格，同时保留 `vmatrix` / `void` 兼容别名；[SimpleDmxWrite.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxWrite.cpp) 也已改成稳定排序写出属性。
  - 公共 DMX codec 已补齐一轮 Valve 类型支持。当前 [SimpleDmxBinary.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxBinary.cpp) 与 [SimpleDmxWrite.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxWrite.cpp) 已支持 `time/color/qangle/matrix/binary` 及其数组类型的 binary 读写，并按 Valve 语义处理 `time` 的 4 位小数秒、`color` 的 RGBA 字节、`matrix` 的 16 float、`binary` 的十六进制文本块；新增样例 [simple_extended_types.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_extended_types.dmx) 已通过 `maya_dmx_sample_tool` 的 text -> binary -> text roundtrip 验证，`attributeOrder` 接入后再次复跑仍通过。
  - unknown field 与属性顺序保真已完成第一轮落地。当前 [SimpleDmxDocument.h](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxDocument.h) 已记录 `attributeOrder`；[SimpleDmxText.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxText.cpp) 会把未知 declared type 按实际 payload 解析成 string / string_array / inline element 并保留原始类型名；[SimpleDmxBinary.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxBinary.cpp) 与 [SimpleDmxWrite.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxWrite.cpp) 已在 text/binary read/write 路径上保留属性顺序，text writer 也已避免把 inline element 额外重复写成顶层元素。新增样例 [simple_unknown_order.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_unknown_order.dmx) 已通过 text -> text roundtrip，确认未知标量、未知数组、inline 自定义元素和原始属性顺序都能保留。
  - 已完成一轮 DMX 动画格式观察与宿主验证。当前对照 [dmattributetypes.h](D:/_Code_Here/Git/csgo-src/src/public/datamodel/dmattributetypes.h)、[movieobjects_compiletools.cpp](D:/_Code_Here/Git/csgo-src/src/public/movieobjects/movieobjects_compiletools.cpp)、[dmechannel.h](D:/_Code_Here/Git/csgo-src/src/public/movieobjects/dmechannel.h) 与 [dmelog.h](D:/_Code_Here/Git/csgo-src/src/public/movieobjects/dmelog.h) 可确认 Valve DMX 动画主干包含 `DmeAnimationList / DmeChannelsClip / DmeChannel / DmeTimeFrame / Dme*Log / Dme*LogLayer`；样例 [vcaanim_VertexAnim.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/MostComplexSampleSet/vcaanim_VertexAnim.dmx) 已实际转成 text 并用 `mayapy` 导入验证，当前插件只会导入其 `skeleton` 层级，不会消费 `animationList/channels/logs`，宿主里最终没有 `animCurve`、mesh、`skinCluster` 或 `blendShape` 节点，说明动画 DMX 目前仍属于未支持范围。
  - 已完成“蒙皮 + blendShape 同时存在”路径的现状评估。当前 importer 的 [DmxImportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/importer/DmxImportTranslator.cpp) 会先 `ApplyDeltaStates()` 再 `ApplySkinning()`，而 exporter 的 [DmxExportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/exporter/DmxExportTranslator.cpp) 会分别从 mesh 上游抓 `blendShape` 与 `skinCluster` 后写成 `deltaStates + jointList/jointWeights/jointIndices`；语义上具备“同一 mesh 同时导入/导出二者”的最小能力，但当前样例库里还没有一个同时包含 `deltaStates` 与 `jointWeights/jointIndices` 的回归样例，因此这一组合仍缺真实闭环验证。
  - 已补最小 `skin + deltaStates` 组合回归样例 [simple_skinned_blendshape.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_skinned_blendshape.dmx)，并把 [MayaBatchRegression.py](D:/_Code_Here/Git/csgo-src/dcc_plugin/tools/MayaBatchRegression.py) 扩到额外快照 `blendShape` 集合。当前用 `mayapy` 直接导入该样例时，宿主里已经能同时得到 mesh、`skinCluster` 和 `blendShape`，说明 importer 至少具备这条组合链路的最小建模能力。
  - 导出端 Alembic 风格 UI 已落地。当前 [performDmxExport.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/performDmxExport.mel)、[doDmxExportArgList.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/doDmxExportArgList.mel)、[DmxCreateUI.mel](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/mel/DmxCreateUI.mel) 已按 `OptionsUI / Init / Commit / Perform / ArgList` 模式组织，并桥接到 `mayaDmxWorkflow`。
  - 当前样例回归已经稳定。`simple_hierarchy`、`simple_blendshape`、`simple_mesh`、`simple_skinned_mesh`、`complex_chr_mesh`、`MostComplexSampleSet/chr_mesh` 六组样例已通过 `导入 -> 导出 text/binary -> 再导入 -> mesh diff` 闭环。
  - Maya roundtrip 回归已扩展到“类型稳定”检查。当前 [MayaBatchRegression.py](D:/_Code_Here/Git/csgo-src/dcc_plugin/tools/MayaBatchRegression.py) 除了 mesh diff，还会对导入根下的 `transform/joint` DAG 节点类型做快照并在 text/binary roundtrip 后比对，确保 `DmeDag`/`DmeJoint` 不会在导出再导入时漂移。
  - Maya roundtrip 回归已继续扩展到 `skincheck`。当前 [MayaBatchRegression.py](D:/_Code_Here/Git/csgo-src/dcc_plugin/tools/MayaBatchRegression.py) 会额外记录哪些 mesh 在原始导入后带有 `skinCluster`，并在 text/binary roundtrip 后确认这些 mesh 仍然保有 skin 绑定，用于捕获“几何不变但权重丢失”的问题。
  - Maya 回归脚本已支持直接吃二进制样例名。当前 [MayaBatchRegression.py](D:/_Code_Here/Git/csgo-src/dcc_plugin/tools/MayaBatchRegression.py) 除了默认解析 `case_name.dmx`，也支持直接传入 `.dmxb/.dmxbin` 样例名或在 `.dmx/.dmxb/.dmxbin` 之间自动探测输入文件，便于把二进制导入能力直接纳入 `mayapy` 回归。
  - 已实际用 `mayapy` 跑通六组样例的全量类型稳定回归，输出目录为 `build\\maya_dmx\\maya_batch_regression\\type_stability_check`；当前 mesh roundtrip 与 `transform/joint` 类型快照均通过，说明 `DmeDag` 被误读成 joint 的问题已在真实 Maya 宿主回归里收敛。
  - 已按第一优先级实际重跑复杂样例宿主回归。当前直接通过 `C:\Program Files\Autodesk\Maya2022\bin\mayapy.exe` 配合 `MAYA_SKIP_USERSETUP_PY=1` 跑了 `complex_chr_mesh`、`MostComplexSampleSet/chr_mesh`、`chr_mesh_body_bin.dmx` 三组样例；外部 `userSetup.py` 干扰已被排除，但三组样例仍都失败在 text roundtrip 后的 mesh point diff：`complex_chr_mesh` 报 `Object001Shape at vertex 0`，后两组都报 `arm_handCoverShape at vertex 0`。

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

- 已确认剩余问题：
  - 复杂样例导入时的 `weight total would have exceeded 1.0` 警告已在 `chr_mesh_body_bin.dmx` 上收敛，但还没有对所有复杂资产做全量重新回归，仍需确认这套 `setWeights()` 前后时序调整能否覆盖 `complex_chr_mesh`、`MostComplexSampleSet/chr_mesh` 等样例。
  - `MostComplexSampleSet/chr_mesh` 的“导入即丢蒙皮”问题已经在 importer 侧收敛，但新的 `skincheck` 回归确认 exporter 对复杂样例的 skin roundtrip 仍未完整保真：当前导出的 text/binary DMX 在再导入时会丢失一批使用 transform influence 的 skinCluster，说明 exporter 的 influence 注册 / `jointList` 组织仍需继续补。
  - `chr_mesh_body_bin.dmx` 直接导入后的蒙皮权重现在已能正确落到 Maya `skinCluster`，但针对该样例的 `mayapy` roundtrip 回归仍失败在 `arm_handCoverShape at vertex 0` 的点位失配，说明 exporter 对这类复杂 body 样例的几何/skin 保真仍未完全收口。
  - 2026-04-07 这轮通过 `C:\Program Files\Autodesk\Maya2022\bin\mayapy.exe` 的干净宿主回归再次确认：`complex_chr_mesh`、`MostComplexSampleSet/chr_mesh`、`chr_mesh_body_bin.dmx` 仍未通过第一优先级回归门槛，失败点分别为 `Object001Shape at vertex 0` 与 `arm_handCoverShape at vertex 0`；当前优先级已经从“先看 skinCluster 警告是否压平”收敛为“继续定位 exporter 在复杂 body / hand cover 样例上的几何或 skin roundtrip 漂移来源”。
  - 材质网络恢复深度仍不足。`AssignFaceSetMaterials()` 虽已改为 API 实现，但当前仍只覆盖基础 shader 图，尚未补 `place2dTexture`、utility 链、分层材质以及更完整的 Valve/Maya 材质语义映射。
  - `exportMetadata=0` 当前只裁掉 Maya 专用 metadata 和材质 inline metadata，不会改写核心 mesh/skin/delta 数据；如果后续希望进一步削减文件大小，还需要继续评估哪些非 `maya*` 字段也可选裁剪而不破坏回读。
  - `SimpleDmx*` 仍是插件定制层，不是通用 DMX DOM/codec。继续扩大 Valve DMX 兼容范围时，未知字段保真、顺序保真和引用语义都会成为重构阻力。
  - `SimpleDmx*` 虽然已经拆出公共 DOM 和 type 映射，exporter 侧 type code 也已切到公共映射，但 [DmxExportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/exporter/DmxExportTranslator.cpp) 仍保留一套独立的 binary serializer 结构；如果不继续把这层往公共 codec 收，后续补 unknown field / 顺序保真时仍会存在实现分叉。
  - 当前 `SimpleDmx*` 已能在 text 路径上保留 unknown field 与属性顺序，但“未知 declared type 的 text -> binary” 仍不成立：例如 [simple_unknown_order.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_unknown_order.dmx) 中的 `mystery_type` / `mystery_array` 目前仍会在 binary 写出阶段被拒绝，因为标准 binary DMX 没有可直接承载这类未知自定义类型名的 type code。
  - 未知 declared type 的 binary 保真目前不存在低成本“原样写回”方案。根据 [dmattributetypes.h](D:/_Code_Here/Git/csgo-src/src/public/datamodel/dmattributetypes.h) 的 `DmAttributeType_t`，binary DMX 只有固定 type code 集，没有用于任意自定义 declared type 名的扩展槽位；因此现阶段如果继续要求 Valve 兼容的 binary DMX，未知类型只能选择显式报错、导出时强制回退 text、或引入插件私有旁带保真方案，不能像 text DMX 那样无损直接写出。
  - [vcaanim_VertexAnim.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/MostComplexSampleSet/vcaanim_VertexAnim.dmx) 并不是“带蒙皮和 BlendShape 的模型样例”，而是纯 `skeleton + channels/logs` 动画样例；当前它更适合作为动画支持回归入口，而不是 `skin + blendShape` 组合回归入口。
  - “蒙皮 + blendShape 同时存在” 仍有两个具体风险点：一是 importer 原先按 `blendShape -> skinCluster` 顺序建历史链，已被新样例直接打出 `skinCluster API creation failed`，当前 [DmxImportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/importer/DmxImportTranslator.cpp) 已改成先 `ApplySkinning()` 再 `ApplyDeltaStates()`；二是 exporter 目前在 [simple_skinned_blendshape.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_skinned_blendshape.dmx) 的宿主回归里仍会漏掉 `deltaStates`，导出的 `.dmx` 只保留 `jointWeights/jointIndices` 而没有 `DmeVertexDeltaData`，导致新的 `blendShape` 回归检查失败。
  - 旧版错误编码产生的历史 batch 文件如果残留在 Maya 用户目录里，`listBatches` 仍可能显示脏名称；新格式已可用，但还缺一次性清理或文件头校验。

- 下一阶段计划：
  - 第一优先级：
    - 针对 `complex_chr_mesh/Object001Shape` 与 `MostComplexSampleSet/chr_mesh`、`chr_mesh_body_bin.dmx` 中 `arm_handCoverShape` 的顶点漂移，继续定位 exporter 在复杂 body 样例上的几何/skin roundtrip 失真来源，并补新的对比日志或最小复现。
    - 继续处理 [simple_skinned_blendshape.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_skinned_blendshape.dmx) 暴露出来的 exporter 缺陷：当前宿主导出后会丢掉 `deltaStates`，需要先定位 `DmxExportTranslator.cpp` 在 `skin + blendShape` 组合 history 下为什么没把 `blendShape` target 提取成 `DmeVertexDeltaData`。
  - 第二优先级：
    - 继续补材质网络，扩到 `place2dTexture`、utility 节点链、更多 shader 类型和更稳定的贴图路径还原。
    - 继续补组合型面部控制器和更完整 deformer / rig 元数据，逐步接近 Valve 角色资产的面部工作流。
    - 为 workflow 增加旧 batch 文件清理、格式校验和更明确的错误提示。
  - 中期重构方向：
    - 在 [SimpleDmxDocument.h](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxDocument.h) 与 [SimpleDmxTypes.h](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxTypes.h) 这一层稳定后，继续把 `SimpleDmx*` 从插件定制实现逐步拆成更通用的 DOM/codec 层，优先补完整 attribute type、未知字段保真和 text/binary 对称。
    - 在 text 路径的 unknown field / 顺序保真已落地后，继续评估未知 declared type 的 text -> binary 降级策略、旁带保真或显式能力边界，避免 binary exporter 对自定义类型直接硬失败。
    - 在 type code 已统一后，继续把 exporter 私有 serializer 往公共 codec 收，减少 `SimpleDmx*` 与 [DmxExportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/exporter/DmxExportTranslator.cpp) 的双份维护。
    - 为动画 DMX 单独补 importer/exporter 路线图，优先支持 `DmeAnimationList / DmeChannelsClip / DmeChannel / DmeTimeFrame / DmeVector3Log / DmeQuaternionLog` 这条骨骼变换动画主干，并明确它与现有静态 mesh / skin translator 的关系。
    - 为“同一 mesh 同时带 skin + deltaStates”补一个最小回归样例和宿主验证脚本，先确认 importer 的 deformer 顺序是否正确，再决定是否需要改成 `skinCluster -> blendShape` 或保留当前顺序并记录兼容边界。
    - 在通用 DMX 层稳定后，再扩主干类型覆盖范围，最后再评估更大范围的 Valve DMX rig / animation 兼容。

## 环境与工具链说明

### A. 批处理构建包装脚本已做兼容性修复
- 状态：已完成
- 结果：
  - [CmakeBuildSolution.bat](D:/_Code_Here/Git/csgo-src/CmakeBuildSolution.bat) 现在会在构建前重新执行 CMake 配置，并避免使用当前环境下会导致早退的 `--parallel` 调用方式。

### B. 已接入 protobuf 源码构建目标
- 状态：已完成
- 结果：
  - 已新增 [src/thirdparty/protobuf-2.5.0/CMakeLists.txt](D:/_Code_Here/Git/csgo-src/src/thirdparty/protobuf-2.5.0/CMakeLists.txt)，从仓库内 protobuf 2.5.0 源码直接构建 `libprotobuf`。
  - 已通过 [src/thirdparty/CMakeLists.txt](D:/_Code_Here/Git/csgo-src/src/thirdparty/CMakeLists.txt) 注册，并在 [src/CMakeLists.txt](D:/_Code_Here/Git/csgo-src/src/CMakeLists.txt) 中提前 thirdparty 注册顺序，使 `prebuilt::libprotobuf` 在消费者配置前优先绑定到源码构建目标。
  - 已通过 `cmake --build build --config Release --target valve_libprotobuf` 验证该目标可以在 VS2022/x86 下独立构建，并输出 `src/lib/public/x86/libprotobuf.lib`。
  - 已为 protobuf 2.5.0 增加 `_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS`，解决现代 MSVC 对 `<hash_map>` 的报错。

### C. 配置阶段发现的可选依赖现状
- 状态：未解决
- 说明：
  - 当前 x86 工具链下未检测到可用 Autodesk FBX SDK，因此自动回退到 OpenFBX。
  - `utils/studiomdl` 缺少 `libedgegeomtool`，当前会以无 edgegeom 支持模式构建。
  - 当前机器/工具链未安装 MFC，因此影响若干工具目标，详见上面的 MFC 相关问题。
