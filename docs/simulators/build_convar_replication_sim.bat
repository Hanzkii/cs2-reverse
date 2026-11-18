@echo off
setlocal
echo ========================================================================
echo Building CS2 ConVar Replication Simulator (C++20 MSVC x64)
echo ========================================================================

cl.exe /std:c++20 /O2 /EHsc /W4 /WX /Fe:cs2_convar_replication_sim.exe cs2_convar_replication_sim.cpp
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Build succeeded: cs2_convar_replication_sim.exe
echo Running verification simulation...
cs2_convar_replication_sim.exe
del /Q *.obj cs2_convar_replication_sim.exe >nul 2>&1
