@echo off
REM ============================================================
REM  Unity C unit tests: build + run + coverage
REM  Double-click this file in the unit\ folder.
REM ============================================================
setlocal

set GCC="C:\Program Files\C++\x86_64-8.1.0-release-win32-seh-rt_v6-rev0\mingw64\bin\gcc.exe"
set GCOV="C:\Program Files\C++\x86_64-8.1.0-release-win32-seh-rt_v6-rev0\mingw64\bin\gcov.exe"

echo [1/4] Clean old artifacts
del /Q *.gcda *.gcno *.gcov *.exe >nul 2>&1
del /Q ..\BSP\*.gcda ..\BSP\*.gcno >nul 2>&1

echo.
echo [2/4] Compile test_crc16
%GCC% -std=c99 -Wall -Wextra -O0 -fprofile-arcs -ftest-coverage -I. -Iunity -I..\BSP test_crc16.c unity\unity.c ..\BSP\modbus_crc.c -o test_crc16.exe -lgcov --coverage
if errorlevel 1 goto fail

echo.
echo [3/4] Compile test_frame
%GCC% -std=c99 -Wall -Wextra -O0 -fprofile-arcs -ftest-coverage -I. -Iunity -I..\BSP test_frame.c unity\unity.c ..\BSP\modbus_frame.c ..\BSP\modbus_crc.c -o test_frame.exe -lgcov --coverage
if errorlevel 1 goto fail

echo.
echo [4/4] Run tests
echo ===== CRC =====
test_crc16.exe
set RC1=%errorlevel%
echo.
echo ===== Frame =====
test_frame.exe
set RC2=%errorlevel%

echo.
echo ===== Coverage =====
%GCOV% -b -c -o . ..\BSP\modbus_frame.c
%GCOV% -b -c -o . ..\BSP\modbus_crc.c

echo.
echo Coverage detail: modbus_frame.c.gcov / modbus_crc.c.gcov
echo.
if %RC1%==0 if %RC2%==0 (
    echo ALL TESTS PASSED
) else (
    echo SOME TESTS FAILED
)
pause
exit /b 0

:fail
echo.
echo [FAILED] compile error
pause
exit /b 1
