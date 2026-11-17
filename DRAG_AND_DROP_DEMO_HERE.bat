@echo off
title CS2 3D Replay & Sub-Tick Overlay Auto-Renderer
color 0B
setlocal enabledelayedexpansion

echo ==============================================================================
echo   COUNTER-STRIKE 2: AUTOMATED 3D MOVIE & SUB-TICK REPLAY PIPELINE
echo ==============================================================================
echo.

:: 1. Check if a .dem file was passed via drag-and-drop
if "%~1"=="" (
    echo [PROMPT] Drag and drop ANY Counter-Strike 2 match recording (.dem) file
    echo          directly onto this batch icon in Windows Explorer!
    echo.
    set /p "USER_DEMO=Or type / paste the full path to a .dem file: "
    if "!USER_DEMO!"=="" (
        echo [ERROR] No file provided. Exiting.
        pause
        exit /b 1
    )
    set "TARGET_DEMO=!USER_DEMO!"
) else (
    set "TARGET_DEMO=%~1"
)

:: Strip surrounding quotes if present
set "TARGET_DEMO=!TARGET_DEMO:"=!"

if not exist "!TARGET_DEMO!" (
    echo [ERROR] Demo file not found: "!TARGET_DEMO!"
    echo Please verify the file path and try again.
    pause
    exit /b 1
)

echo [TARGET] Ingesting demo: "!TARGET_DEMO!"
echo.

:: 2. Execute automated Python master orchestrator
python tools\cs2_auto_record.py "!TARGET_DEMO!"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [WARNING] Pipeline exited with status %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [DONE] Automated render complete!
pause
