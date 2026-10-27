@echo off
setlocal

echo ===================================================
echo Building CS2 Skeleton Bone Hierarchy Simulator
echo ===================================================

where cl.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if defined VCVARS64 (
        call "%VCVARS64%" >nul 2>&1
    ) else (
        call vcvars64.bat >nul 2>&1
    )
)

cl.exe /O2 /std:c++20 /EHsc /W4 /Fe:cs2_skeleton_animation_sim.exe cs2_skeleton_animation_sim.cpp
if %ERRORLEVEL% neq 0 (
    echo Compilation failed!
    exit /b 1
)

echo.
echo Running Verification Simulator...
echo ---------------------------------------------------
cs2_skeleton_animation_sim.exe
