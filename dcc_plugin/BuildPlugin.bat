@echo off
setlocal

set PLUGIN_ROOT=%~dp0
set PLUGIN_ROOT=%PLUGIN_ROOT:~0,-1%
for %%I in ("%PLUGIN_ROOT%\..") do set REPO_ROOT=%%~fI
set BUILD_DIR=%REPO_ROOT%\build\maya_dmx
set BUILD_LOG=%BUILD_DIR%\temp_build_log.log
set CONFIG=Release
set PLATFORM=x64

if not "%~1"=="" set CONFIG=%~1
if not "%~2"=="" set PLATFORM=%~2

echo ============================================================
echo  Building Maya plugins (%CONFIG%, %PLATFORM%)
echo ============================================================
echo.

if /I not "%PLATFORM%"=="x64" (
    echo ERROR: Maya 2022.5 plugin builds should use x64.
    pause
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo ============================================================ > "%BUILD_LOG%"
echo  Maya DMX Plugin Build Log (%CONFIG%, %PLATFORM%) >> "%BUILD_LOG%"
echo ============================================================ >> "%BUILD_LOG%"
echo. >> "%BUILD_LOG%"

echo Configuring CMake...
echo [CONFIGURE] cmake -S "%PLUGIN_ROOT%" -B "%BUILD_DIR%" -A %PLATFORM% >> "%BUILD_LOG%"
cmake -S "%PLUGIN_ROOT%" -B "%BUILD_DIR%" -A %PLATFORM% >> "%BUILD_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: cmake configuration failed. See "%BUILD_LOG%"
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Building plugin...
echo. >> "%BUILD_LOG%"
echo [BUILD] cmake --build "%BUILD_DIR%" --config %CONFIG% >> "%BUILD_LOG%"
cmake --build "%BUILD_DIR%" --config %CONFIG% --target maya_dmx maya_smd >> "%BUILD_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed. See "%BUILD_LOG%"
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build succeeded. Outputs:
echo   "%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.mll"
echo   "%PLUGIN_ROOT%\bin\%CONFIG%\maya_smd.mll"
echo Log: "%BUILD_LOG%"
pause
