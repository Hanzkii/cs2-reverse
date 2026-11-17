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
echo  Compiling CS2 Demo Replay Simulator (C++20 MSVC x64)
echo ========================================================

cl.exe /nologo /O2 /std:c++20 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /I../.. cs2_demo_replay_sim.cpp ../../game/networksystem/demo_reader.cpp ../../game/networksystem/netmessage_framing.cpp ../../game/client/usercmd_wire.cpp /Fe:cs2_demo_replay_sim.exe

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo  Executing Demo Replay and Sub-Tick Simulator Harness
echo ========================================================
cs2_demo_replay_sim.exe %*
set SIM_EXIT_CODE=%ERRORLEVEL%

del cs2_demo_replay_sim.exe cs2_demo_replay_sim.obj demo_reader.obj netmessage_framing.obj usercmd_wire.obj >nul 2>&1

if %SIM_EXIT_CODE% neq 0 (
    echo [SIMULATOR FAILED] Exit code: %SIM_EXIT_CODE%
    exit /b %SIM_EXIT_CODE%
)

echo.
echo [SIMULATOR SUCCESS] Verification completed with exit code 0.
endlocal
