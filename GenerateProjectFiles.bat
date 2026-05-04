@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "BUILD_DIR=build"

REM -----------------------------------------------------------------------------
REM Configure MiniEngine out-of-tree build (Windows x64).
REM
REM - No hard-coded Visual Studio version: CMake chooses the default generator
REM   for your installation (typically the newest VS when present).
REM - Pin a generator explicitly by passing CMake arguments after this script name:
REM     GenerateProjectFiles.bat -G "Visual Studio 17 2022"
REM     GenerateProjectFiles.bat -G "Visual Studio 16 2019"
REM - Single-config generators (Ninja): pass build type at configure time, e.g.
REM     GenerateProjectFiles.bat -G Ninja -DCMAKE_BUILD_TYPE=Debug
REM - Visual Studio generators are multi-config: pick Debug/Release at build time:
REM     cmake --build build --config Debug
REM -----------------------------------------------------------------------------

cmake -S "%CD%" -B "%BUILD_DIR%" -A x64 %*
if errorlevel 1 (
	pause
	exit /b 1
)

if exist "%BUILD_DIR%\MiniEngine.sln" (
	start "" "%BUILD_DIR%\MiniEngine.sln"
) else (
	echo [%~n0] No Visual Studio solution at "%BUILD_DIR%\MiniEngine.sln".
	echo [%~n0] Configure-only finished ^(e.g. Ninja / non-VS generator or custom project name^).
)

exit /b 0
