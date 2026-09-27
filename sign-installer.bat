@echo off
setlocal
cd /d "%~dp0"
if "%EPLUS_CERT%"=="" (
  echo No code-signing certificate configured.
  echo E#+ Studio will remain Unknown Publisher until a trusted certificate is purchased and configured.
  exit /b 0
)
where signtool >nul 2>nul
if errorlevel 1 (
  echo signtool.exe not found. Run this from a Visual Studio Developer Command Prompt.
  exit /b 1
)
signtool sign /fd SHA256 /a /f "%EPLUS_CERT%" "installer-output\EPlus-Studio-Setup.exe"
if errorlevel 1 exit /b 1
signtool verify /pa "installer-output\EPlus-Studio-Setup.exe"
