@echo off
echo Generating config...
python scripts\generate_config.py
if errorlevel 1 exit /b 1

echo Preparing Switch build environment...
if not exist build\switch mkdir build\switch
if exist build\switch\exlaunch rmdir /s /q build\switch\exlaunch
mkdir build\switch\exlaunch

:: Copy exlaunch template to build/switch/exlaunch
xcopy /s /e /y /i vendor\exlaunch build\switch\exlaunch > nul

:: Copy generated configs
copy /y build\generated\switch\config.json build\switch\exlaunch\config.json > nul
copy /y build\generated\switch\config.mk build\switch\exlaunch\config.mk > nul

:: Copy our source files into the exlaunch source tree
if not exist build\switch\exlaunch\source\wiixlaunch mkdir build\switch\exlaunch\source\wiixlaunch
xcopy /s /e /y /i src\* build\switch\exlaunch\source\wiixlaunch > nul

:: Delete exlaunch template main.cpp to avoid multiple definition conflict with our src/main.cpp
del /f /q build\switch\exlaunch\source\program\main.cpp

:: Fix GCC anonymous struct typedef error in exlaunch
powershell -Command "(Get-Content build\switch\exlaunch\source\lib\hook\nx64\hook_impl.cpp) -replace 'typedef struct \{', 'struct context {' -replace '\} context;', '};' | Set-Content build\switch\exlaunch\source\lib\hook\nx64\hook_impl.cpp"

:: Run make
echo Building for Switch (ARM64)...
cd build\switch\exlaunch
make
if errorlevel 1 (
    cd ..\..\..
    exit /b 1
)

:: Extract artifacts
cd ..\..\..
copy /y build\switch\exlaunch\deploy\subsdk9 build\switch\subsdk9 > nul
copy /y build\switch\exlaunch\deploy\main.npdm build\switch\main.npdm > nul

python scripts\deploy.py
echo Switch build complete!
