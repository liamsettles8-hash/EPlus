@echo off
setlocal
cd /d "%~dp0"
if not exist build\EPlusStudio.exe (
  echo Run build-studio.bat first.
  exit /b 1
)
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" (
  "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" installer\EPlusStudio.iss
) else if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" (
  "%ProgramFiles%\Inno Setup 6\ISCC.exe" installer\EPlusStudio.iss
) else (
  echo Inno Setup 6 not found.
  exit /b 1
)
