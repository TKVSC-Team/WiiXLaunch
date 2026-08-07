@echo off
set PATH=C:\devkitPro\devkitPPC\bin;%PATH%
if exist build\CMakeCache.txt del /f /q build\CMakeCache.txt
if exist build\CMakeFiles rmdir /s /q build\CMakeFiles
cmake -B build -DPLATFORM=CEMU
cmake --build build --target wiixlaunch_cemu
python scripts\deploy.py
