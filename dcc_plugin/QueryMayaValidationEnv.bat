@echo off
setlocal

set PLUGIN_ROOT=%~dp0
set PLUGIN_ROOT=%PLUGIN_ROOT:~0,-1%
for %%I in ("%PLUGIN_ROOT%\..") do set REPO_ROOT=%%~fI

set CONFIG=Release
if not "%~1"=="" set CONFIG=%~1

powershell -ExecutionPolicy Bypass -File "%PLUGIN_ROOT%\tools\QueryMayaValidationEnv.ps1" ^
    -Config "%CONFIG%" ^
    -RepoRoot "%REPO_ROOT%" ^
    -OutputPath "%PLUGIN_ROOT%\build\maya_validation_env_report.md"

exit /b %ERRORLEVEL%
