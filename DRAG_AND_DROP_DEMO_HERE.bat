@echo off
setlocal
cd /d "%~dp0"
title CS2 In-Game Sub-Tick HUD and Replay Overlay Launcher
color 0B

echo ==============================================================================
echo   COUNTER-STRIKE 2: IN-GAME SUB-TICK AND LAG COMPENSATION HUD OVERLAY
echo ==============================================================================
echo.
echo   - 100% In-Game Overlay (floats directly over CS2 window while watching demo)
echo   - Layer 1: Sub-Tick Micro-Slice Bar (15.625 ms discrete window)
echo   - Layer 2: 3D Rewound Lag Comp Hitbox (Capsule + Apex #6 Head Sphere)
echo   - Layer 3: Bullet Ray-Trace and Trajectory Vector
echo   - Layer 4: Desync and Anomaly Rollback Alerts
echo   - Layer 5: Interactive Settings Menu (Press F1 or INSERT in-game)
echo.
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
