@echo off
setlocal
cd /d "%~dp0"
title CS2 3D Replay and Sub-Tick Overlay Auto-Renderer
color 0B

echo ==============================================================================
echo   COUNTER-STRIKE 2: AUTOMATED 3D MOVIE AND SUB-TICK REPLAY PIPELINE
echo ==============================================================================
echo.

if "%~1"=="" (
    echo [PROMPT] Drag and drop ANY CS2 match recording .dem file onto this icon.
    echo.
    set /p "TARGET_DEMO=Or paste full path to a demo file: "
) else (
    set "TARGET_DEMO=%~1"
)

if defined TARGET_DEMO (
    set "TARGET_DEMO=%TARGET_DEMO:"=%"
)

if "%TARGET_DEMO%"=="" (
    echo [ERROR] No demo file provided. Exiting.
    echo.
    pause
    exit /b 1
)

if not exist "%TARGET_DEMO%" (
    echo [ERROR] File does not exist: "%TARGET_DEMO%"
    echo.
    pause
    exit /b 1
)

echo [TARGET] Demo: "%TARGET_DEMO%"
echo.

python "%~dp0tools\cs2_auto_record.py" "%TARGET_DEMO%"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Pipeline stopped with exit code %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [DONE] Processing complete!
pause
