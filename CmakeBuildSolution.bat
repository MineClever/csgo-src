@echo off
setlocal

set REPO_ROOT=%~dp0
set REPO_ROOT=%REPO_ROOT:~0,-1%
set BUILD_DIR=%REPO_ROOT%\build
set CONFIG=Release
set PLATFORM=Win32

if not "%~1"=="" set CONFIG=%~1
if not "%~2"=="" set PLATFORM=%~2

echo ============================================================
echo  Building CSGO (%CONFIG%, %PLATFORM%)
echo ============================================================
echo.

:: 如果 build 目录不存在，先生成
if not exist "%BUILD_DIR%\CSGO.sln" (
    echo Build directory not found, running cmake configure first...
    cmake -B "%BUILD_DIR%" -A %PLATFORM% -S "%REPO_ROOT%"
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: cmake configuration failed.
        pause
        exit /b %ERRORLEVEL%
    )
)

cmake --build "%BUILD_DIR%" --config %CONFIG% --parallel
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed with config: %CONFIG%
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build succeeded (%CONFIG%).
pause
