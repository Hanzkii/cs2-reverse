@echo off
setlocal
where cl.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if defined VCVARS64 (
        call "%VCVARS64%" >nul 2>&1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else (
        call vcvars64.bat >nul 2>&1
    )
)

echo ========================================================
echo  Compiling CS2 Particle Dynamics Simulator (C++20)
echo ========================================================

cl.exe /nologo /O2 /std:c++20 /EHsc /W4 /WX cs2_particle_engine_sim.cpp /Fe:cs2_particle_engine_sim.exe
if %ERRORLEVEL% equ 0 (
    echo [BUILD SUCCESS] Running Particle Engine simulation...
    echo.
    cs2_particle_engine_sim.exe
    set SIM_RESULT=%ERRORLEVEL%
    del cs2_particle_engine_sim.exe cs2_particle_engine_sim.obj >nul 2>&1
    exit /b %SIM_RESULT%
) else (
    echo [BUILD FAILED]
    del cs2_particle_engine_sim.obj >nul 2>&1
    exit /b %ERRORLEVEL%
)
