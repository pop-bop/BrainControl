@echo off
setlocal
cd /d "%~dp0"
title Auto Firmware Uploader - TinkerTech S4S Robot

echo ========================================================
echo   Auto Firmware Uploader - TinkerTech S4S Robot
echo   (Direct NPG Lite Embedded Firmware)
echo ========================================================
echo.

:: Detect Python environment
set "PYTHON_CMD=python"
if exist ".venv\Scripts\python.exe" (
    set "PYTHON_CMD=.venv\Scripts\python.exe"
    echo [Info] Using virtual environment Python: .venv
) else (
    echo [Info] Using system Python
)

:: Ensure esptool and pyserial are present
%PYTHON_CMD% -c "import esptool, serial" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [Info] Installing required flashing tools: esptool, pyserial...
    %PYTHON_CMD% -m pip install --quiet esptool pyserial
)

echo [Info] Scanning for connected robot and uploading firmware...
echo.

%PYTHON_CMD% "tinkertech-esp32s3-firmware\binaries\flash_firmware.py" --baud 1152000 %*

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo   UPLOAD SUCCESSFUL! Robot is rebooting...
    echo ========================================================
) else (
    echo.
    echo ========================================================
    echo   UPLOAD FAILED!
    echo   1. Make sure robot power switch is ON.
    echo   2. Hold the BOOT button while resetting, then retry.
    echo ========================================================
)

echo.
pause
