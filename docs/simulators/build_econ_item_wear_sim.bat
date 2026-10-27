@echo off
setlocal
echo =================================================================
echo  Compiling CS2 Econ Item Schema and Wear Degradation Simulator
echo =================================================================

where cl.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if defined VCVARS64 (
        call "%VCVARS64%" >nul 2>&1
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else (
        call vcvars64.bat >nul 2>&1
    )
)

cl.exe /nologo /std:c++20 /EHsc /W4 /WX /O2 /I..\.. cs2_econ_item_wear_sim.cpp ..\..\game\client\inventory_schema.cpp /Fe:cs2_econ_item_wear_sim.exe
if %ERRORLEVEL% equ 0 (
    echo [OK] Compilation successful. Running econ wear degradation simulation...
    echo -----------------------------------------------------------------
    cs2_econ_item_wear_sim.exe
    del cs2_econ_item_wear_sim.exe cs2_econ_item_wear_sim.obj inventory_schema.obj
) else (
    echo [FAIL] Compilation failed with error %ERRORLEVEL%.
    exit /b %ERRORLEVEL%
)
