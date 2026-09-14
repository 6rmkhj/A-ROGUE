@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 1
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\narrative-qa mkdir build\narrative-qa
copy /y translations.tsv build\narrative-qa\translations.tsv >nul
pushd build\narrative-qa
cl /nologo /std:c++17 /O2 /MT /utf-8 /GR- /W4 /DUNICODE /D_UNICODE ..\..\src\campaign.cpp ..\..\tools\narrative_check.cpp ..\..\src\render.cpp ..\..\src\localization.cpp ..\..\src\audio.cpp ..\..\src\music.cpp ..\..\src\game.cpp /Fe:narrative_check.exe /link /SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF user32.lib gdi32.lib winmm.lib
if errorlevel 1 (popd & exit /b 1)
if "%~1"=="--render" (
    if not exist frames mkdir frames
    .\narrative_check.exe frames
) else (
    .\narrative_check.exe
)
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
