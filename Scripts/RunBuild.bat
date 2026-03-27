@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "CONFIG=%~1"
set "TARGET=%~2"

if "%CONFIG%"=="" (
    echo [Build] Missing build config.
    exit /b 1
)

if "%TARGET%"=="" (
    echo [Build] Missing build target.
    exit /b 1
)

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%ROOT_DIR%\build"

echo [Build] Building target "%TARGET%" with config "%CONFIG%"...
call "%SCRIPT_DIR%\FilterBuildOutput.bat" cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target "%TARGET%"
exit /b %errorlevel%
