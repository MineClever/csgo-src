@echo off
setlocal

set REPO_ROOT=%~dp0
set REPO_ROOT=%REPO_ROOT:~0,-1%
set BUILD_DIR=%REPO_ROOT%\build_vpc
set CONFIG=Release
set PLATFORM=Win32

if not "%~1"=="" set CONFIG=%~1
if not "%~2"=="" set PLATFORM=%~2

echo ============================================================
echo  Building VPC (%CONFIG%, %PLATFORM%)
echo ============================================================
echo.

if not exist "%BUILD_DIR%\VPC.sln" (
    echo Build directory not found, running cmake configure first...
    cmake -B "%BUILD_DIR%" -A %PLATFORM% -S "%REPO_ROOT%" -DVALVE_CONFIGURE_VPC_ONLY=ON
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: cmake configuration failed.
        pause
        exit /b %ERRORLEVEL%
    )
)

cmake --build "%BUILD_DIR%" --config %CONFIG% --target vpc_all
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed with config: %CONFIG%
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build succeeded (%CONFIG%).
pause
