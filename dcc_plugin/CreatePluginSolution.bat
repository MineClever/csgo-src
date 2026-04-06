@echo off
setlocal

set PLUGIN_ROOT=%~dp0
set PLUGIN_ROOT=%PLUGIN_ROOT:~0,-1%
for %%I in ("%PLUGIN_ROOT%\..") do set REPO_ROOT=%%~fI
set BUILD_DIR=%REPO_ROOT%\build\maya_dmx
set PLATFORM=x64

if not "%~1"=="" set PLATFORM=%~1

echo ============================================================
echo  Generating Maya DMX plugin solution (%PLATFORM%)
echo ============================================================
echo.

if /I not "%PLATFORM%"=="x64" (
    echo ERROR: Maya 2022.5 plugin builds should use x64.
    pause
    exit /b 1
)

cmake -S "%PLUGIN_ROOT%" -B "%BUILD_DIR%" -A %PLATFORM%
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: cmake configuration failed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Done. Solution: %BUILD_DIR%\MayaDmxPlugin.sln
echo Open with: start "" "%BUILD_DIR%\MayaDmxPlugin.sln"
pause
