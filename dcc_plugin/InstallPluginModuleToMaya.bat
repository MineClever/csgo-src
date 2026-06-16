@echo off
setlocal EnableDelayedExpansion

set PLUGIN_ROOT=%~dp0
set PLUGIN_ROOT=%PLUGIN_ROOT:~0,-1%
set CONFIG=Release
set MODULE_NAME=maya_dmx
set MODULE_VERSION=0.1.0
set MAYA_VERSION=2022
set SOURCE_BIN_DIR=%PLUGIN_ROOT%\bin\%CONFIG%
set SOURCE_MEL_DIR=%PLUGIN_ROOT%\src\mel
set MODULE_ROOT=%PLUGIN_ROOT%\maya_module
set MODULE_PLUGIN_DIR=%MODULE_ROOT%\plug-ins\windows\%MAYA_VERSION%
set MODULE_SCRIPTS_DIR=%MODULE_ROOT%\scripts
set MODULE_ICONS_DIR=%MODULE_ROOT%\icons
set USER_MODULE_DIR=%USERPROFILE%\Documents\maya\modules
set USER_MODULE_FILE=%USER_MODULE_DIR%\%MODULE_NAME%.mod

if not "%~1"=="" set CONFIG=%~1
set SOURCE_BIN_DIR=%PLUGIN_ROOT%\bin\%CONFIG%

echo ============================================================
echo  Installing Maya plugin module (%CONFIG%)
echo ============================================================
echo.

taskkill /f /im maya.exe 2>nul
timeout /t 2 >nul

if not exist "%SOURCE_BIN_DIR%" (
    echo ERROR: Build output directory not found: "%SOURCE_BIN_DIR%"
    echo Run "%PLUGIN_ROOT%\BuildPlugin.bat" first.
    pause
    exit /b 1
)

REM --- Discover all .mll plugins in the build output directory ---
set HAS_MLL=0
for %%f in ("%SOURCE_BIN_DIR%\*.mll") do (
    set HAS_MLL=1
)

if "!HAS_MLL!"=="0" (
    echo ERROR: No .mll plugin binaries found in: "%SOURCE_BIN_DIR%"
    echo Run "%PLUGIN_ROOT%\BuildPlugin.bat" first.
    pause
    exit /b 1
)

if not exist "%SOURCE_MEL_DIR%" (
    echo WARNING: MEL source directory not found, skipping MEL scripts: "%SOURCE_MEL_DIR%"
)

if not exist "%USER_MODULE_DIR%" mkdir "%USER_MODULE_DIR%"
if not exist "%MODULE_PLUGIN_DIR%" mkdir "%MODULE_PLUGIN_DIR%"
if not exist "%MODULE_SCRIPTS_DIR%" mkdir "%MODULE_SCRIPTS_DIR%"
if not exist "%MODULE_ICONS_DIR%" mkdir "%MODULE_ICONS_DIR%"

REM --- Clear and copy .mll plugin binaries ---
echo --- Plugin binaries ---
del /Q "%MODULE_PLUGIN_DIR%\*.mll" >nul 2>nul
del /Q "%MODULE_PLUGIN_DIR%\*.pdb" >nul 2>nul

for %%f in ("%SOURCE_BIN_DIR%\*.mll") do (
    copy /Y "%%f" "%MODULE_PLUGIN_DIR%\" >nul
    if !ERRORLEVEL! NEQ 0 (
        echo ERROR: Failed to copy plugin binary: %%~nxf
        pause
        exit /b !ERRORLEVEL!
    )
    echo   %%~nxf
)

REM --- Copy matching .pdb files ---
set COPIED_PDB=0
for %%f in ("%SOURCE_BIN_DIR%\*.pdb") do (
    copy /Y "%%f" "%MODULE_PLUGIN_DIR%\" >nul
    if !ERRORLEVEL! NEQ 0 (
        echo WARNING: Failed to copy debug symbols: %%~nxf
    ) else (
        set COPIED_PDB=1
        echo   %%~nxf
    )
)
if "!COPIED_PDB!"=="0" (
    echo   (no PDB files found)
)

REM --- Copy MEL scripts ---
if exist "%SOURCE_MEL_DIR%" (
    echo --- MEL scripts ---
    del /Q "%MODULE_SCRIPTS_DIR%\*" >nul 2>nul
    for %%f in ("%SOURCE_MEL_DIR%\*.mel") do (
        copy /Y "%%f" "%MODULE_SCRIPTS_DIR%\" >nul
        if !ERRORLEVEL! NEQ 0 (
            echo WARNING: Failed to copy MEL script: %%~nxf
        ) else (
            echo   %%~nxf
        )
    )
)

REM --- Write module descriptor file ---
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
echo Module file: "%USER_MODULE_FILE%"
echo Plugin binaries installed to: "%MODULE_PLUGIN_DIR%"
echo MEL scripts installed to:   "%MODULE_SCRIPTS_DIR%"
echo.
echo Restart Maya 2022.5 to discover the new module.
pause
