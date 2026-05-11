@echo off
REM RISC-V UART Bootloader Upload
REM Edit PROGRAM, PORT, and BAUD below as needed

REM ===== CONFIGURATION =====
REM set "PROGRAM=programs/my_gcd.bin"
REM set "PROGRAM=//wsl.localhost/Ubuntu-24.04/home/nielslaurits/riscv-C/main.bin"
set "PROGRAM=..\sw\build\firmware.bin"
set "PORT=COM4"
set "BAUD=115200"
REM ==========================

cd /d "%~dp0" || exit /b 1

echo.
echo ================================
echo RISC-V UART Bootloader Upload
echo ================================
echo Program: %PROGRAM%
echo Port: %PORT%
echo Baud: %BAUD%
echo.

py -3 write_bin_via_serial.py %PROGRAM% --port %PORT% --baud %BAUD%

echo.
pause
exit /b %errorlevel%