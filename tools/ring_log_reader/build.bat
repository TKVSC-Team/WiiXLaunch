@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /nologo /EHsc /std:c++17 /O2 /Fe:ring_log_reader.exe main.cpp
echo BUILD_EXIT=%ERRORLEVEL%
