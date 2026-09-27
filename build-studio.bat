@echo off
setlocal
cd /d "%~dp0"

rem Force MSVC to target x64 even when launched from a normal Command Prompt.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Could not find Visual Studio vswhere.exe.
  echo Install Visual Studio 2026 Build Tools with the C++ workload.
  goto :error
)
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
if not defined VSROOT (
  echo Could not find a Visual Studio C++ installation.
  goto :error
)
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 goto :error

if not exist build mkdir build

echo [1/3] Building E#+ engine (x64)...
cl /nologo /O2 /W3 /EHsc /favor:AMD64 src\engine\main.c src\engine\lexer.c src\engine\parser.c /Fe:build\eplus-engine.exe
if errorlevel 1 goto :error

echo [2/3] Building E#+ 3D runtime (x64)...
if not exist third_party\raylib (
  echo Raylib source is required in third_party\raylib.
  echo Clone raylib 6.0 there, then run this script again.
  goto :error
)

cmake -S third_party\raylib -B build\raylib -A x64 -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF
if errorlevel 1 goto :error

cmake --build build\raylib --config Release
if errorlevel 1 goto :error

set RAYLIB_LIB=build\raylib\raylib\Release\raylib.lib
if not exist "%RAYLIB_LIB%" set RAYLIB_LIB=build\raylib\Release\raylib.lib
if not exist "%RAYLIB_LIB%" (
  echo Could not find raylib.lib.
  goto :error
)

cl /nologo /O2 /W3 /EHsc /favor:AMD64 /Ithird_party\raylib\src src\engine\game_runtime.c /Fe:build\EPlusGameRuntime.exe "%RAYLIB_LIB%" opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib
if errorlevel 1 goto :error

echo [3/3] Building E#+ Studio (x64)...
cl /nologo /O2 /W3 /EHsc /DUNICODE /D_UNICODE /favor:AMD64 src\studio\studio.c /Fe:build\EPlusStudio.exe user32.lib gdi32.lib comdlg32.lib shell32.lib winhttp.lib
if errorlevel 1 goto :error

echo.
echo ========================================
echo E#+ BUILD SUCCESSFUL
echo ========================================
echo Engine:  build\eplus-engine.exe
echo Runtime: build\EPlusGameRuntime.exe
echo Studio:  build\EPlusStudio.exe
echo.
exit /b 0

:error
echo.
echo ========================================
echo E#+ BUILD FAILED
echo ========================================
exit /b 1
