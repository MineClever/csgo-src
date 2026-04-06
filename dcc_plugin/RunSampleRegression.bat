@echo off
setlocal

set PLUGIN_ROOT=%~dp0
set PLUGIN_ROOT=%PLUGIN_ROOT:~0,-1%
for %%I in ("%PLUGIN_ROOT%\..") do set REPO_ROOT=%%~fI
set BUILD_DIR=%REPO_ROOT%\build\maya_dmx
set CONFIG=Release
if not "%~1"=="" set CONFIG=%~1

set TOOL_PATH=%PLUGIN_ROOT%\bin\%CONFIG%\maya_dmx_sample_tool.exe
set OUTPUT_DIR=%BUILD_DIR%\sample_regression\%CONFIG%

echo ============================================================
echo  Running Maya DMX serialization regression (%CONFIG%)
echo ============================================================
echo.
echo This regression only verifies DMX text/binary serialization roundtrip.
echo Maya export/import node-type stability is verified by RunMayaBatchRegression.bat.
echo.

if not exist "%TOOL_PATH%" (
    echo Sample tool was not found at:
    echo   "%TOOL_PATH%"
    echo Build the plugin first with:
    echo   dcc_plugin\BuildPlugin.bat %CONFIG%
    pause
    exit /b 1
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

call :RunCase simple_hierarchy
if errorlevel 1 goto :fail

call :RunCase simple_blendshape
if errorlevel 1 goto :fail

call :RunCase simple_mesh
if errorlevel 1 goto :fail

call :RunCase simple_skinned_mesh
if errorlevel 1 goto :fail

call :RunCase complex_chr_mesh
if errorlevel 1 goto :fail

call :RunCase MostComplexSampleSet/chr_mesh
if errorlevel 1 goto :fail

echo.
echo Sample regression passed.
echo Output directory: "%OUTPUT_DIR%"
pause
exit /b 0

:RunCase
set SAMPLE_NAME=%~1
set INPUT_FILE=%PLUGIN_ROOT%\samples\%SAMPLE_NAME%.dmx
set OUTPUT_NAME=%SAMPLE_NAME:/=__%
set BINARY_FILE=%OUTPUT_DIR%\%OUTPUT_NAME%.dmxb
set ROUNDTRIP_FILE=%OUTPUT_DIR%\%OUTPUT_NAME%.roundtrip.dmx

echo [%SAMPLE_NAME%] text -^> binary
"%TOOL_PATH%" "%INPUT_FILE%" "%BINARY_FILE%"
if errorlevel 1 exit /b 1

echo [%SAMPLE_NAME%] binary -^> text
"%TOOL_PATH%" "%BINARY_FILE%" "%ROUNDTRIP_FILE%"
if errorlevel 1 exit /b 1

exit /b 0

:fail
echo.
echo Sample regression failed.
pause
exit /b 1
