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
  - Maya 2022.5 DevKit 路径：`D:\_Code_Here\Maya\Autodesk_Maya_2022_5_Update_DEVKIT_Windows\devkitBase`
  - Maya 2022.5 安装路径：`C:\Program Files\Autodesk\Maya2022`
  - 插件工程输出目录：`D:\_Code_Here\Git\csgo-src\dcc_plugin`
- 参考实现：
  - `D:\_Code_Here\Maya\Autodesk_Maya_2022_5_Update_DEVKIT_Windows\devkitBase\devkit\plug-ins\AbcImport`
  - `D:\_Code_Here\Maya\Autodesk_Maya_2022_5_Update_DEVKIT_Windows\devkitBase\devkit\plug-ins\AbcExport`
- 目标说明：
  - 参考 Maya DevKit 中 `AbcImport` / `AbcExport` 的工程组织、命令注册方式、文件翻译器实现和构建方法，为 [src/datamodel](D:/_Code_Here/Git/csgo-src/src/datamodel) 对应的数据结构设计并实现 Maya 的 DMX 导入插件与导出插件。
- 当前已确认现状：
  - [dcc_plugin](D:/_Code_Here/Git/csgo-src/dcc_plugin) 已建立初始工程骨架，当前已包含公共层、导入层、导出层和插件入口层。
  - 仓库内可直接复用的 DMX 核心链路主要位于 [src/datamodel](D:/_Code_Here/Git/csgo-src/src/datamodel)、[src/dmxloader](D:/_Code_Here/Git/csgo-src/src/dmxloader)、[src/dmserializers](D:/_Code_Here/Git/csgo-src/src/dmserializers)。
  - [src/utils/studiomdl/dmxsupport.cpp](D:/_Code_Here/Git/csgo-src/src/utils/studiomdl/dmxsupport.cpp) 已包含较完整的 DMX 模型装载逻辑，可作为 Maya 导入映射与数据字段梳理的重要参考，而不应重复发明一套 DMX 解释器。
  - DCC 插件构建策略更新为优先使用 x64 工具链，以匹配 Maya 2022.5 Windows 运行时；不再以仓库主工程的 Win32 / x86 约束反向限制插件目标。
- 实施范围：
  - 第一阶段只做模型向导入/导出最小闭环，不包含材质网络完整还原、动画剪辑批处理、复杂约束、粒子、灯光、摄像机和 Valve 专用编辑器元数据。
  - 最小可用功能以静态网格、骨架、蒙皮权重、基础 transform、mesh 名称、材质槽位、基础 shape key / delta state 为核心。
  - 插件形态采用两个 Maya 文件翻译器或一个插件内注册两个 translator，分别负责 DMX 导入与导出；公共逻辑放在共享静态库或共享源码层，避免导入导出各自复制一份转换代码。
