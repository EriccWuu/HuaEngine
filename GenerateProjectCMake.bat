@echo off
echo Generating CMake project files for HuaEngine...

if not exist "build" mkdir build
cd build

echo Configuring project with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64

if %errorlevel% equ 0 (
    echo.
    echo CMake project generation completed successfully!
    echo You can now open build/HuaEngine.sln in Visual Studio
    echo Or build from command line using:
    echo   cmake --build . --config Debug
    echo   cmake --build . --config Release
) else (
    echo.
    echo CMake project generation failed!
)

pause
