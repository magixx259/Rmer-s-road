@echo off
REM Usage: drag a .hex/.bin file onto this script, or pass path as %1
set "OCD=D:\msys64\mingw64\bin\openocd.exe"
set "SCR=D:\msys64\mingw64\share\openocd\scripts"
if "%~1"=="" (
  echo Drag your firmware file onto this script, e.g.
  echo   flash_any.cmd  D:\somewhere\firmware.hex
  pause
  exit /b 1
)
set "HEX=%~1"
echo Flashing %HEX%
"%OCD%" -s "%SCR%" -f interface/stlink.cfg -f target/stm32f4x.cfg -c "adapter speed 1800" -c "program %HEX:\=/% verify reset exit"
echo.
echo Done. exit=%errorlevel%
pause
