@echo off
setlocal

call scripts\devkitpro_env.bat
if errorlevel 1 exit /b 1

echo Generating config...
python scripts\generate_config.py
if errorlevel 1 exit /b 1

:: devkitPro's make rules cannot handle spaces in paths, so stage the build in
:: %TEMP% (space-free) instead of building in-place. See scripts/wiiu/Makefile.
set STAGE=%TEMP%\wiixlaunch-wiiu
echo Preparing Wii U build environment in %STAGE%...
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
xcopy /s /e /y /i src "%STAGE%\src" > nul
xcopy /s /e /y /i include "%STAGE%\include" > nul
xcopy /s /e /y /i build\generated\include "%STAGE%\generated\include" > nul

:: Optional WiiXLaunch modules (e.g. vendor/wiixlaunch-botw) - not part of
:: base WiiXLaunch, staged here if this mod added one as a submodule
:: (git submodule add <url> vendor/wiixlaunch-<name>). scripts/wiiu/Makefile
:: picks up modules/*/include automatically.
for /d %%G in (vendor\wiixlaunch-*) do (
    if exist "%%G\include" xcopy /s /e /y /i "%%G\include" "%STAGE%\modules\%%~nxG\include" > nul
)

copy /y scripts\wiiu\Makefile "%STAGE%\Makefile" > nul

:: Build via devkitPro's msys2 so DEVKITPRO/elf2rpl/wups_rules all resolve.
:: Requires: dkp-pacman packages wut + wut-tools, and WUPS + libfunctionpatcher
:: installed from vendor/ (make install into /opt/devkitpro/wups and /wums).
echo Building for Wii U (PowerPC)...
set STAGEFWD=%STAGE:\=/%
"%DKP_BASH%" -lc "cd '%STAGEFWD%' && make"
if errorlevel 1 exit /b 1

if not exist build\wiiu mkdir build\wiiu
copy /y "%STAGE%\BotW_SampleMod.wps" build\wiiu\BotW_SampleMod.wps > nul
if errorlevel 1 exit /b 1

python scripts\deploy.py
echo Wii U build complete!
