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
cl.exe /nologo /O2 /std:c++20 /EHsc /W4 /WX cs2_kv3_deserializer_sim.cpp /Fe:cs2_kv3_deserializer_sim.exe
if %ERRORLEVEL% equ 0 (
    echo [BUILD SUCCESS] Running KeyValues3 and Resource Container simulation...
    cs2_kv3_deserializer_sim.exe
    del cs2_kv3_deserializer_sim.exe cs2_kv3_deserializer_sim.obj
) else (
    echo [BUILD FAILED]
    exit /b %ERRORLEVEL%
)
