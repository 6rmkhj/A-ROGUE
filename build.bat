@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found. Use build-msvc.bat on Windows.
    exit /b 1
)
if not exist build mkdir build
g++ -std=c++17 -Os -s -municode -mwindows -DUNICODE -D_UNICODE src\campaign.cpp src\main.cpp src\screens.cpp src\render.cpp src\localization.cpp src\audio.cpp src\music.cpp src\game.cpp -o build\AROGUE.exe -lgdi32 -lwinmm
if errorlevel 1 exit /b 1
copy /y translations.tsv build\translations.tsv >nul
g++ -std=c++17 -O2 src\campaign.cpp src\smoke.cpp src\localization.cpp src\game.cpp src\music.cpp -o build\smoke.exe -luser32
if errorlevel 1 exit /b 1
build\smoke.exe
exit /b %errorlevel%

