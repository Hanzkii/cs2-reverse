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
cl.exe /O2 /std:c++17 /EHsc cs2_weapon_spread_recoil_sim.cpp
if %ERRORLEVEL% equ 0 (
    echo [BUILD SUCCESS] Running weapon spread & recoil simulation...
    cs2_weapon_spread_recoil_sim.exe
) else (
    echo [BUILD FAILED] MSVC compiler (cl.exe) not found or compilation error.
)
