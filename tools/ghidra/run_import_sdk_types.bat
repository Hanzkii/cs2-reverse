@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: Source 2 CS2 Headless SDK Type Importer Batch Runner
:: Synchronizes all 2,276 SchemaSystem classes and C++20 entity types into Ghidra.
::
:: Usage:
::   run_import_sdk_types.bat [GHIDRA_HOME] [PROJECT_DIR] [PROJECT_NAME] [FOLDER] [TARGET_BINARY]
::
:: Environment Variables (optional overrides):
::   GHIDRA_HOME        - Path to Ghidra root
::   CS2_DISASSEMBLY_DIR - Path to CS2 Ghidra project root
:: ============================================================================

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%..\..\"

set "FIRST_ARG=%~1"
if not "%FIRST_ARG:~0,2%"=="--" (
    set "GHIDRA_HOME_VAL=%~1"
    shift
)
if "%GHIDRA_HOME_VAL%"=="" set "GHIDRA_HOME_VAL=%GHIDRA_HOME%"

set "SECOND_ARG=%~1"
if not "%SECOND_ARG:~0,2%"=="--" (
    set "PROJ_DIR_VAL=%~1"
    shift
)
if "%PROJ_DIR_VAL%"=="" set "PROJ_DIR_VAL=%CS2_DISASSEMBLY_DIR%"

set "THIRD_ARG=%~1"
if not "%THIRD_ARG:~0,2%"=="--" (
    set "PROJ_NAME_VAL=%~1"
    shift
)
if "%PROJ_NAME_VAL%"=="" set "PROJ_NAME_VAL=CS2"

set "FOURTH_ARG=%~1"
if not "%FOURTH_ARG:~0,2%"=="--" (
    set "FOLDER_VAL=%~1"
    shift
)
if "%FOLDER_VAL%"=="" set "FOLDER_VAL=2026-11-11_26598758"

set "FIFTH_ARG=%~1"
if not "%FIFTH_ARG:~0,2%"=="--" (
    set "TARGET_BIN_VAL=%~1"
    shift
)
if "%TARGET_BIN_VAL%"=="" set "TARGET_BIN_VAL=all"

if not defined GHIDRA_HOME_VAL (
    echo [ERROR] GHIDRA_HOME is not set and could not be detected.
    echo Please set GHIDRA_HOME or pass it as the first parameter.
    exit /b 1
)

if not defined PROJ_DIR_VAL (
    echo [ERROR] CS2_DISASSEMBLY_DIR is not set and could not be detected.
    echo Please set CS2_DISASSEMBLY_DIR or pass it as the second parameter.
    exit /b 1
)

python "%SCRIPT_DIR%import_sdk_types.py" ^
    --ghidra-home "%GHIDRA_HOME_VAL%" ^
    --project-dir "%PROJ_DIR_VAL%" ^
    --project-name "%PROJ_NAME_VAL%" ^
    --version-folder "%FOLDER_VAL%" ^
    --target-binary "%TARGET_BIN_VAL%" %*

exit /b %ERRORLEVEL%
