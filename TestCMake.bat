@echo off
echo Testing CMake configuration...

if exist "build" (
    echo Removing existing build directory...
    rmdir /s /q build
)

echo Creating build directory...
mkdir build
cd build

echo Running CMake configuration...
cmake .. -G "Visual Studio 17 2022" -A x64

if %errorlevel% equ 0 (
    echo.
    echo ✅ CMake configuration successful!
    echo.
    echo Testing build...
    cmake --build . --config Debug --target HuaEngine
    
    if %errorlevel% equ 0 (
        echo ✅ Build test successful!
        echo.
        echo Available targets:
        echo - HuaEngine (static library)
        echo - Sandbox (executable)
        echo - Editor (executable)
        echo - glfw (static library)
        echo - GLAD (static library)
        echo - ImGui (static library)
        echo.
        echo Build configurations:
        echo - Debug
        echo - Release  
        echo - Dist
    ) else (
        echo ❌ Build test failed!
    )
) else (
    echo ❌ CMake configuration failed!
)

echo.
echo Test completed. Press any key to continue...
pause > nul
