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
echo         HuaEngine Visual Studio Build Script
echo ===============================================
echo.

:: 检查是否存在 build 目录和解决方案文件
if not exist "build" (
    echo Error: Build directory not found!
    echo Please run GenerateProjectCMake.bat first to generate project files.
    echo.
    pause
    exit /b 1
)

if not exist "build\HuaEngine.sln" (
    echo Error: Visual Studio solution file not found!
    echo Please run GenerateProjectCMake.bat first to generate project files.
    echo.
    pause
    exit /b 1
)

:: 设置默认值
set BUILD_CONFIG=Debug
set BUILD_TARGET=ALL_BUILD
set CLEAN_BUILD=false

:: 显示菜单
:MENU
echo.
echo Choose build configuration:
echo   1. Debug (default)
echo   2. Release
echo   3. Both Debug and Release
echo.
set /p CONFIG_CHOICE="Enter your choice (1-3, or press Enter for default): "

if "%CONFIG_CHOICE%"=="" set CONFIG_CHOICE=1

if "%CONFIG_CHOICE%"=="1" (
    set BUILD_CONFIG=Debug
    goto TARGET_MENU
) else if "%CONFIG_CHOICE%"=="2" (
    set BUILD_CONFIG=Release
    goto TARGET_MENU
) else if "%CONFIG_CHOICE%"=="3" (
    set BUILD_CONFIG=Both
    goto TARGET_MENU
) else (
    echo Invalid choice! Please try again.
    goto MENU
)

:TARGET_MENU
echo.
echo Choose target to build:
echo   1. All Projects (ALL_BUILD)
echo   2. HuaEngine Library only
echo   3. Sandbox Application
echo   4. Editor Application
echo   5. Clean and Rebuild All
echo.
set /p TARGET_CHOICE="Enter your choice (1-5, or press Enter for default): "

if "%TARGET_CHOICE%"=="" set TARGET_CHOICE=1

if "%TARGET_CHOICE%"=="1" (
    set BUILD_TARGET=ALL_BUILD
) else if "%TARGET_CHOICE%"=="2" (
    set BUILD_TARGET=HuaEngine
) else if "%TARGET_CHOICE%"=="3" (
    set BUILD_TARGET=Sandbox
) else if "%TARGET_CHOICE%"=="4" (
    set BUILD_TARGET=Editor
) else if "%TARGET_CHOICE%"=="5" (
    set BUILD_TARGET=ALL_BUILD
    set CLEAN_BUILD=true
) else (
    echo Invalid choice! Please try again.
    goto TARGET_MENU
)

echo.
echo ===============================================
echo Build Settings:
echo   Configuration: %BUILD_CONFIG%
echo   Target: %BUILD_TARGET%
if "%CLEAN_BUILD%"=="true" echo   Clean Build: Yes
echo ===============================================
echo.

:: 进入 build 目录
cd build

:: 清理构建（如果选择）
if "%CLEAN_BUILD%"=="true" (
    echo Cleaning previous build...
    if "%BUILD_CONFIG%"=="Both" (
        cmake --build . --target clean --config Debug 2>&1 | findstr /i "error"
        cmake --build . --target clean --config Release 2>&1 | findstr /i "error"
    ) else (
        cmake --build . --target clean --config %BUILD_CONFIG% 2>&1 | findstr /i "error"
    )
    echo.
)

