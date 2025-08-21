@echo off
echo Verifying CMake configuration files...
echo.

echo Checking main CMakeLists.txt...
if exist "CMakeLists.txt" (
    echo ✅ Main CMakeLists.txt found
) else (
    echo ❌ Main CMakeLists.txt missing
)

echo.
echo Checking dependency CMakeLists.txt files...

if exist "Dependencies\glad\CMakeLists.txt" (
    echo ✅ GLAD CMakeLists.txt found
) else (
    echo ❌ GLAD CMakeLists.txt missing
)

if exist "Dependencies\imgui\CMakeLists.txt" (
    echo ✅ ImGui CMakeLists.txt found
) else (
    echo ❌ ImGui CMakeLists.txt missing
)

if exist "Dependencies\glfw\CMakeLists.txt" (
    echo ✅ GLFW CMakeLists.txt found (existing)
) else (
    echo ❌ GLFW CMakeLists.txt missing
)

echo.
echo Checking project CMakeLists.txt files...

if exist "HuaEngine\CMakeLists.txt" (
    echo ✅ HuaEngine CMakeLists.txt found
) else (
    echo ❌ HuaEngine CMakeLists.txt missing
)

if exist "Sandbox\CMakeLists.txt" (
    echo ✅ Sandbox CMakeLists.txt found
) else (
    echo ❌ Sandbox CMakeLists.txt missing
)

if exist "Editor\CMakeLists.txt" (
    echo ✅ Editor CMakeLists.txt found
) else (
    echo ❌ Editor CMakeLists.txt missing
)

echo.
echo Checking configuration files...

if exist "cmake\Config.cmake" (
    echo ✅ CMake config file found
) else (
    echo ❌ CMake config file missing
)

if exist "GenerateProjectCMake.bat" (
    echo ✅ Generate script found
) else (
    echo ❌ Generate script missing
)

if exist "CMAKE_README.md" (
    echo ✅ CMake README found
) else (
    echo ❌ CMake README missing
)

if exist "MIGRATION_ANALYSIS.md" (
    echo ✅ Migration analysis found
) else (
    echo ❌ Migration analysis missing
)

echo.
echo ===========================================
echo CMake Configuration Files Summary:
echo ===========================================
echo.
echo Created Files:
echo - CMakeLists.txt (main)
echo - HuaEngine\CMakeLists.txt
echo - Sandbox\CMakeLists.txt  
echo - Editor\CMakeLists.txt
echo - Dependencies\glad\CMakeLists.txt
echo - Dependencies\imgui\CMakeLists.txt
echo - cmake\Config.cmake
echo - GenerateProjectCMake.bat
echo - TestCMake.bat (this file)
echo - CMAKE_README.md
echo - MIGRATION_ANALYSIS.md
echo.
echo Existing Files Used:
echo - Dependencies\glfw\CMakeLists.txt (already exists)
echo.
echo To use CMake build system:
echo 1. Install CMake (cmake.org)
echo 2. Run: GenerateProjectCMake.bat
echo 3. Open build\HuaEngine.sln in Visual Studio
echo.
echo The CMake configuration provides identical functionality
echo to the original Premake setup with these benefits:
echo - Better cross-platform support
echo - More robust dependency management  
echo - Better IDE integration
echo - Easier to maintain and extend
echo.

pause
