@echo off
setlocal enabledelayedexpansion

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

echo ===============================================
echo         HuaEngine Quick Build Script
echo ===============================================
echo.

:: 检查是否存在 build 目录
if not exist "build" (
    echo Build directory not found! Generating project files first...
    call GenerateProjectCMake.bat
    echo.
)

:: 进入 build 目录
cd build

:: 快速构建 Debug 版本
echo Building Debug configuration...
cmake --build . --config Debug 2>&1 | findstr /c:"error" /c:"Error" /c:"ERROR" > "%TEMP%\quick_build_errors.txt"
if exist "%TEMP%\quick_build_errors.txt" (
    for /f "delims=" %%i in (%TEMP%\quick_build_errors.txt%) do (
        echo %RED%%%i%RESET%
    )
    del "%TEMP%\quick_build_errors.txt"
)

if %errorlevel% equ 0 (
    echo.
    echo ===============================================
    echo %GREEN%Quick build completed successfully!%RESET%
    echo Debug binaries: build\bin\Debug-Windows-x64\
    echo ===============================================
    
    :: 显示可用的可执行文件
    echo.
    echo Available applications:
    if exist "bin\Debug-Windows-x64\Sandbox.exe" echo   - Sandbox.exe
    if exist "bin\Debug-Windows-x64\Editor.exe" echo   - Editor.exe
    
    echo.
    echo Quick launch options:
    echo   Press 1 to run Sandbox
    echo   Press 2 to run Editor  
    echo   Press any other key to exit
    echo.
    
    choice /c 123 /n /t 10 /d 3 /m "Your choice (auto-exit in 10s): "
    
    if errorlevel 3 (
        echo Exiting...
    ) else if errorlevel 2 (
        if exist "bin\Debug-Windows-x64\Editor.exe" (
            echo Starting Editor...
            start "" "bin\Debug-Windows-x64\Editor.exe"
        )
    ) else if errorlevel 1 (
        if exist "bin\Debug-Windows-x64\Sandbox.exe" (
            echo Starting Sandbox...
            start "" "bin\Debug-Windows-x64\Sandbox.exe"
        )
    )
) else (
    echo.
    echo ===============================================
    echo %RED%Build failed! Check the output above for details.%RESET%
    echo ===============================================
)
