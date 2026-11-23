@echo off
setlocal
cd /d "%~dp0"
title R-Squelch build
echo.
echo  ==============================
echo     R-Squelch  -  Windows build
echo  ==============================
echo.

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :novs

set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto :novs

set "CMAKE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"

rem ---- when the JUCE version (or the build settings) changed, start from a clean build folder ----
rem      (old compiled files from the previous JUCE version cause "unresolved external symbol" errors)
set "STAMP="
for /f "usebackq tokens=*" %%a in (`findstr /c:"JUCE/archive/refs/tags/" CMakeLists.txt`) do set "STAMP=%%a v2"
set "OLDSTAMP="
if exist "build\rsq_build_stamp.txt" set /p OLDSTAMP=<"build\rsq_build_stamp.txt"
if exist build if not "%STAMP%"=="%OLDSTAMP%" (
    echo JUCE / build settings changed - removing the old build folder first...
    rmdir /s /q build
)

echo [1/3] Configuring - the first run downloads JUCE, this can take a few minutes...
"%CMAKE%" -S . -B build -A x64
if errorlevel 1 goto :fail
> "build\rsq_build_stamp.txt" echo %STAMP%

echo.
echo [2/3] Compiling R-Squelch - Release...
"%CMAKE%" --build build --config Release --parallel --target RSquelch_VST3 RSquelch_Standalone
if errorlevel 1 goto :fail

echo.
echo [3/3] Copying the finished plugin to the Output folder...
if exist Output rmdir /s /q Output
mkdir Output
xcopy /e /i /y /q "build\RSquelch_artefacts\Release\VST3\R-Squelch.vst3" "Output\R-Squelch.vst3" >nul
copy /y "build\RSquelch_artefacts\Release\Standalone\R-Squelch.exe" "Output\" >nul

echo.
echo  DONE!
echo   Standalone : Output\R-Squelch.exe
echo   VST3       : Output\R-Squelch.vst3
echo   Copy R-Squelch.vst3 into  C:\Program Files\Common Files\VST3  and rescan in your DAW.
echo.
explorer Output
pause
exit /b 0

:novs
echo  Visual Studio with C++ tools was not found.
echo  Install Visual Studio Community and tick "Desktop development with C++".
echo.
pause
exit /b 1

:fail
echo.
echo  BUILD FAILED - take a screenshot of this window and send it to Claude.
echo.
pause
exit /b 1
