@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /O2 /EHsc fast_scan.cpp
if %ERRORLEVEL% equ 0 (
    fast_scan.exe
)
