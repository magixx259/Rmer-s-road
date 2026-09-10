@echo off
REM Build bench then flash via ST-Link/OpenOCD
cd /d "%~dp0"
echo === 1/2 Compiling ===
call build.cmd
if errorlevel 1 (
  echo BUILD FAILED - fix errors first
  pause
  exit /b 1
)
echo.
echo === 2/2 Flashing ===
call flash.cmd
