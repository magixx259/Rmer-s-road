@echo off
setlocal
set PATH=D:\tools\cmake\bin;D:\tools\ninja;D:\tools\arm-gnu-toolchain\bin;%PATH%
cd /d %~dp0
if not exist build mkdir build
cmake -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM=D:\tools\ninja\ninja.exe || exit /b 1
cmake --build build || exit /b 1
echo.
echo BUILD OK: %~dp0build\bench.hex
