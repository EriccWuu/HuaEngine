@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%ROOT_DIR%\build"

if exist "%BUILD_DIR%\CMakeCache.txt" exit /b 0

echo [Build] CMake project is missing. Generating it first...
call "%SCRIPT_DIR%\GenerateProject.bat"
exit /b %errorlevel%
