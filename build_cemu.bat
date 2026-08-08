@echo off
setlocal

echo Generating config...
python scripts\generate_config.py
if errorlevel 1 exit /b 1

if not exist build mkdir build

:: Invoke devkitPPC directly instead of CMake: on machines with Visual Studio
:: installed, CMake defaults to the VS generator, which ignores the
:: powerpc-eabi-gcc toolchain settings and tries to compile PowerPC code with
:: MSVC. The Cemu target is one compile+link, so there is nothing CMake adds.
:: Relative paths keep the (possibly space-containing) repo root out of args.
echo Building Cemu payload (PowerPC)...
:: No -g: the payload ships as a raw binary (debug info is useless) and debug
:: sections add .rela.debug_* entries that the deploy-time relocator must not
:: see (it now filters them, but there is no reason to generate them at all).
C:\devkitPro\devkitPPC\bin\powerpc-eabi-g++.exe ^
  -std=gnu++20 -fPIE -msdata=none ^
  -D__CEMU__=1 -DWIIXL_CEMU=1 ^
  -I include -I build\generated\include ^
  -nostartfiles -T scripts\cemu.ld -Wl,-q ^
  src\main.cpp src\wiiu_plugin.cpp ^
  -o build\wiixlaunch_cemu
if errorlevel 1 exit /b 1

python scripts\deploy.py
echo Cemu build complete!
