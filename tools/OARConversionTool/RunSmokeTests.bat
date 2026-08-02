@echo off
setlocal
cd /d "%~dp0"
echo === smoke_test.py ===
python "%~dp0smoke_test.py"
if errorlevel 1 goto :fail
echo.
echo === verification_test.py ===
python "%~dp0verification_test.py"
if errorlevel 1 goto :fail
echo.
echo All test suites passed.
pause
exit /b 0
:fail
echo.
echo TESTS FAILED.
pause
exit /b 1
