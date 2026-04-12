# Maya DMX 宿主验证环境

## 查询入口

- 批处理入口：`dcc_plugin\QueryMayaValidationEnv.bat`
- PowerShell 脚本：`dcc_plugin\tools\QueryMayaValidationEnv.ps1`
- 默认输出报告：`dcc_plugin\build\maya_validation_env_report.md`

## 查询范围

- Maya 2022 安装目录
- `maya.exe` / `mayapy.exe`
- Maya 2022.5 DevKit 根目录
- 已构建的 `maya_dmx.mll`
- MEL 脚本目录
- 批回归与交互验证脚本/包装入口
- `mayapy` Python 版本

## 本机最新查询结果

- 查询时间：2026-04-11 11:39:06 +08:00
- 配置：`Release`
- Maya 安装目录：`C:\Program Files\Autodesk\Maya2022`
- Maya DevKit：`D:\_Code_Here\Maya\Autodesk_Maya_2022_5_Update_DEVKIT_Windows\devkitBase`
- `maya.exe`：存在，文件版本 `22.5.0.2448`
- `mayapy.exe`：存在，文件版本 `22.5.0.2448`
- `mayapy` Python 版本：`3.7.7`
- 插件二进制：`dcc_plugin\bin\Release\maya_dmx.mll`，存在，最后写入时间 `2026-04-10 01:18:12`
- MEL 脚本目录：`dcc_plugin\src\mel`，存在
- 批回归入口：`dcc_plugin\RunMayaBatchRegression.bat`，存在
- 交互验证入口：`dcc_plugin\RunMayaInteractiveValidation.bat`，存在

## 结论

- 当前机器满足 DMX 插件宿主级批回归与交互验证所需的基础路径条件。
- 若 Maya 不在默认安装目录，批回归可通过 `MAYA_PYTHON_EXE_OVERRIDE` 覆盖解释器路径，交互验证可通过 `MAYA_EXE_OVERRIDE` 覆盖宿主路径。
- 当前机器存在第三方 `userSetup.py` 干扰风险；宿主级批回归应保持 `MAYA_SKIP_USERSETUP_PY=1`，与现有包装脚本约定一致。
