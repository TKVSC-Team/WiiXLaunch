@echo off
set PATH=C:\devkitPro\devkitPPC\bin;%PATH%
rmdir /s /q build\wiixlaunch_cemu.dir
cmake -B build -DPLATFORM=CEMU
cmake --build build --target wiixlaunch_cemu
python scripts\deploy.py
