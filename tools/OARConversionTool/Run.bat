@echo off
setlocal
cd /d "%~dp0"

where python >nul 2>&1
if errorlevel 1 (
  echo Python was not found on PATH.
  echo Install Python 3.10+ from https://www.python.org/ or use the portable build via build_exe.bat
  pause
  exit /b 1
)

python -c "import customtkinter" >nul 2>&1
if errorlevel 1 (
  echo Installing dependencies...
  python -m pip install -r "%~dp0requirements.txt"
  if errorlevel 1 (
    echo Failed to install dependencies.
    pause
    exit /b 1
  )
)

python "%~dp0main.py"
if errorlevel 1 pause
endlocal
