@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
    echo [Build] Missing command to execute.
    exit /b 1
)

set "LOG_FILE=%TEMP%\huaengine-build-%RANDOM%%RANDOM%.log"

call %* >"%LOG_FILE%" 2>&1
set "BUILD_RESULT=%errorlevel%"

if exist "%LOG_FILE%" (
    findstr /i /c:" error " /c:": error" /c:"fatal error" /c:"FAILED:" /c:"CMake Error" "%LOG_FILE%"
    if errorlevel 1 if not "%BUILD_RESULT%"=="0" (
        type "%LOG_FILE%"
    )
    del "%LOG_FILE%" >nul 2>&1
)

exit /b %BUILD_RESULT%
