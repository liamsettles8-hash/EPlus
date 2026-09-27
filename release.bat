@echo off
setlocal
cd /d "%~dp0"
call build-studio.bat || exit /b 1
call build-installer.bat || exit /b 1
echo.
echo Installer is ready:
echo installer-output\EPlusStudio-Setup.exe
echo.
echo Create a GitHub release and upload that file as EPlusStudio-Setup.exe.
pause
