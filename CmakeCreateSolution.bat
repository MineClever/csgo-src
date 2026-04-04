@echo off
setlocal

:: %~dp0 含尾部反斜杠，去掉后再用，避免 "path\" 被解析为转义引号
set REPO_ROOT=%~dp0
set REPO_ROOT=%REPO_ROOT:~0,-1%
set BUILD_DIR=%REPO_ROOT%\build

echo ============================================================
echo  Generating CMake solution (Win32 / Visual Studio 2022)
echo ============================================================
echo.

cmake -B "%BUILD_DIR%" -A Win32 -S "%REPO_ROOT%"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: cmake configuration failed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Done. Solution: %BUILD_DIR%\CSGO.sln
echo Open with: start "%BUILD_DIR%\CSGO.sln"
pause
