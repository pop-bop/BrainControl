@echo off
setlocal
cd /d "%~dp0"
title TinkerTech ESP32-S3 Flash Utility

echo ==================================================
echo   TinkerTech S4S ESP32-S3 Flash Tool
echo ==================================================
echo.

set PYTHON_EXE=python
if exist "..\..\.venv\Scripts\python.exe" set PYTHON_EXE="..\..\.venv\Scripts\python.exe"

"%PYTHON_EXE%" flash_firmware.py %*

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo If 'esptool' was not found, install it with:
    echo   pip install esptool pyserial
    echo.
)

pause
