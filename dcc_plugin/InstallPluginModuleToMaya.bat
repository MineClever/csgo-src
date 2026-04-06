@echo off
setlocal

set PLUGIN_ROOT=%~dp0
set PLUGIN_ROOT=%PLUGIN_ROOT:~0,-1%
set CONFIG=Release
set MODULE_NAME=maya_dmx
set MODULE_VERSION=0.1.0
set MAYA_VERSION=2022
set SOURCE_PLUGIN=%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.mll
set SOURCE_PDB=%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.pdb
set MODULE_ROOT=%PLUGIN_ROOT%\maya_module
set MODULE_PLUGIN_DIR=%MODULE_ROOT%\plug-ins\windows\%MAYA_VERSION%
set MODULE_SCRIPTS_DIR=%MODULE_ROOT%\scripts
set MODULE_ICONS_DIR=%MODULE_ROOT%\icons
set USER_MODULE_DIR=%USERPROFILE%\Documents\maya\modules
set USER_MODULE_FILE=%USER_MODULE_DIR%\%MODULE_NAME%.mod

if not "%~1"=="" set CONFIG=%~1
set SOURCE_PLUGIN=%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.mll
set SOURCE_PDB=%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.pdb

echo ============================================================
echo  Installing Maya DMX plugin module (%CONFIG%)
echo ============================================================
echo.

taskkill /f /im maya.exe 2>nul
timeout /t 2

if not exist "%SOURCE_PLUGIN%" (
    echo ERROR: Built plugin not found: "%SOURCE_PLUGIN%"
    echo Run "%PLUGIN_ROOT%\BuildPlugin.bat" first.
    pause
    exit /b 1
)

if not exist "%USER_MODULE_DIR%" mkdir "%USER_MODULE_DIR%"
if not exist "%MODULE_PLUGIN_DIR%" mkdir "%MODULE_PLUGIN_DIR%"
if not exist "%MODULE_SCRIPTS_DIR%" mkdir "%MODULE_SCRIPTS_DIR%"
if not exist "%MODULE_ICONS_DIR%" mkdir "%MODULE_ICONS_DIR%"

copy /Y "%SOURCE_PLUGIN%" "%MODULE_PLUGIN_DIR%\maya_dmx.mll" >nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to copy plugin binary.
    pause
    exit /b %ERRORLEVEL%
)

if exist "%SOURCE_PDB%" (
    copy /Y "%SOURCE_PDB%" "%MODULE_PLUGIN_DIR%\maya_dmx.pdb" >nul
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Failed to copy plugin debug symbols.
        pause
        exit /b %ERRORLEVEL%
    )
) else (
    echo NOTE: Debug symbols not found, skipping PDB copy.
)

set MODULE_ROOT_MOD=%MODULE_ROOT:\=/%

(
    echo + MAYAVERSION:%MAYA_VERSION% PLATFORM:win64 MayaDmx %MODULE_VERSION% %MODULE_ROOT_MOD%
    echo plug-ins: plug-ins/windows/%MAYA_VERSION%
    echo icons: icons
    echo scripts: scripts
) > "%USER_MODULE_FILE%"

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to write module file: "%USER_MODULE_FILE%"
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Installed module file:
echo   "%USER_MODULE_FILE%"
echo Plugin binary:
echo   "%MODULE_PLUGIN_DIR%\maya_dmx.mll"
if exist "%MODULE_PLUGIN_DIR%\maya_dmx.pdb" (
    echo Debug symbols:
    echo   "%MODULE_PLUGIN_DIR%\maya_dmx.pdb"
)
echo.
echo Restart Maya 2022.5 to discover the new module.
pause
