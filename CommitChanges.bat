@echo off
echo Git Repository Cleanup and CMake Integration
echo ===========================================
echo.

echo 1. Staging .gitignore changes...
git add .gitignore

echo.
echo 2. Staging CMake configuration files...
git add CMakeLists.txt
git add cmake/
git add HuaEngine/CMakeLists.txt
git add Sandbox/CMakeLists.txt
git add Editor/CMakeLists.txt
git add Dependencies/glad/CMakeLists.txt
git add Dependencies/imgui/CMakeLists.txt
git add *.bat
git add *.md

echo.
echo 3. Staging new source files...
git add HuaEngine/src/HuaEngine/ECS/
git add HuaEngine/src/HuaEngine/Math/
git add HuaEngine/src/HuaEngine/Reflection/
git add HuaEngine/src/HuaEngine/Rendering/
git add HuaEngine/src/HuaEngine/Scene/
git add HuaEngine/src/HuaEngine/Serialization/
git add HuaEngine/src/Module/
git add Editor/
git add Sandbox/src/Console.*

echo.
echo 4. Current git status:
git status --short

echo.
echo 5. Ready to commit with the following message:
echo    "feat: Add CMake build system and improve .gitignore"
echo.
echo    - Add complete CMake configuration as alternative to Premake
echo    - Update .gitignore to exclude build artifacts and generated files
echo    - Remove tracked build outputs, project files, and binaries
echo    - Add comprehensive documentation for CMake usage
echo.

set /p choice="Do you want to commit these changes? (y/n): "
if /i "%choice%"=="y" (
    git commit -m "feat: Add CMake build system and improve .gitignore

- Add complete CMake configuration as alternative to Premake
- Support for Debug/Release/Dist configurations matching Premake
- Proper dependency management for GLFW/GLAD/ImGui
- Update .gitignore to exclude build artifacts and generated files
- Remove tracked build outputs, project files, and binaries
- Add comprehensive documentation and migration analysis
- Maintain full compatibility with existing Premake workflow

CMake Features:
- Cross-platform build support
- Better dependency management
- Modern build system with improved IDE integration
- Automatic asset copying for applications
- Precompiled header support
- Static runtime linking on Windows"
    
    echo.
    echo ✅ Changes committed successfully!
    echo.
    echo Next steps:
    echo 1. Install CMake from cmake.org
    echo 2. Run GenerateProjectCMake.bat to test the build system
    echo 3. You can now use either Premake or CMake for building
) else (
    echo.
    echo ❌ Commit cancelled. You can review the changes and commit manually.
)

echo.
echo Files that were removed from Git tracking:
echo - bin/ directory and all build outputs
echo - *.sln solution files
echo - *.vcxproj and *.vcxproj.filters project files
echo - imgui.ini configuration files
echo - All dependency build artifacts
echo.
echo These files are now ignored and won't be tracked in future commits.
echo.
pause
