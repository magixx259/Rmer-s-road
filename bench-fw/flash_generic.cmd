@echo off
REM Universal OpenOCD flash for many probes/chips
REM Usage:
REM   flash_generic.cmd  probe  chip  file
REM probe : stlink | cmsis-dap | jlink  (OpenOCD interface cfg name)
REM chip  : stm32f4x | stm32f1x | stm32h7x | stm32f0x | stm32l4x | nrf52 ...
REM file  : path to .hex (address built-in) or .bin (needs address below)
set "OCD=D:\msys64\mingw64\bin\openocd.exe"
set "SCR=D:\msys64\mingw64\share\openocd\scripts"

if "%~3"=="" (
  echo Usage: flash_generic.cmd probe chip file
  echo   probe: stlink ^| cmsis-dap ^| jlink
  echo   chip : stm32f4x ^| stm32f1x ^| stm32h7x ^| nrf52 ^| ...
  echo   file : D:\path\firmware.hex  ^(or .bin, then edit address below^)
  pause
  exit /b 1
)

set "PROBE=%~1"
set "CHIP=%~2"
set "HEX=%~3"
echo Probe=interface/%PROBE%.cfg  Chip=target/%CHIP%.cfg
echo File=%HEX%

set "ADDR="
if /I "%~x3"==".bin" set "ADDR=0x08000000"

if defined ADDR (
  "%OCD%" -s "%SCR%" -f interface/%PROBE%.cfg -f target/%CHIP%.cfg -c "adapter speed 1800" -c "program %HEX:\=/% %ADDR% verify reset exit"
) else (
  "%OCD%" -s "%SCR%" -f interface/%PROBE%.cfg -f target/%CHIP%.cfg -c "adapter speed 1800" -c "program %HEX:\=/% verify reset exit"
)
echo.
echo Done. exit=%errorlevel%
pause