- 目标拆分：
  - 阶段 1，工程落地：
    - 在 `dcc_plugin` 下建立 `maya_dmx_common`、`maya_dmx_import`、`maya_dmx_export` 三层结构。
    - 设计独立 CMake 入口，并通过仓库主 CMake 增加可选开关，例如 `BUILD_MAYA_DMX_PLUGIN`，未提供 DevKit 时默认关闭，不影响现有 Win32 游戏/工具链构建。
    - 封装 Maya DevKit 检测、include/lib 路径、`.mll` 输出目录和运行时复制规则。
  - 阶段 2，DMX 读取与场景映射：
    - 先打通 DMX 文件加载、根元素识别、骨架层级遍历、网格顶点/索引提取、材质名读取。
    - 在 Maya 侧最先落地 `MFnMesh`、`MFnSkinCluster`、`MFnTransform`、`MFnIkJoint` 对应的最小对象创建路径。
    - 明确坐标系、单位、旋转顺序和骨骼朝向转换规则，统一放入公共转换模块，避免导入导出规则分散。
  - 阶段 3，DMX 写出：
    - 从 Maya DAG 收集 transform、joint、mesh、skin cluster、blend shape 数据，生成最小可保存的 DMX 层次。
    - 复用 `datamodel` / `dmserializers` 直接写出合法 DMX，优先兼容仓库内 `studiomdl` / `dmxloader` 的消费路径。
    - 增加导出选项：坐标系、是否导出蒙皮、是否导出 blend shape、是否写出材质槽位。
  - 阶段 4，验证与样例：
    - 建立一组最小样例资产，覆盖静态模型、单骨骼蒙皮、多人骨架、含 delta state 的面部模型。
    - 验证链路至少包括 `Maya -> DMX -> studiomdl/dmxloader` 与 `现有 DMX -> Maya -> 再导出 DMX` 两条。
    - 为导入导出失败场景补日志与诊断信息，避免只返回 Maya 通用报错。
  - 阶段 5，工作流完善：
    - 参考 Blender Source Tools 的完整产品形态，把当前“单次文件导入导出”继续扩展为“场景级配置 + 批量任务 + 编译联动”的工作流。
    - 为 Maya 侧补持久化导出配置，包括导出根目录、游戏路径、材质相对路径、DMX 编码版本、坐标轴、是否导出蒙皮、是否导出 delta state 等，避免每次导出重新手填。
    - 设计批量导出清单或导出集概念，对齐 Blender Source Tools 里对象 / Collection 导出列表的思路，让多个角色、LOD 或拆件可以在一次操作中稳定输出。
    - 该阶段只保留 Maya 内部的导出配置管理与批量导出编排，不再包含 QC / studiomdl 联动、自动编译或日志回传范围。
  - 阶段 6，动画与面部系统扩展：
    - 参考 Blender Source Tools 对 action / 动画槽 / shape key 的导出组织方式，规划 Maya 侧 animation clip、骨骼动画、vertex animation、blendShape 批量导出的统一入口。
    - 把当前仅支持最小位置 delta 的 `deltaStates` 继续扩展到更接近 Valve 角色资产的面部工作流，至少补足 target 过滤、命名稳定性、批量导出和回归样例。
    - 评估是否需要引入“隐式根骨骼 / zero bone”“旧旋转兼容”等可选导出策略，减少与旧版 Source 资产链路的姿态偏差。
- 模块设计：
  - `maya_dmx_common`：
    - 负责坐标系转换、字符串/路径转换、DMX 元素查找、名称规范化、错误日志桥接、导入导出共享选项结构。
  - `maya_dmx_import`：
    - 负责 Maya `MPxFileTranslator` 导入入口、文件选项解析、场景对象创建、DMX 到 Maya 的数据映射。
  - `maya_dmx_export`：
    - 负责 Maya `MPxFileTranslator` 导出入口、选择集遍历、Maya 到 DMX 的数据采集与写出。
  - `maya_dmx_workflow`：
    - 负责场景级导出配置、批量导出清单、最近使用参数和 Maya 内部工作流状态管理，不负责外部 QC / 编译工具调用。
- 数据映射约束：
  - 坐标系必须显式定义 Valve 与 Maya 之间的轴向映射，禁止在不同模块内各自硬编码。
  - 骨骼命名、父子层级、bind pose、蒙皮权重精度需要以 `studiomdl` 可消费为准，而不是只满足 Maya 内可见效果。
  - blend shape / delta state 的命名和顺序需要保持稳定，避免回写后破坏下游面部动画控制器。
  - 材质先只保留槽位和材质名，不在第一阶段尝试恢复完整 Hypershade 网络。
  - 动画片段、shape key、delta state 的导出命名必须稳定且可批量复现，避免同一 Maya 场景重复导出后文件名、元素名、控制器名漂移。
  - 需要为历史资产兼容预留导出选项，但兼容策略必须集中管理，不能散落在 importer / exporter 各处硬编码。
- 验收标准：
  - 能在 Maya 2022.5 中成功加载 `.mll`，并出现可用的 DMX 导入与导出入口。
  - 能导入至少一份仓库现有 DMX 模型并在 Maya 中看到正确层级、网格和基础蒙皮结果。
  - 能从 Maya 导出一份最小骨架网格 DMX，并被仓库内 DMX 读取链路接受。
  - 插件构建默认对未安装 Maya DevKit 的开发者无侵入，不破坏主工程配置与构建。
