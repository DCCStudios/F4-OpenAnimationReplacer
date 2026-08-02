@echo off
setlocal
cd /d "%~dp0"

where python >nul 2>&1
if errorlevel 1 (
  echo Python was not found on PATH.
  pause
  exit /b 1
)

python -m pip install -r "%~dp0requirements.txt"
if errorlevel 1 (
  echo Failed to install dependencies.
  pause
  exit /b 1
)

python -m PyInstaller --noconfirm --clean --windowed --name F4OARConversionTool --paths "%~dp0." --collect-all customtkinter "%~dp0main.py"
if errorlevel 1 (
  echo PyInstaller failed.
  pause
  exit /b 1
)

echo.
echo Portable build: "%~dp0dist\F4OARConversionTool\F4OARConversionTool.exe"
echo You can copy the whole dist\F4OARConversionTool folder anywhere.
pause
endlocal
