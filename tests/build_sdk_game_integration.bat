@echo off
setlocal
echo =================================================================
echo  Compiling CS2 Master SDK and Game Subsystem Integration (C++20)
echo =================================================================

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

cl.exe /nologo /std:c++20 /EHsc /W4 /WX /O2 /I.. test_sdk_game_integration.cpp ..\game\animationsystem\two_bone_ik.cpp ..\game\server\hit_registration_damage.cpp ..\game\server\lagcompensation.cpp ..\game\server\bullet_penetration.cpp ..\game\server\controller_pawn.cpp ..\game\particles\smoke_voxels.cpp ..\game\particles\inferno_fire_grid.cpp ..\game\particles\particle_system.cpp ..\game\resourcesystem\kv3_parser.cpp ..\game\soundsystem\sound_ops.cpp ..\game\vphysics2\rubikon_collision.cpp ..\game\client\bone_setup.cpp ..\game\client\inventory_schema.cpp ..\game\navsystem\pathfinder.cpp ..\game\networksystem\entity_delta_codec.cpp ..\game\networksystem\flattened_serializers.cpp ..\game\networksystem\netmessage_framing.cpp ..\game\panorama\panorama_engine.cpp /Fe:test_sdk_game_integration.exe
if %ERRORLEVEL% equ 0 (
    echo [OK] Compilation successful. Running integration verification test...
    echo -----------------------------------------------------------------
    .\test_sdk_game_integration.exe
    if errorlevel 1 (
        echo [FAIL] Integration tests failed.
        del test_sdk_game_integration.exe *.obj >nul 2>&1
        exit /b 1
    )
    del test_sdk_game_integration.exe *.obj >nul 2>&1
) else (
    echo [FAIL] Compilation failed with error %ERRORLEVEL%.
    exit /b %ERRORLEVEL%
)
