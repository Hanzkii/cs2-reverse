@echo off
setlocal
where cl.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if defined VCVARS64 (
        call "%VCVARS64%" >nul 2>&1
    ) else (
        call vcvars64.bat >nul 2>&1
    )
)

echo ========================================================
echo  Compiling CS2 Volumetric Smoke Grenade Simulator (C++17)
echo ========================================================

cl.exe /O2 /std:c++17 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /Fe:cs2_volumetric_smoke_sim.exe cs2_volumetric_smoke_sim.cpp

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo  Executing Simulator Test Suite
echo ========================================================
cs2_volumetric_smoke_sim.exe