- 主要风险：
  - Maya 2022.5 官方插件 ABI 为 x64，和仓库现有主工程 Win32 构建体系分离后，跨模块直接复用现有库时需要谨慎处理位数与二进制兼容边界。
  - `datamodel` / `dmserializers` 是否能直接无修改地嵌入 Maya 插件进程尚未验证，可能需要补一层更轻的适配封装以避免依赖过重。
  - 现有 DMX 字段并不都适合直接映射为 Maya 原生节点属性，需要先约束最小支持子集，再逐步扩展。
- 近期执行顺序：
  - 先以 x64 工具链确认 Maya 2022.5 插件最小构建方式、运行库与输出目录规则。
  - 再建立 `dcc_plugin` 工程骨架和可选 CMake 开关，确保空实现插件可以编译输出 `.mll`。
  - 之后实现导入最小闭环，再实现导出最小闭环，最后补样例与回归验证。
- 当前进展：
  - 已建立 [dcc_plugin/CMakeLists.txt](D:/_Code_Here/Git/csgo-src/dcc_plugin/CMakeLists.txt) 与 [dcc_plugin/cmake/MayaPluginSupport.cmake](D:/_Code_Here/Git/csgo-src/dcc_plugin/cmake/MayaPluginSupport.cmake)，用于独立配置 Maya SDK、x64 优先策略和 `.mll` 输出规则。
  - 已建立 [dcc_plugin/src/common](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common)、[dcc_plugin/src/importer](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/importer)、[dcc_plugin/src/exporter](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/exporter)、[dcc_plugin/src/plugin](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/plugin) 骨架，并注册了导入 / 导出 translator 空实现。
  - 已在仓库根 [CMakeLists.txt](D:/_Code_Here/Git/csgo-src/CMakeLists.txt) 增加 `BUILD_MAYA_DMX_PLUGIN` 可选开关，但该插件仍建议通过独立 x64 配置使用，避免与主仓库 Win32 目标混淆。
  - 已通过 `cmake -S dcc_plugin -B build\\maya_dmx -A x64` 与 `cmake --build build\\maya_dmx --config Release` 验证骨架可以生成 [dcc_plugin/bin/Release/maya_dmx.mll](D:/_Code_Here/Git/csgo-src/dcc_plugin/bin/Release/maya_dmx.mll)。
  - 已新增 [dcc_plugin/CreatePluginSolution.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/CreatePluginSolution.bat) 与 [dcc_plugin/BuildPlugin.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/BuildPlugin.bat)，用于独立生成 x64 工程和快速构建插件。
  - 已新增 [dcc_plugin/InstallPluginModuleToMaya.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/InstallPluginModuleToMaya.bat)，按 `C:\Users\chnis\Documents\maya\modules` 现有风格生成 `.mod` 文件，并把插件拷贝到仓库内的 `maya_module/plug-ins/windows/2022` 目录作为 module 载荷。
  - 已新增 [dcc_plugin/src/common/SimpleDmxText.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxText.cpp) 与 [dcc_plugin/src/common/SimpleDmxText.h](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxText.h)，实现文本 DMX 最小解析器，当前覆盖 `DmeModel`、`DmeDag`、`DmeJoint`、`DmeTransform`、`children`、`jointList`、`position`、`orientation` 等常见字段。
  - [dcc_plugin/src/importer/DmxImportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/importer/DmxImportTranslator.cpp) 已从纯 stub 升级为可导入文本 DMX 层级、局部变换和基础 polygon mesh，当前会在 Maya 中创建 transform / joint 层级，并支持 `shape -> DmeMesh -> bindState/baseStates -> positions/positionsIndices -> faceSets/faces` 这条最小网格路径；同时已支持主 UV 通道 `textureCoordinates/textureCoordinatesIndices`、face-vertex normals `normals/normalsIndices`，以及基于 `jointList + jointCount + jointWeights + jointIndices` 的基础 skinCluster 权重导入；对 `upAxis = Z` 的模型会添加一层简单的 X 轴校正。
  - importer 已补上最小材质槽位恢复：当前会按 `DmeFaceSet.name` 创建或复用 Maya shading group，并把对应 polygon 范围重新分配到这些面集上，用于保留基础材质槽位组织。
  - importer 已补上最小二进制 DMX 读取：当前会自动识别 `<!-- dmx encoding binary 5 ... -->` 头，并解析插件当前使用的最小属性子集，包括 `string/int/float/bool/vector2/vector3/quaternion`、对应数组、`element`、`element_array`，从而让现有层级、mesh、UV、normals、skin、faceSet 导入路径也能处理二进制 DMX。
  - importer 已补上最小 `deltaStates` 支持：当前会读取 `DmeMesh.deltaStates` 中的 `DmeVertexDeltaData` 位置 delta，并在 Maya 中为基础 mesh 创建对应的 blendShape target；现阶段仅覆盖位置形变，不处理组合器、法线 delta、wrinkle、balance/speed 等高级字段。
  - 已新增 [dcc_plugin/samples/simple_hierarchy.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_hierarchy.dmx) 作为最小手工验证样例，用于在 Maya 2022.5 中快速验证 importer。
  - 已新增 [dcc_plugin/samples/simple_blendshape.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_blendshape.dmx) 与 [dcc_plugin/samples/simple_blendshape.dmxb](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_blendshape.dmxb)，用于验证最小位置 delta / blendShape 导入链路以及 text/binary 样例转换。
  - 已新增 [dcc_plugin/samples/simple_mesh.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_mesh.dmx) 作为最小网格导入样例，用于验证基础 mesh 创建路径。
  - 已新增 [dcc_plugin/samples/simple_skinned_mesh.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_skinned_mesh.dmx) 作为最小蒙皮导入样例，用于验证 `jointList` 与 skin weights 导入路径。
  - 已新增 [dcc_plugin/tools/DmxSampleTool.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/tools/DmxSampleTool.cpp) 与 [dcc_plugin/tools/CMakeLists.txt](D:/_Code_Here/Git/csgo-src/dcc_plugin/tools/CMakeLists.txt)，生成独立命令行工具 [dcc_plugin/bin/Release/maya_dmx_sample_tool.exe](D:/_Code_Here/Git/csgo-src/dcc_plugin/bin/Release/maya_dmx_sample_tool.exe)，用于在当前最小支持子集内执行 text/binary DMX 样例互转，减少 Maya 内手工回归成本。
  - 已新增 [dcc_plugin/RunSampleRegression.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/RunSampleRegression.bat) 与 CMake 自定义目标 `maya_dmx_sample_regression`，会统一跑 `simple_hierarchy`、`simple_blendshape`、`simple_mesh`、`simple_skinned_mesh`、`complex_chr_mesh` 五份文本样例的 `text -> binary -> text` 转换回归。
  - 已新增 [dcc_plugin/src/common/SimpleDmxWrite.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxWrite.cpp) 与 [dcc_plugin/src/common/SimpleDmxWrite.h](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxWrite.h)，把当前最小 DMX 文本/二进制写出逻辑下沉到公共层，供 exporter 与样例工具复用。
  - [dcc_plugin/src/exporter/DmxExportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/exporter/DmxExportTranslator.cpp) 已从 stub 升级为最小文本 DMX exporter，可从当前场景或活动选择集中收集 DAG / joint / mesh，写出 `DmeModel -> DmeDag/DmeJoint -> DmeTransform/DmeMesh` 的基础结构，并覆盖 `positions`、`positionsIndices`、`normals`、`normalsIndices`、`textureCoordinates`、`textureCoordinatesIndices`、`faceSets/faces`。
  - exporter 已进一步补上完整导出骨架的 `jointList` 以及基础蒙皮字段 `jointCount`、`jointIndices`、`jointWeights`，当前会从 Maya `skinCluster` 收集导出层级内的 influence，并按顶点写出固定槽位权重布局，以便当前 importer 可以回读。
  - exporter 已补上最小材质槽位写出：当前会从 Maya mesh 连接的 shading group 收集 polygon 分配，并按集合名称写出多个 `DmeFaceSet`，以保留基础材质槽位信息。
  - exporter 已补上最小二进制 DMX 写出：当前支持通过 `.dmxb` / `.dmxbin` 扩展名，或 translator 选项中的 `binary=1` / `encoding=binary` 输出二进制 DMX；二进制写出覆盖插件当前已支持的最小属性子集，以保证 importer 可以回读。
  - exporter 已补上最小 `deltaStates` 写出：当前会从 mesh 上游的 Maya `blendShape` 节点收集基础 target，并把每个 target 相对基础 mesh 的位置差写成 `DmeVertexDeltaData`；现阶段只覆盖位置 delta，不处理组合器、法线 delta、wrinkle、balance/speed 等高级形变字段。
  - 已再次通过 `cmake --build build\\maya_dmx --config Release` 验证 importer + exporter 的当前 x64 插件工程可以成功生成 [dcc_plugin/bin/Release/maya_dmx.mll](D:/_Code_Here/Git/csgo-src/dcc_plugin/bin/Release/maya_dmx.mll)。
  - 已用 `maya_dmx_sample_tool.exe` 从现有文本样例生成 [dcc_plugin/samples/simple_hierarchy.dmxb](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_hierarchy.dmxb)、[dcc_plugin/samples/simple_mesh.dmxb](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_mesh.dmxb)、[dcc_plugin/samples/simple_skinned_mesh.dmxb](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/simple_skinned_mesh.dmxb)，并额外验证 `simple_mesh.dmxb -> build/simple_mesh_roundtrip.dmx` 的最小双向转换链路可运行。
  - 已将你提供的真实样例 [dcc_plugin/samples/complex_chr_mesh.dmx](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/complex_chr_mesh.dmx) 纳入测试范围；当前最小工具链已可完成 `complex_chr_mesh.dmx -> complex_chr_mesh.dmxb` 与 `complex_chr_mesh.dmx -> build/complex_chr_mesh_roundtrip_text.dmx`，说明这份真实资产至少落在现阶段 text/binary 最小支持子集内。
  - 已通过 `cmake --build build\\maya_dmx --config Release --target maya_dmx_sample_regression` 验证样例回归目标可运行，当前会在 `build\\maya_dmx\\sample_regression\\Release` 下生成各样例的二进制文件和 roundtrip 文本文件，并已覆盖 `simple_blendshape` 的最小 delta 样例。
  - 已新增 [dcc_plugin/tools/MayaBatchRegression.py](D:/_Code_Here/Git/csgo-src/dcc_plugin/tools/MayaBatchRegression.py) 与 [dcc_plugin/RunMayaBatchRegression.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/RunMayaBatchRegression.bat)，可通过 Maya 2022 自带 `mayapy.exe` 在独立进程中执行 `导入样例 DMX -> 导出文本 DMX -> 导出二进制 DMX` 的最小 Maya 内闭环回归，当前默认覆盖 `simple_hierarchy`、`simple_blendshape`、`simple_mesh`、`simple_skinned_mesh`、`complex_chr_mesh` 五份样例。
  - 参考 `sourcepp` 仓库当前对 KeyValues / DMX 的分层思路后，后续计划调整为继续弱化 [dcc_plugin/src/common/SimpleDmxText.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxText.cpp)、[dcc_plugin/src/common/SimpleDmxBinary.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxBinary.cpp)、[dcc_plugin/src/common/SimpleDmxWrite.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/common/SimpleDmxWrite.cpp) 之间的耦合，把当前实现逐步拆成 `值/元素 DOM 层`、`text/binary codec 层`、`Maya 适配层` 三层，避免 importer、exporter、样例工具各自继续堆专用分支。
  - 结合 `sourcepp` 文档中 `KV1ElementBase + Readable/Writable` 这类读写职责分离设计，后续会把当前最小 DMX 支持子集整理成更稳定的公共数据模型：解析器负责尽量完整保留字段与元素引用，Maya 层只消费已归一化的 DOM 视图；这一步属于计划更新项，尚未开始大规模重构。
  - 已参考 Blender Source Tools 当前公开实现与文档说明，确认其成熟工作流不仅覆盖 SMD / DMX 导入导出，还包含场景持久化导出参数、批量导出列表、QC 导入 / 编译联动、动画与 shape key 选择策略、DMX 编码版本切换等产品级功能；据此已把 Maya 插件后续计划从“单文件 translator”扩展为“translator + workflow 层”的两层目标。
  - 结合当前范围收敛，已明确把 `QC / studiomdl` 联动、自动编译和日志回传从 Maya 插件计划中移除；后续即使参考 Blender Source Tools 的产品形态，也只吸收导出配置持久化、批量导出组织、动画/shape key 管理等 DCC 内部工作流能力。
  - 已新增 [dcc_plugin/src/workflow](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/workflow) 工作流层骨架，并在插件里注册 `mayaDmxWorkflow` 命令；当前已支持通过 Maya `optionVar` 持久化保存 / 读取 / 删除 / 列出导出预设，以及保存 / 读取 / 删除 / 列出批量导出清单，为后续真正的批量导出执行器提供稳定数据入口。
  - 已通过 `cmake --build build\\maya_dmx --config Release` 再次验证新增工作流层后插件仍可成功生成 [dcc_plugin/bin/Release/maya_dmx.mll](D:/_Code_Here/Git/csgo-src/dcc_plugin/bin/Release/maya_dmx.mll)。
  - 已处理一条真实宿主兼容问题：在 Maya 内执行 DMX 导出时，原先 `kExportActiveAccessMode` 路径直接调用 `MGlobal::getActiveSelectionList()`，会在包含 UFE 项或无效宿主路径的场景下触发 `Ufe_v2::InvalidPath` 异常。当前已把导出根收集改为通过 `ls -sl -long` 文本路径回收再逐项解析 DAG，并在 exporter writer 顶层增加 C++ 异常兜底，避免这类宿主异常直接打断导出流程。
  - 已进一步定位并修复一条更关键的导出崩溃根因：`DmxExportTranslator` 内部 `DmxTextBuilder` 原先使用 `std::vector<DmxElement>` 保存元素并向外暴露内部对象指针，后续继续创建元素时会因为 `vector` 扩容使已挂接到属性树中的元素指针全部悬空，导致 Maya 导出阶段表现为宿主侧异常甚至误报 `Ufe_v2::InvalidPath`。当前已改为稳定地址存储，最小 `simple_hierarchy` 与 `simple_mesh` 样例在 `mayapy` 下均可完成 `导入 -> 文本导出 -> 二进制导出`。
  - 当前导出诊断日志已临时改写到系统临时目录 `%TEMP%\\maya_dmx_export_debug.log`，避免继续向仓库目录写入调试垃圾；待 Maya 内闭环稳定后再移除或收敛这类临时日志。
  - 已继续用 `mayapy` 跑 Maya 内回归，并补齐 importer 剩余兼容问题：`simple_blendshape` 现已改为通过 `MFnBlendShapeDeformer` API 创建目标，不再依赖不稳定的 `blendShape` 命令字符串；`simple_skinned_mesh` 现已支持在 `skinCluster` 命令返回失败但实际节点已创建的情况下回退到 mesh history 查找 skinCluster 并继续写权重；`complex_chr_mesh` 现已可完成 `导入 -> 文本导出 -> 二进制导出`，二进制写出额外放宽了不完整向量数组条目的容错，改为缺失分量补零。
  - 已通过 `mayapy` 完整验证五份样例 `simple_hierarchy`、`simple_blendshape`、`simple_mesh`、`simple_skinned_mesh`、`complex_chr_mesh` 的 Maya 内闭环回归，输出位于 `build\\maya_dmx\\maya_batch_regression\\full_run_2`；这意味着当前 `RunMayaBatchRegression` 覆盖范围内，文本与二进制导出路径均已可运行。
  - importer 的 skinCluster 处理已进一步转向 OpenMaya API：当前创建阶段不再完全依赖 `skinCluster` MEL 命令，而是通过 OpenMaya 创建 `skinCluster` 节点、复制并挂接 intermediate 原始 mesh、连接 `matrix / bindPreMatrix / geomMatrix / inputGeometry / outputGeometry` 等关键属性，再由 `MFnSkinCluster::setWeights` 写入权重；这使 skin 处理主干正式切到 API 层，后续只剩复杂资产上的细节兼容需要继续收敛。
  - [dcc_plugin/tools/MayaBatchRegression.py](D:/_Code_Here/Git/csgo-src/dcc_plugin/tools/MayaBatchRegression.py) 与 [dcc_plugin/RunMayaBatchRegression.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/RunMayaBatchRegression.bat) 已扩展为完整 roundtrip 回归：当前流程会对每个样例执行 `导入 -> 导出 text/binary -> 分别再导入 -> 比较 mesh 拓扑与顶点数据`，并为 text/binary roundtrip 各自写出 meshcheck 标记文件。
  - 已继续收敛复杂资产上的 skin weight 警告：当前 importer 改为对 skinCluster 全部 influence 显式写零值基线、累加 DMX 权重并按顶点归一化，再统一写回；同时在创建 skinCluster 前尝试解锁 joint 的 `liw`，并关闭 cluster 的 `maintainMaxInfluences/normalizeWeights`。这一步已经去掉了 `Some weights could not be set to the specified value. The weight total would have exceeded 1.0.` 这类权重总量超限警告，但 `complex_chr_mesh` 仍保留 `Locked influences prevented enforcement of the max influences on some points.`，说明剩余问题主要集中在 Maya 创建 skinCluster 时的 influence 锁或默认约束策略。
  - 已继续补 exporter / importer 的能力边界并重新跑 `complex_chr_mesh` 的单样例 Maya 闭环：exporter 现已改为“显式 polygon component 优先、整物体材质分配延后、未覆盖 polygon 追加 `default_faces` 兜底”的 `faceSets` 写出策略，从而修掉复杂资产 roundtrip 中因 shading group 返回不完整而导致的丢面 / 拓扑缺口。
  - exporter 现已对非有限浮点值做写出收敛：`FormatFloat` 会把 `NaN/Inf` 统一压成 `0.0`，避免导出文本 DMX 时把 Maya 宿主里的脏几何直接序列化成 `-nan`，从而导致 importer 在后续回读时把整段 `positions` 视为无效并报“mesh geometry was empty”。
  - importer 现已对“单 joint、全部顶点 100% 权重、且 joint 索引恒定”的刚性蒙皮做短路：这类数据在当前 DAG 层级下已经能正确表达刚性跟随，继续创建 `skinCluster` 只会把 joint 变换重复施加。该修正已消除 `complex_chr_mesh` 中 `Object006` 一类刚性子件在 Maya 内导入后导出坐标爆炸的问题。
  - `MayaBatchRegression.py` 的拓扑 diff 报错已补充首个差异位置与数组长度，后续继续追踪复杂资产 roundtrip 偏差时，不再只看到笼统的 `Mesh topology mismatch`，而能直接知道 `counts/connects` 在哪一段开始漂移。
  - 已把你新增的 [dcc_plugin/samples/MostComplexSampleSet](D:/_Code_Here/Git/csgo-src/dcc_plugin/samples/MostComplexSampleSet) 纳入回归入口；当前 `maya_dmx_sample_regression`、[dcc_plugin/RunSampleRegression.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/RunSampleRegression.bat) 与 [dcc_plugin/RunMayaBatchRegression.bat](D:/_Code_Here/Git/csgo-src/dcc_plugin/RunMayaBatchRegression.bat) 都已支持 `MostComplexSampleSet/chr_mesh` 这类带子目录的样本名，并把输出文件名规整为 `MostComplexSampleSet__chr_mesh.*`，避免回归输出目录继续生成多层嵌套路径。
  - 已单独用 `mayapy` 跑过 `MostComplexSampleSet/chr_mesh` 的 Maya 闭环，输出目录为 `build\\maya_dmx\\maya_batch_regression\\MostComplexSampleSet_single`。当前链路可完成 `导入 -> 文本导出 -> 二进制导出 -> 文本回读`，但 text roundtrip 仍在 mesh diff 阶段失败，现象为 `Mesh point mismatch for body_middle_tip2Shape at vertex 89`；这说明这组新样本同样落入“少数 skinned mesh 点位保真仍不足”的剩余问题域，而不是新的解析崩溃或拓扑装配崩溃。
  - 已进一步修复 importer 的 skin weight 写回稳定性：`ApplySkinning` 现在会先把传给 `MFnSkinCluster::setWeights()` 的整块权重数组显式清零，再对每个顶点的 influence 行做归一化，避免未覆盖槽位残留垃圾值或总权重漂移。这一步直接修掉了 `MostComplexSampleSet/chr_mesh` 中 `body_middle_tip2Shape` 的输出 shape 炸点问题，也让 `complex_chr_mesh` 中 `Mesh002Shape` 的 roundtrip 点位偏差消失。
  - 已重新用 `mayapy` 跑通全量 Maya 闭环回归，输出目录为 `build\\maya_dmx\\maya_batch_regression\\full_suite_after_normalize`；当前 `simple_hierarchy`、`simple_blendshape`、`simple_mesh`、`simple_skinned_mesh`、`complex_chr_mesh`、`MostComplexSampleSet/chr_mesh` 六组样本均已通过 `导入 -> 导出 text/binary -> 再导入 -> mesh diff`。
