@echo off
setlocal

set REPO_ROOT=%~dp0
set REPO_ROOT=%REPO_ROOT:~0,-1%
set BUILD_DIR=%REPO_ROOT%\build
set BUILD_LOG=%BUILD_DIR%\temp_build_log.log
set CONFIG=Release
set PLATFORM=Win32

if not "%~1"=="" set CONFIG=%~1
if not "%~2"=="" set PLATFORM=%~2

echo ============================================================
echo  Building CSGO (%CONFIG%, %PLATFORM%)
echo ============================================================
echo.

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo ============================================================ > "%BUILD_LOG%"
echo  CSGO Build Log (%CONFIG%, %PLATFORM%) >> "%BUILD_LOG%"
echo ============================================================ >> "%BUILD_LOG%"
echo. >> "%BUILD_LOG%"

echo Configuring CMake...
echo [CONFIGURE] cmake -B "%BUILD_DIR%" -A %PLATFORM% -S "%REPO_ROOT%" >> "%BUILD_LOG%"
cmake -B "%BUILD_DIR%" -A %PLATFORM% -S "%REPO_ROOT%" >> "%BUILD_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: cmake configuration failed. See "%BUILD_LOG%"
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Building solution...
rem Passing --parallel or explicit MSBuild args causes an immediate early exit
rem in this repository/toolchain combination. Use the default Visual Studio build.
echo. >> "%BUILD_LOG%"
echo [BUILD] cmake --build "%BUILD_DIR%" --config %CONFIG% >> "%BUILD_LOG%"
cmake --build "%BUILD_DIR%" --config %CONFIG% >> "%BUILD_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed with config: %CONFIG%. See "%BUILD_LOG%"
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build succeeded (%CONFIG%).
echo Build succeeded (%CONFIG%). Log: "%BUILD_LOG%"
pause
