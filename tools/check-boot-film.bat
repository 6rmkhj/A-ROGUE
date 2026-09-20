@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 1
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build\boot-film-qa mkdir build\boot-film-qa
pushd build\boot-film-qa
rc /nologo /i ..\..\src /fo arogue.res ..\..\src\arogue.rc
if errorlevel 1 (popd & exit /b 1)
cl /nologo /std:c++17 /O2 /MT /utf-8 /GR- /W4 /DUNICODE /D_UNICODE ..\..\tools\boot_film_check.cpp ..\..\src\boot_film.cpp arogue.res /Fe:boot_film_check.exe /link /SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF user32.lib gdi32.lib windowscodecs.lib ole32.lib
if errorlevel 1 (popd & exit /b 1)
.\boot_film_check.exe
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
