@echo off
setlocal
echo ========================================================================
echo Building CS2 Game Events Simulator (C++20 MSVC x64)
echo ========================================================================

cl.exe /std:c++20 /O2 /EHsc /W4 /WX /Fe:cs2_game_events_sim.exe cs2_game_events_sim.cpp
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Build succeeded: cs2_game_events_sim.exe
echo Running verification simulation...
cs2_game_events_sim.exe
del /Q *.obj cs2_game_events_sim.exe >nul 2>&1
