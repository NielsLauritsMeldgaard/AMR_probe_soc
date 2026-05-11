@echo off
REM ===== CONFIGURATION (edit this section) =====
set "TASKS=task1 task2 task3 task4"
set "TIMEOUT=60"
set "XSIM_PATH="
set "TB_ROOT=..\..\vivado\viv_prj_AMR_probe_soc\viv_prj_AMR_probe_soc.sim\sim_1\behav\xsim"
set "TEST_ROOT=..\hdl\test\tb_risc_v"
REM ============================================

cd /d "%~dp0" || exit /b 1

echo.
echo ================================
echo RISC-V Test Runner
echo ================================
echo Working directory: %CD%
echo Tasks to run: %TASKS%
echo.

REM Step 1: Convert binaries to memory files
echo Step 1: Converting binary files...
echo.
for %%T in (%TASKS%) do (
    echo Converting %%T...
    py -3 bin_to_mem.py %TEST_ROOT%\%%T
)

echo.
echo Step 2: Running simulations...
echo.

REM Step 2: Run tests
set "CMD=py -3 run_risc_v_tb.py --task %TASKS% --timeout %TIMEOUT% --testbench-root %TB_ROOT% --test-root %TEST_ROOT%"
if defined XSIM_PATH set "CMD=%CMD% --xsim-path \"%XSIM_PATH%\""
%CMD%

pause
exit /b %errorlevel%
