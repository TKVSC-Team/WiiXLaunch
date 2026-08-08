@echo off
:: Locates the devkitPro installation for the build scripts. Do NOT add
:: setlocal here - the variables below must propagate to the caller.
::
:: Resolution order:
::   1. DEVKITPRO_WIN environment variable (set this if devkitPro lives
::      somewhere non-standard, e.g. set DEVKITPRO_WIN=D:\devkitPro)
::   2. C:\devkitPro (the Windows installer default)
::
:: Note: the DEVKITPRO variable itself is usually the msys2-style path
:: /opt/devkitpro, which is only meaningful inside devkitPro's msys2 - it
:: cannot be used from cmd, hence the separate Windows-path variable.
::
:: Exports:
::   DKP_ROOT    - Windows path to the devkitPro root
::   DKP_BASH    - devkitPro's msys2 bash (runs make with proper env/mounts)
::   DKP_PPC_GXX - devkitPPC C++ compiler

if defined DEVKITPRO_WIN (
    set "DKP_ROOT=%DEVKITPRO_WIN%"
) else (
    set "DKP_ROOT=C:\devkitPro"
)

set "DKP_BASH=%DKP_ROOT%\msys2\usr\bin\bash.exe"
set "DKP_PPC_GXX=%DKP_ROOT%\devkitPPC\bin\powerpc-eabi-g++.exe"

if not exist "%DKP_ROOT%\" (
    echo [WiiXLaunch] devkitPro not found at "%DKP_ROOT%".
    echo [WiiXLaunch] Install it from https://devkitpro.org/wiki/Getting_Started
    echo [WiiXLaunch] or set DEVKITPRO_WIN to your install directory and retry.
    exit /b 1
)
exit /b 0
