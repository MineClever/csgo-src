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
set SMD_PLUGIN_BINARY=%PLUGIN_ROOT%\bin\%CONFIG%\maya_smd.mll
set SAMPLE_DIR=%PLUGIN_ROOT%\samples
set OUTPUT_DIR=%PLUGIN_ROOT%\build\maya_batch_regression\%CONFIG%
set SCRIPT_PATH=%PLUGIN_ROOT%\tools\MayaBatchRegression.py

echo ============================================================
echo  Running Maya batch DMX regression (%CONFIG%)
echo ============================================================
echo.
echo This regression verifies Maya roundtrip for DMX and SMD samples,
echo including mesh, transform/joint type stability, skin retention,
echo and animation import gates for dedicated animation samples.
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

if not exist "%SMD_PLUGIN_BINARY%" (
    echo ERROR: Built SMD plugin was not found:
    echo   "%SMD_PLUGIN_BINARY%"
    echo Build the plugin first so SMD cases can participate in regression.
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
    --plugin-smd "%SMD_PLUGIN_BINARY%" ^
    --samples "%SAMPLE_DIR%" ^
    --output "%OUTPUT_DIR%" ^
    --cases simple_hierarchy simple_blendshape simple_mesh simple_skinned_mesh complex_chr_mesh MostComplexSampleSet/chr_mesh simple_ngon_mesh MostComplexSampleSet/vcaanim_VertexAnim simple_float_animation simple_blendshape_animation MostComplexSampleSet/chr_mesh.smd MostComplexSampleSet/vcaanim_VertexAnim.smd Ellis/DMX/RAGDOLL.smd ctm_fbi/ctm_fbi.smd ctm_fbi/ctm_fbi_physics.smd ctm_fbi/ctm_fbi_w_ct_base_glove.smd ctm_fbi/ctm_fbi_anims/default.smd ctm_fbi/ctm_fbi_anims/ragdoll.smd ctm_fbi/ctm_fbi_anims/rom_skin.smd ctm_fbi/ctm_fbi_anims/shield_deploy.smd Ellis/DMX/animation/c1m1_intro_mechanic.dmx
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
