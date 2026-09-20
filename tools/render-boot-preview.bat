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
if not exist assets\boot_intro.arvf (
    echo [ERROR] assets\boot_intro.arvf is missing. Package the final boot film first.
    exit /b 1
)
if not exist build\boot-preview mkdir build\boot-preview
copy /y translations.tsv build\boot-preview\translations.tsv >nul
pushd build\boot-preview
rc /nologo /i ..\..\src /fo arogue.res ..\..\src\arogue.rc
if errorlevel 1 (popd & exit /b 1)
cl /nologo /std:c++17 /O2 /MT /utf-8 /GR- /W4 /DUNICODE /D_UNICODE ..\..\src\campaign.cpp ..\..\tools\boot_intro_preview.cpp ..\..\src\render.cpp ..\..\src\boot_film.cpp ..\..\src\localization.cpp ..\..\src\audio.cpp ..\..\src\music.cpp ..\..\src\game.cpp arogue.res /Fe:boot_intro_preview.exe /link /SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF user32.lib gdi32.lib winmm.lib imm32.lib windowscodecs.lib ole32.lib
if errorlevel 1 (popd & exit /b 1)
echo Rendering silent native preview: 240 BMP frames at 30 fps, 8 seconds.
.\boot_intro_preview.exe .
set "PREVIEW_RESULT=%errorlevel%"
popd
exit /b %PREVIEW_RESULT%
