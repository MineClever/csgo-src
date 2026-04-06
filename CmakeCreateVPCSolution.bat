@echo off
setlocal

set REPO_ROOT=%~dp0
set REPO_ROOT=%REPO_ROOT:~0,-1%
set BUILD_DIR=%REPO_ROOT%\build_vpc
set PLATFORM=Win32

if not "%~1"=="" set PLATFORM=%~1

echo ============================================================
echo  Generating VPC CMake solution (%PLATFORM% / Visual Studio 2022)
echo ============================================================
echo.

cmake -B "%BUILD_DIR%" -A %PLATFORM% -S "%REPO_ROOT%" -DVALVE_CONFIGURE_VPC_ONLY=ON
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: cmake configuration failed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Done. Solution: %BUILD_DIR%\VPC.sln
echo Open with: start "%BUILD_DIR%\VPC.sln"
pause