- 当前限制：
  - importer 虽然已支持文本 DMX 与最小二进制 DMX 子集，但仍只覆盖层级、局部变换、基础 mesh、主 UV、face-vertex normals、基础蒙皮权重、基础材质槽位和最小位置 delta；不导入完整材质网络、组合型面部控制器、额外 UV 通道，也不保证兼容仓库外所有 Valve 历史二进制 DMX 变体。
  - exporter 已能写出文本 DMX 与最小二进制 DMX 子集，并支持基础蒙皮权重、完整导出层级内的 `jointList`、基础材质槽位和最小位置 delta；但仍不支持组合型面部控制器、完整材质网络、额外 UV、bind pose 扩展信息和更完整的 deformer 元数据。
  - 当前 `SimpleDmx*` 仍偏“插件定制解析器”而不是通用 DMX 库，和 `sourcepp` 当前按格式层、数据层拆分的方向相比，可维护性与格式扩展能力仍偏弱；后续补更完整二进制兼容与未知字段保真时，这会成为主要重构压力点。
  - 当前 Maya 版插件还缺少 Blender Source Tools 那类“围绕导入导出器的完整生产工作流”，例如导出配置持久化、批量导出集、动画批处理、历史兼容选项面板；这些缺口不会影响最小 DMX 闭环，但会直接限制真实资产管线落地效率。
  - 当前 `mayaDmxWorkflow` 还只是数据管理骨架：它尚未驱动真正的批量导出执行、也未把预设参数接入 [dcc_plugin/src/exporter/DmxExportTranslator.cpp](D:/_Code_Here/Git/csgo-src/dcc_plugin/src/exporter/DmxExportTranslator.cpp) 的 writer 逻辑；下一步需要先打通“预设 -> translator 选项串 -> 单次导出”，再扩展到“批量清单 -> 多次导出”。
  - 当前对 Maya UFE / 非 DAG 选择项的兼容仍以“规避宿主异常”为主，而不是完整理解 UFE 层级；如果后续需要支持 USD、代理节点或更多混合场景对象，仍需继续细化导出根过滤与类型识别规则。
  - 当前 Maya 内六组样例的 roundtrip mesh diff 已全部通过，但复杂样例在首次导入和 roundtrip 回读阶段仍会触发多条 `Some weights could not be set to the specified value. The weight total would have exceeded 1.0.` 警告；这说明虽然当前权重写回已经足以保证几何结果稳定，但 Maya 对 `skinCluster` 的内部约束仍在对部分顶点进行二次裁剪。后续若要把宿主侧警告也压平，需要继续收紧 importer 的 influence 选择、max influences 策略或 bind/setup 细节，而不只是依赖 `setWeights` 的最终结果。

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
