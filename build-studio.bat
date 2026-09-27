@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
echo [1/3] Building E#+ engine...
cl /nologo /O2 /W3 src\engine\main.c src\engine\lexer.c src\engine\parser.c /Fe:build\eplus-engine.exe
if errorlevel 1 goto :error
echo [2/3] Building E#+ 3D runtime...
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
if not exist "%RAYLIB_LIB%" goto :error
cl /nologo /O2 /W3 /Ithird_party\raylib\src src\engine\game_runtime.c /Fe:build\EPlusGameRuntime.exe "%RAYLIB_LIB%" opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib
if errorlevel 1 goto :error
echo [3/3] Building E#+ Studio...
cl /nologo /O2 /W3 /DUNICODE /D_UNICODE src\studio\studio.c /Fe:build\EPlusStudio.exe user32.lib gdi32.lib comdlg32.lib shell32.lib winhttp.lib
if errorlevel 1 goto :error
echo BUILD SUCCESSFUL
exit /b 0
:error
echo BUILD FAILED
exit /b 1
