@echo off
setlocal
where cl.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if defined VCVARS64 (
        call "%VCVARS64%" >nul 2>&1
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else (
        call vcvars64.bat >nul 2>&1
    )
)

echo ========================================================
echo  Compiling CS2 Inferno Fire Grid Simulator (C++20)
echo ========================================================

cl.exe /nologo /O2 /std:c++20 /EHsc /W4 /WX /D_CRT_SECURE_NO_WARNINGS /Fe:cs2_inferno_fire_sim.exe cs2_inferno_fire_sim.cpp

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo  Executing Simulator Test Suite
echo ========================================================
cs2_inferno_fire_sim.exe
del cs2_inferno_fire_sim.exe cs2_inferno_fire_sim.obj >nul 2>&1
