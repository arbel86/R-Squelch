@echo off
setlocal
cd /d "%~dp0"
title R-Squelch installer
echo.
echo  ======================================
echo     R-Squelch  -  make the installer
echo  ======================================
echo.

if not exist "Output\R-Squelch.exe" goto :nobuild
if not exist "Output\R-Squelch.vst3" goto :nobuild

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%LocalAppData%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" goto :noinno

echo Building the installer...
"%ISCC%" /Q "installer\R-Squelch.iss"
if errorlevel 1 goto :fail

echo.
echo  DONE!  The installer is in the Installer folder:
dir /b Installer\*.exe
echo.
explorer Installer
pause
exit /b 0

:nobuild
echo  The plugin is not built yet. Run build.bat first, then run this again.
echo.
pause
exit /b 1

:noinno
echo  Inno Setup 6 (free) is needed to make the installer, and it was not found.
echo  Download it from  https://jrsoftware.org/isdl.php  and install it,
echo  then run make_installer.bat again.
echo.
start "" "https://jrsoftware.org/isdl.php"
pause
exit /b 1

:fail
echo.
echo  INSTALLER BUILD FAILED - take a screenshot of this window and send it to Claude.
echo.
pause
exit /b 1
