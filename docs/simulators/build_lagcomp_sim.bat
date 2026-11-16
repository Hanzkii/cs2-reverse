@echo off
setlocal
where cl.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if defined VCVARS64 (
        call "%VCVARS64%" >nul 2>&1
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else (
        call vcvars64.bat >nul 2>&1
    )
)
cl.exe /nologo /O2 /std:c++20 /EHsc /W4 cs2_lag_compensation_sim.cpp /Fe:cs2_lag_compensation_sim.exe
if %ERRORLEVEL% equ 0 (
    echo [BUILD SUCCESS] Running simulation...
    cs2_lag_compensation_sim.exe
    del cs2_lag_compensation_sim.exe cs2_lag_compensation_sim.obj
) else (
    echo [BUILD FAILED]
    exit /b %ERRORLEVEL%
)
