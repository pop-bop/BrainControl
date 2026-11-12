@echo off
setlocal
cd /d "C:\Users\Hema Pandey\PycharmProjects\BrainControl"
title BrainControl - TinkerTech Robot Wireless Controller
echo =======================================================
echo   Starting BrainControl Wireless Robot Controller
echo =======================================================
echo.
"C:\Users\Hema Pandey\PycharmProjects\BrainControl\.venv\scripts\python.exe" main.py %*
pause
