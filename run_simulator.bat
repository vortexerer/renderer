@echo off
title DSHS VR Game Engine Simulator Launcher
echo ==================================================
echo   DSHS VR Game Engine Visual Simulator Launcher
echo ==================================================
echo.
echo Running visual_engine_simulator.py using Python 3.12...
echo.
C:\Users\Moonc\AppData\Local\Programs\Python\Python312\python.exe Tests/visual_engine_simulator.py
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Failed to run visual_engine_simulator.py.
    echo Trying fallback launcher (py command)...
    echo.
    py Tests/visual_engine_simulator.py
)
pause
