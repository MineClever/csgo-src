@echo off
setlocal

set REPO_ROOT=%~dp0
set REPO_ROOT=%REPO_ROOT:~0,-1%
set OUTPUT_ZIP=%REPO_ROOT%\cmake_patch.zip

echo ============================================================
echo  Packing CMake patch to: %OUTPUT_ZIP%
echo ============================================================
echo.

powershell -ExecutionPolicy Bypass -File "%REPO_ROOT%\CmakeFilePack.ps1" -Root "%REPO_ROOT%" -Output "%OUTPUT_ZIP%"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Packing failed.
    pause
    exit /b 1
)

echo.
echo Pack completed successfully.
exit /b 0
