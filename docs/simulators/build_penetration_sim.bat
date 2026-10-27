@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)
cl.exe /O2 /std:c++20 /EHsc cs2_bullet_penetration_sim.cpp
if errorlevel 0 (
    cs2_bullet_penetration_sim.exe
)
endlocal
