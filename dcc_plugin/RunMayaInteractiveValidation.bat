@echo off
setlocal

set PLUGIN_ROOT=%~dp0
set PLUGIN_ROOT=%PLUGIN_ROOT:~0,-1%
for %%I in ("%PLUGIN_ROOT%\..") do set REPO_ROOT=%%~fI

set CONFIG=Release
if not "%~1"=="" set CONFIG=%~1

set MAYA_EXE=C:\Program Files\Autodesk\Maya2022\bin\maya.exe
if not "%MAYA_EXE_OVERRIDE%"=="" set MAYA_EXE=%MAYA_EXE_OVERRIDE%

set PLUGIN_BINARY=%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.mll
set MEL_DIR=%PLUGIN_ROOT%\src\mel
set VALIDATION_SCRIPT=%PLUGIN_ROOT%\tools\MayaInteractiveValidation.py
set PLUGIN_BINARY_PY=%PLUGIN_BINARY:\=/%
set MEL_DIR_PY=%MEL_DIR:\=/%
set VALIDATION_SCRIPT_PY=%VALIDATION_SCRIPT:\=/%

echo ============================================================
echo  Launching Maya DMX interactive validation (%CONFIG%)
echo ============================================================
echo.

if not exist "%MAYA_EXE%" (
    echo ERROR: Maya executable was not found:
    echo   "%MAYA_EXE%"
    echo Set MAYA_EXE_OVERRIDE to override the default path.
    pause
    exit /b 1
)

if not exist "%PLUGIN_BINARY%" (
    echo ERROR: Built plugin was not found:
    echo   "%PLUGIN_BINARY%"
    echo Build the plugin first with:
    echo   dcc_plugin\BuildPlugin.bat %CONFIG%
    pause
    exit /b 1
)

if not exist "%VALIDATION_SCRIPT%" (
    echo ERROR: Validation bootstrap script was not found:
    echo   "%VALIDATION_SCRIPT%"
    pause
    exit /b 1
)

set MAYA_SKIP_USERSETUP_PY=1
set MAYA_DMX_PLUGIN=%PLUGIN_BINARY_PY%
set MAYA_DMX_SCRIPTS=%MEL_DIR_PY%

"%MAYA_EXE%" -command "python(\"import os, runpy, sys; sys.argv=['MayaInteractiveValidation.py','--plugin', os.environ['MAYA_DMX_PLUGIN'], '--scripts', os.environ['MAYA_DMX_SCRIPTS']]; runpy.run_path(r'%VALIDATION_SCRIPT_PY%', run_name='__main__')\")"
exit /b %ERRORLEVEL%
