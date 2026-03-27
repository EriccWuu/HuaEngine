@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "CONFIG=%~1"
set "TARGET=%~2"

if "%CONFIG%"=="" (
    echo [Build] Missing launch config.
    exit /b 1
)

if "%TARGET%"=="" (
    echo [Build] Missing launch target.
    exit /b 1
)

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "ROOT_DIR=%%~fI"

if /I "%TARGET%"=="Editor" (
    set "EXE_PATH=%ROOT_DIR%\build\bin\%CONFIG%-Windows-x64\Editor.exe"
    goto launch
)

if /I "%TARGET%"=="Sandbox" (
    set "EXE_PATH=%ROOT_DIR%\build\bin\%CONFIG%-Windows-x64\Sandbox.exe"
    goto launch
)

echo [Build] Target "%TARGET%" has no launchable executable. Skipping launch.
exit /b 0

:launch
if not exist "%EXE_PATH%" (
    echo [Build] Launch target not found: "%EXE_PATH%"
    exit /b 1
)

echo [Build] Launching "%EXE_PATH%"...
start "" "%EXE_PATH%"
exit /b 0
