@echo off
setlocal

echo Generating config...
python scripts\generate_config.py
if errorlevel 1 exit /b 1

:: devkitPro's make rules cannot handle spaces in paths, so stage the build in
:: %TEMP% (space-free) instead of building in-place.
set STAGE=%TEMP%\wiixlaunch-switch
echo Preparing Switch build environment in %STAGE%...
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"

:: Copy exlaunch template and generated configs into the stage
xcopy /s /e /y /i vendor\exlaunch "%STAGE%" > nul
copy /y build\generated\switch\config.json "%STAGE%\config.json" > nul
copy /y build\generated\switch\config.mk "%STAGE%\config.mk" > nul

:: Copy our source files into the exlaunch source tree
if not exist "%STAGE%\source\wiixlaunch" mkdir "%STAGE%\source\wiixlaunch"
xcopy /s /e /y /i src\* "%STAGE%\source\wiixlaunch" > nul

:: Delete exlaunch template main.cpp to avoid multiple definition conflict with our src/main.cpp
del /f /q "%STAGE%\source\program\main.cpp"

:: Fix GCC anonymous struct typedef error in exlaunch
powershell -Command "(Get-Content '%STAGE%\source\lib\hook\nx64\hook_impl.cpp') -replace 'typedef struct \{', 'struct context {' -replace '\} context;', '};' | Set-Content '%STAGE%\source\lib\hook\nx64\hook_impl.cpp'"

:: Build via devkitPro's msys2 so DEVKITA64 and the switch rules resolve
echo Building for Switch (ARM64)...
set STAGEFWD=%STAGE:\=/%
C:\devkitPro\msys2\usr\bin\bash.exe -lc "cd '%STAGEFWD%' && make"
if errorlevel 1 exit /b 1

:: Extract artifacts
if not exist build\switch mkdir build\switch
copy /y "%STAGE%\deploy\subsdk9" build\switch\subsdk9 > nul
copy /y "%STAGE%\deploy\main.npdm" build\switch\main.npdm > nul

python scripts\deploy.py
echo Switch build complete!
