@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 1
set "VSROOT="
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\audio-qa mkdir build\audio-qa
pushd build\audio-qa
cl /nologo /std:c++17 /O2 /MT /utf-8 /GR- /W4 ..\..\tools\audio_check.cpp ..\..\src\audio.cpp ..\..\src\music.cpp /Fe:audio_check.exe /link /SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF user32.lib winmm.lib
if errorlevel 1 (popd & exit /b 1)
if "%~1"=="--export" (
    .\audio_check.exe --export
) else (
    .\audio_check.exe
)
set "CHECK_RESULT=%errorlevel%"
popd
exit /b %CHECK_RESULT%
