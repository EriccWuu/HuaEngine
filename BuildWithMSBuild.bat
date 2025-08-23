@echo off
setlocal enabledelayedexpansion

echo ===============================================
echo      HuaEngine MSBuild Direct Build Script
echo ===============================================
echo.

:: 检查是否存在解决方案文件
if not exist "build\HuaEngine.sln" (
    echo Solution file not found! Generating project files first...
    call GenerateProjectCMake.bat
    if !errorlevel! neq 0 (
        echo Failed to generate project files!
        pause
        exit /b 1
    )
    echo.
)

:: 查找 MSBuild.exe
set "MSBUILD_PATH="
set "VS_VERSIONS=2022 2019 2017"

for %%v in (%VS_VERSIONS%) do (
    if exist "C:\Program Files\Microsoft Visual Studio\%%v\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\%%v\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        goto found_msbuild
    )
    if exist "C:\Program Files\Microsoft Visual Studio\%%v\Professional\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\%%v\Professional\MSBuild\Current\Bin\MSBuild.exe"
        goto found_msbuild
    )
    if exist "C:\Program Files\Microsoft Visual Studio\%%v\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\%%v\Community\MSBuild\Current\Bin\MSBuild.exe"
        goto found_msbuild
    )
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%v\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD_PATH=C:\Program Files (x86)\Microsoft Visual Studio\%%v\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        goto found_msbuild
    )
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%v\Professional\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD_PATH=C:\Program Files (x86)\Microsoft Visual Studio\%%v\Professional\MSBuild\Current\Bin\MSBuild.exe"
        goto found_msbuild
    )
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%v\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD_PATH=C:\Program Files (x86)\Microsoft Visual Studio\%%v\Community\MSBuild\Current\Bin\MSBuild.exe"
        goto found_msbuild
    )
)

:: 如果没找到，尝试使用 CMAKE 构建
echo MSBuild not found! Falling back to CMake build...
cd build
cmake --build . --config Debug
goto end

:found_msbuild
echo Found MSBuild at: %MSBUILD_PATH%
echo.

:: 选择构建配置
echo Choose build configuration:
echo   1. Debug
echo   2. Release
echo   3. Both
echo.
set /p CONFIG_CHOICE="Enter your choice (1-3): "

cd build

if "%CONFIG_CHOICE%"=="1" (
    echo Building Debug configuration with MSBuild...
    "%MSBUILD_PATH%" HuaEngine.sln /p:Configuration=Debug /p:Platform=x64 /m
) else if "%CONFIG_CHOICE%"=="2" (
    echo Building Release configuration with MSBuild...
    "%MSBUILD_PATH%" HuaEngine.sln /p:Configuration=Release /p:Platform=x64 /m
) else if "%CONFIG_CHOICE%"=="3" (
    echo Building Debug configuration with MSBuild...
    "%MSBUILD_PATH%" HuaEngine.sln /p:Configuration=Debug /p:Platform=x64 /m
    echo.
    echo Building Release configuration with MSBuild...
    "%MSBUILD_PATH%" HuaEngine.sln /p:Configuration=Release /p:Platform=x64 /m
) else (
    echo Invalid choice! Building Debug by default...
    "%MSBUILD_PATH%" HuaEngine.sln /p:Configuration=Debug /p:Platform=x64 /m
)

:end
if !errorlevel! equ 0 (
    echo.
    echo ===============================================
    echo Build completed successfully!
    echo ===============================================
) else (
    echo.
    echo ===============================================
    echo Build failed!
    echo ===============================================
)

echo.