@echo off
setlocal
cd /d "C:\Users\D&T\BrainControl"
title BrainControl - TinkerTech Robot Wireless Controller
echo =======================================================
echo   Starting BrainControl Wireless Robot Controller
echo =======================================================
echo.
"C:\Users\D&T\BrainControl\.venv\Scripts\python.exe" main.py %*
pause
