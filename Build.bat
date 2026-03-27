@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"

set "SCRIPT_DIR=%ROOT_DIR%\Scripts"
set "HELPER=%SCRIPT_DIR%\BuildMain.bat"

if not exist "%HELPER%" (
    echo [Build] Missing helper script: "%HELPER%"
    exit /b 1
)

call "%HELPER%" %*
exit /b %errorlevel%
