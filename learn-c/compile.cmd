@echo off
rem ??: compile e07 ? a07  (???????????)
set "GCC=D:\msys64\mingw64\bin\gcc.exe"
if "%~1"=="" ( echo usage: compile e00~e09 or a00~a09 & pause & exit /b 1 )
set "FOUND="
for %%F in ("%~dp0exercises\%~1*.c") do if not defined FOUND set "FOUND=%%~fF"
if not defined FOUND for %%F in ("%~dp0answers\%~1*.c") do if not defined FOUND set "FOUND=%%~fF"
if not defined FOUND ( echo file not found: %~1 & pause & exit /b 1 )
echo compiling %FOUND%
"%GCC%" -Wall -Wno-unused-variable -Wno-unused-but-set-variable -finput-charset=UTF-8 -fexec-charset=GBK -o "%~dp0_tmp.exe" "%FOUND%"
if errorlevel 1 ( echo ???? & pause & exit /b 1 )
"%~dp0_tmp.exe"
del "%~dp0_tmp.exe" 2>nul
pause



