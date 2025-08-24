@echo off
setlocal enabledelayedexpansion

:: 通用构建输出过滤脚本
:: 用法: FilterBuildOutput.bat "build_command" [output_file]
:: 
:: 此脚本只显示错误信息，忽略警告，并用红色标注错误

:: 启用ANSI颜色代码支持
for /f "tokens=2 delims=[]" %%i in ('cmd /c "ver"') do set winver=%%i
if "%winver%" geq "10.0" (
    reg add HKCU\Console /v VirtualTerminalLevel /t REG_DWORD /d 1 /f >nul 2>&1
)

:: 定义颜色代码
set "RED=[91m"
set "GREEN=[92m"
set "YELLOW=[93m"
set "BLUE=[94m"
set "RESET=[0m"

:: 检查参数
if "%~1"=="" (
    echo %RED%Error: No build command specified!%RESET%
    echo Usage: FilterBuildOutput.bat "build_command" [output_file]
    exit /b 1
)

set "BUILD_COMMAND=%~1"
set "OUTPUT_FILE=%~2"
if "%OUTPUT_FILE%"=="" set "OUTPUT_FILE=%TEMP%\build_filter_output.txt"

:: 执行构建命令并过滤输出
echo Executing: %BUILD_COMMAND%
echo.

:: 执行命令并捕获输出
%BUILD_COMMAND% 2>&1 > "%OUTPUT_FILE%"
set "BUILD_RESULT=%errorlevel%"

:: 过滤错误信息
findstr /i /c:"error" /c:"Error" /c:"ERROR" /c:"fatal" /c:"Fatal" /c:"FATAL" "%OUTPUT_FILE%" > "%TEMP%\filtered_errors.txt" 2>nul

:: 显示错误信息（如果有）
if exist "%TEMP%\filtered_errors.txt" (
    for /f %%F in ('type "%TEMP%\filtered_errors.txt" ^| find /c /v ""') do set "ERROR_COUNT=%%F"
    if !ERROR_COUNT! gtr 0 (
        echo %RED%Found !ERROR_COUNT! error(s):%RESET%
        echo.
        for /f "delims=" %%i in (%TEMP%\filtered_errors.txt%) do (
            echo %RED%%%i%RESET%
        )
    )
    del "%TEMP%\filtered_errors.txt" 2>nul
)

:: 清理临时文件
if exist "%OUTPUT_FILE%" del "%OUTPUT_FILE%" 2>nul

:: 返回构建结果
exit /b %BUILD_RESULT%
