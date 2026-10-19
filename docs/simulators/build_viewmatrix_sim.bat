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
cl.exe /O2 /std:c++17 /EHsc cs2_view_matrix_sim.cpp
if %ERRORLEVEL% equ 0 (
    echo [BUILD SUCCESS] Running ViewMatrix simulation...
    cs2_view_matrix_sim.exe
) else (
    echo [BUILD FAILED]
)
