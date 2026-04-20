@echo off
setlocal

set PLUGIN_ROOT=%~dp0
set PLUGIN_ROOT=%PLUGIN_ROOT:~0,-1%
for %%I in ("%PLUGIN_ROOT%\..") do set REPO_ROOT=%%~fI
set BUILD_DIR=%PLUGIN_ROOT%\build
set BUILD_LOG=%BUILD_DIR%\temp_build_log.log
set MODULE_PLUGIN_DIR=%PLUGIN_ROOT%\maya_module\plug-ins\windows\2022
set CONFIG=Release
set PLATFORM=x64
set BUILD_PDB=OFF

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

if exist "%BUILD_DIR%" (
    echo Removing previous build directory...
    rmdir /S /Q "%BUILD_DIR%"
    if exist "%BUILD_DIR%" (
        echo ERROR: failed to remove previous build directory "%BUILD_DIR%"
        pause
        exit /b 1
    )
)

mkdir "%BUILD_DIR%"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: failed to create build directory "%BUILD_DIR%"
    pause
    exit /b %ERRORLEVEL%
)

echo ============================================================ > "%BUILD_LOG%"
echo  Maya DMX Plugin Build Log (%CONFIG%, %PLATFORM%) >> "%BUILD_LOG%"
echo ============================================================ >> "%BUILD_LOG%"
echo. >> "%BUILD_LOG%"
echo Full rebuild: build directory recreated from scratch. >> "%BUILD_LOG%"

echo Configuring CMake...
echo [CONFIGURE] cmake -S "%PLUGIN_ROOT%" -B "%BUILD_DIR%" -A %PLATFORM% -DMAYA_DMX_BUILD_PDB=%BUILD_PDB% >> "%BUILD_LOG%"
cmake -S "%PLUGIN_ROOT%" -B "%BUILD_DIR%" -A %PLATFORM% -DMAYA_DMX_BUILD_PDB=%BUILD_PDB% >> "%BUILD_LOG%" 2>&1
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
cmake --build "%BUILD_DIR%" --config %CONFIG% >> "%BUILD_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed. See "%BUILD_LOG%"
    pause
    exit /b %ERRORLEVEL%
)

if not exist "%MODULE_PLUGIN_DIR%" (
    mkdir "%MODULE_PLUGIN_DIR%"
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo ERROR: failed to create module plugin directory "%MODULE_PLUGIN_DIR%"
        pause
        exit /b %ERRORLEVEL%
    )
)

copy /Y "%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.mll" "%MODULE_PLUGIN_DIR%\maya_dmx.mll" >nul
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: failed to sync maya_dmx.mll into "%MODULE_PLUGIN_DIR%"
    pause
    exit /b %ERRORLEVEL%
)

copy /Y "%PLUGIN_ROOT%\bin\%CONFIG%\maya_smd.mll" "%MODULE_PLUGIN_DIR%\maya_smd.mll" >nul
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: failed to sync maya_smd.mll into "%MODULE_PLUGIN_DIR%"
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build succeeded. Outputs:
if /I "%BUILD_PDB%"=="OFF" (
del /Q "%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.pdb" >nul 2>nul
del /Q "%PLUGIN_ROOT%\bin\%CONFIG%\maya_smd.pdb" >nul 2>nul
)
echo   "%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.mll"
echo   "%PLUGIN_ROOT%\bin\%CONFIG%\maya_smd.mll"
echo Synced module plugins:
echo   "%MODULE_PLUGIN_DIR%\maya_dmx.mll"
echo   "%MODULE_PLUGIN_DIR%\maya_smd.mll"
if /I "%BUILD_PDB%"=="ON" (
echo   "%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.pdb"
echo   "%PLUGIN_ROOT%\bin\%CONFIG%\maya_smd.pdb"
)
echo Log: "%BUILD_LOG%"
pause
