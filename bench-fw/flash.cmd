@echo off
REM One-click flash bench.hex to STM32F427 (A board) via ST-Link
cd /d "%~dp0"
set "OCD=D:\msys64\mingw64\bin\openocd.exe"
set "SCR=D:\msys64\mingw64\share\openocd\scripts"
set "HEX=%~dp0build\bench.hex"
echo Flashing %HEX%
"%OCD%" -s "%SCR%" -f interface/stlink.cfg -f target/stm32f4x.cfg -c "adapter speed 1800" -c "program %HEX:\=/% verify reset exit"
echo.
echo Done. exit=%errorlevel%
pause
