@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "ROOT_DIR=%%~fI"

set "ACTION=build"
set "CONFIG=Debug"
set "TARGET=ALL_BUILD"
set "LAUNCH_AFTER_BUILD=0"

:parse_args
if "%~1"=="" goto args_done

if /I "%~1"=="--generate-project" (
    set "ACTION=generate"
    shift
    goto parse_args
)

if /I "%~1"=="--config" (
    if "%~2"=="" goto missing_config
    call :normalize_config "%~2"
    if errorlevel 1 exit /b 1
    shift
    shift
    goto parse_args
)

if /I "%~1"=="--target" (
    if "%~2"=="" goto missing_target
    call :normalize_target "%~2"
    if errorlevel 1 exit /b 1
    shift
    shift
    goto parse_args
)

if /I "%~1"=="--launch-after-build" (
    set "LAUNCH_AFTER_BUILD=1"
    shift
    goto parse_args
)

if /I "%~1"=="--help" goto show_help_ok
if /I "%~1"=="-h" goto show_help_ok
if /I "%~1"=="/?" goto show_help_ok

echo [Build] Unknown argument: %~1
goto show_help_error

:args_done
if /I "%ACTION%"=="generate" (
    call "%SCRIPT_DIR%\GenerateProject.bat"
    exit /b %errorlevel%
)

call "%SCRIPT_DIR%\EnsureProject.bat"
if errorlevel 1 exit /b 1

call "%SCRIPT_DIR%\RunBuild.bat" "%CONFIG%" "%TARGET%"
if errorlevel 1 exit /b %errorlevel%

if "%LAUNCH_AFTER_BUILD%"=="1" (
    call "%SCRIPT_DIR%\LaunchTarget.bat" "%CONFIG%" "%TARGET%"
    exit /b %errorlevel%
)

exit /b 0

:normalize_config
set "CONFIG_ARG=%~1"
if /I "%CONFIG_ARG%"=="debug" (
    set "CONFIG=Debug"
    exit /b 0
)
if /I "%CONFIG_ARG%"=="release" (
    set "CONFIG=Release"
    exit /b 0
)
echo [Build] Unsupported config: %~1
echo [Build] Allowed values: debug, release
exit /b 1

:normalize_target
set "TARGET_ARG=%~1"
if /I "%TARGET_ARG%"=="all" (
    set "TARGET=ALL_BUILD"
    exit /b 0
)
if /I "%TARGET_ARG%"=="huaengine" (
    set "TARGET=HuaEngine"
    exit /b 0
)
if /I "%TARGET_ARG%"=="editor" (
    set "TARGET=Editor"
    exit /b 0
)
if /I "%TARGET_ARG%"=="projecthub" (
    set "TARGET=ProjectHub"
    exit /b 0
)
echo [Build] Unsupported target: %~1
echo [Build] Allowed values: huaengine, editor, projecthub, all
exit /b 1

:missing_config
echo [Build] Missing value after --config
goto show_help_error

:missing_target
echo [Build] Missing value after --target
goto show_help_error

:show_help_ok
echo Usage:
echo   Build --generate-project
echo   Build [--config debug^|release] [--target huaengine^|editor^|projecthub^|all] [--launch-after-build]
echo.
echo Defaults:
echo   --config debug
echo   --target all
exit /b 0

:show_help_error
echo Usage:
echo   Build --generate-project
echo   Build [--config debug^|release] [--target huaengine^|editor^|projecthub^|all] [--launch-after-build]
echo.
echo Defaults:
echo   --config debug
echo   --target all
exit /b 1
