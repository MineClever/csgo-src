@echo off
setlocal

set PLUGIN_ROOT=%~dp0
set PLUGIN_ROOT=%PLUGIN_ROOT:~0,-1%
for %%I in ("%PLUGIN_ROOT%\..") do set REPO_ROOT=%%~fI
set BUILD_DIR=%PLUGIN_ROOT%\build
set BUILD_LOG=%BUILD_DIR%\temp_build_log.log
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

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo ============================================================ > "%BUILD_LOG%"
echo  Maya DMX Plugin Build Log (%CONFIG%, %PLATFORM%) >> "%BUILD_LOG%"
echo ============================================================ >> "%BUILD_LOG%"
echo. >> "%BUILD_LOG%"

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
cmake --build "%BUILD_DIR%" --config %CONFIG% --target maya_dmx maya_smd >> "%BUILD_LOG%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed. See "%BUILD_LOG%"
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
if /I "%BUILD_PDB%"=="ON" (
echo   "%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.pdb"
echo   "%PLUGIN_ROOT%\bin\%CONFIG%\maya_smd.pdb"
)
echo Log: "%BUILD_LOG%"
pause
