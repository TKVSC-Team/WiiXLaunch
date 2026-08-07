@echo off
set PATH=C:\devkitPro\devkitPPC\bin;%PATH%
if exist build\wiiu rmdir /s /q build\wiiu
mkdir build\wiiu
cmake -B build/wiiu -DPLATFORM=WIIU
cmake --build build/wiiu
python scripts\deploy.py