:: 开始构建
if "%BUILD_CONFIG%"=="Both" (
    echo Building Debug configuration...
    cmake --build . --target %BUILD_TARGET% --config Debug 2>&1 | findstr /c:"error" /c:"Error" /c:"ERROR" > "%TEMP%\build_errors_debug.txt"
    if exist "%TEMP%\build_errors_debug.txt" (
        for /f "delims=" %%i in (%TEMP%\build_errors_debug.txt%) do (
            echo %RED%%%i%RESET%
        )
        del "%TEMP%\build_errors_debug.txt"
    )
    set DEBUG_RESULT=!errorlevel!
    
    echo.
    echo Building Release configuration...
    cmake --build . --target %BUILD_TARGET% --config Release 2>&1 | findstr /c:"error" /c:"Error" /c:"ERROR" > "%TEMP%\build_errors_release.txt"
    if exist "%TEMP%\build_errors_release.txt" (
        for /f "delims=" %%i in (%TEMP%\build_errors_release.txt%) do (
            echo %RED%%%i%RESET%
        )
        del "%TEMP%\build_errors_release.txt"
    )
    set RELEASE_RESULT=!errorlevel!
    
    echo.
    echo ===============================================
    echo Build Results:
    if !DEBUG_RESULT! equ 0 (
        echo   %GREEN%Debug: SUCCESS%RESET%
    ) else (
        echo   %RED%Debug: FAILED%RESET%
    )
    
    if !RELEASE_RESULT! equ 0 (
        echo   %GREEN%Release: SUCCESS%RESET%
    ) else (
        echo   %RED%Release: FAILED%RESET%
    )
    echo ===============================================
    
    if !DEBUG_RESULT! equ 0 if !RELEASE_RESULT! equ 0 (
        echo.
        echo %GREEN%All builds completed successfully!%RESET%
        echo Debug binaries: build\bin\Debug-Windows-x64\
        echo Release binaries: build\bin\Release-Windows-x64\
    ) else (
        echo.
        echo %RED%Some builds failed! Check the output above for details.%RESET%
    )
) else (
    echo Building %BUILD_CONFIG% configuration...
    cmake --build . --target %BUILD_TARGET% --config %BUILD_CONFIG% 2>&1 | findstr /c:"error" /c:"Error" /c:"ERROR" > "%TEMP%\build_errors.txt"
    if exist "%TEMP%\build_errors.txt" (
        for /f "delims=" %%i in (%TEMP%\build_errors.txt%) do (
            echo %RED%%%i%RESET%
        )
        del "%TEMP%\build_errors.txt"
    )
    
    if !errorlevel! equ 0 (
        echo.
        echo ===============================================
        echo %GREEN%Build completed successfully!%RESET%
        echo Binaries: build\bin\%BUILD_CONFIG%-Windows-x64\
        echo ===============================================
        
        :: 显示生成的可执行文件
        if exist "bin\%BUILD_CONFIG%-Windows-x64\Sandbox.exe" (
            echo.
            echo Available executables:
            echo   Sandbox: bin\%BUILD_CONFIG%-Windows-x64\Sandbox.exe
        )
        if exist "bin\%BUILD_CONFIG%-Windows-x64\Editor.exe" (
            echo   Editor: bin\%BUILD_CONFIG%-Windows-x64\Editor.exe
        )
    ) else (
        echo.
        echo ===============================================
        echo %RED%Build failed! Check the output above for details.%RESET%
        echo ===============================================
    )
)

echo.
echo Do you want to run an application?
echo   1. Run Sandbox
echo   2. Run Editor
echo   3. Open output directory
echo   4. Exit
echo.
set /p RUN_CHOICE="Enter your choice (1-4): "

if "%RUN_CHOICE%"=="1" (
    if "%BUILD_CONFIG%"=="Both" set BUILD_CONFIG=Debug
    if exist "bin\%BUILD_CONFIG%-Windows-x64\Sandbox.exe" (
        echo.
        echo Starting Sandbox...
        start "" "bin\%BUILD_CONFIG%-Windows-x64\Sandbox.exe"
    ) else (
        echo Sandbox.exe not found! Make sure the build was successful.
    )
) else if "%RUN_CHOICE%"=="2" (
    if "%BUILD_CONFIG%"=="Both" set BUILD_CONFIG=Debug
    if exist "bin\%BUILD_CONFIG%-Windows-x64\Editor.exe" (
        echo.
        echo Starting Editor...
        start "" "bin\%BUILD_CONFIG%-Windows-x64\Editor.exe"
    ) else (
        echo Editor.exe not found! Make sure the build was successful.
    )
) else if "%RUN_CHOICE%"=="3" (
    if "%BUILD_CONFIG%"=="Both" set BUILD_CONFIG=Debug
    echo.
    echo Opening output directory...
    start "" "bin\%BUILD_CONFIG%-Windows-x64\"
)

echo.