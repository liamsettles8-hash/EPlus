@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /O2 /W3 src\engine\main.c src\engine\lexer.c src\engine\parser.c /Fe:build\eplus-engine.exe
if errorlevel 1 goto :error
cl /nologo /O2 /W3 /DUNICODE /D_UNICODE src\studio\studio.c /Fe:build\EPlusStudio.exe user32.lib gdi32.lib comdlg32.lib shell32.lib winhttp.lib
if errorlevel 1 goto :error
echo BUILD SUCCESSFUL
exit /b 0
:error
echo BUILD FAILED
exit /b 1
