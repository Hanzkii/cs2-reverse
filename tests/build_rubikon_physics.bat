@echo off
setlocal
echo =================================================================
echo  Compiling Source 2 Rubikon Physics Tests (MSVC x64 C++20)
echo =================================================================

where cl.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else (
        call vcvars64.bat >nul 2>&1
    )
)

cl.exe /nologo /std:c++20 /EHsc /W4 /WX /O2 /I. tests\test_rubikon_physics.cpp /Fe:tests\test_rubikon_physics.exe
if %ERRORLEVEL% equ 0 (
    echo [OK] Compilation successful. Running Rubikon physics verification tests...
    echo -----------------------------------------------------------------
    tests\test_rubikon_physics.exe
    set TEST_ERR=!ERRORLEVEL!
    del tests\test_rubikon_physics.exe tests\test_rubikon_physics.obj >nul 2>&1
    exit /b %TEST_ERR%
) else (
    echo [FAIL] Compilation failed with error %ERRORLEVEL%.
    exit /b %ERRORLEVEL%
)
