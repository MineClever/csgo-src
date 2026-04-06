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

echo Configuring CMake...
cmake -B "%BUILD_DIR%" -A %PLATFORM% -S "%REPO_ROOT%"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: cmake configuration failed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Building solution...
rem Passing --parallel or explicit MSBuild args causes an immediate early exit
rem in this repository/toolchain combination. Use the default Visual Studio build.
cmake --build "%BUILD_DIR%" --config %CONFIG%
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed with config: %CONFIG%
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build succeeded (%CONFIG%).
pause
