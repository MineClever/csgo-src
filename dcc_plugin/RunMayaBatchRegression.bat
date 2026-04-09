@echo off
setlocal

set PLUGIN_ROOT=%~dp0
set PLUGIN_ROOT=%PLUGIN_ROOT:~0,-1%
for %%I in ("%PLUGIN_ROOT%\..") do set REPO_ROOT=%%~fI

set CONFIG=Release
if not "%~1"=="" set CONFIG=%~1

set MAYA_PYTHON_EXE=C:\Program Files\Autodesk\Maya2022\bin\mayapy.exe
if not "%MAYA_PYTHON_EXE_OVERRIDE%"=="" set MAYA_PYTHON_EXE=%MAYA_PYTHON_EXE_OVERRIDE%

set PLUGIN_BINARY=%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx.mll
set SAMPLE_DIR=%PLUGIN_ROOT%\samples
set OUTPUT_DIR=%REPO_ROOT%\build\maya_dmx\maya_batch_regression\%CONFIG%
set SCRIPT_PATH=%PLUGIN_ROOT%\tools\MayaBatchRegression.py

echo ============================================================
echo  Running Maya batch DMX regression (%CONFIG%)
echo ============================================================
echo.
echo This regression verifies mesh roundtrip and transform/joint type stability.
echo.

if not exist "%MAYA_PYTHON_EXE%" (
    echo ERROR: Maya Python executable was not found:
    echo   "%MAYA_PYTHON_EXE%"
    echo Set MAYA_PYTHON_EXE_OVERRIDE to override the default path.
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

if not exist "%SCRIPT_PATH%" (
    echo ERROR: Regression script was not found:
    echo   "%SCRIPT_PATH%"
    pause
    exit /b 1
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

set MAYA_SKIP_USERSETUP_PY=1

"%MAYA_PYTHON_EXE%" "%SCRIPT_PATH%" ^
    --plugin "%PLUGIN_BINARY%" ^
    --samples "%SAMPLE_DIR%" ^
    --output "%OUTPUT_DIR%" ^
    --cases simple_hierarchy simple_blendshape simple_mesh simple_skinned_mesh complex_chr_mesh MostComplexSampleSet/chr_mesh simple_ngon_mesh MostComplexSampleSet/vcaanim_VertexAnim simple_float_animation simple_blendshape_animation
if errorlevel 1 (
    echo.
    echo Maya batch regression failed.
    pause
    exit /b 1
)

echo.
echo Maya batch regression passed.
echo Output directory:
echo   "%OUTPUT_DIR%"
pause
exit /b 0
