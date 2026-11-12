@echo off
setlocal
cd /d "%~dp0"
title TinkerTech Police Bot Steerable Teleop Cockpit

set "PY_EXE=python"
if exist ".venv\Scripts\python.exe" set "PY_EXE=.venv\Scripts\python.exe"

"%PY_EXE%" teleop.py %*

pause
