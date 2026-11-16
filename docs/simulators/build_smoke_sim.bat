@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)

echo ========================================================
echo  Compiling CS2 Volumetric Smoke Grenade Simulator (C++20)
echo ========================================================

cl.exe /O2 /std:c++20 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /Fe:cs2_volumetric_smoke_sim.exe cs2_volumetric_smoke_sim.cpp

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo  Executing Simulator Test Suite
echo ========================================================
cs2_volumetric_smoke_sim.exe
