# HuaEngine CMake Build System

This project has been converted from Premake to CMake while maintaining all the original functionality.

## Quick Start

### Windows (Visual Studio)
```batch
# Run the generation script
GenerateProjectCMake.bat

# Or manually:
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
```

### Manual Configuration
```bash
# Create build directory
mkdir build && cd build

# Generate project files
cmake ..

# Build the project
cmake --build . --config Debug
cmake --build . --config Release
cmake --build . --config Dist
```

## Build Configurations

The project supports three build configurations matching the original Premake setup:

- **Debug**: Debug symbols, assertions enabled, no optimization
- **Release**: Optimized build with debug symbols
- **Dist**: Optimized build without debug symbols (distribution ready)

## Project Structure

The CMake build system creates the following targets:

### Libraries
- **GLFW**: Window management library (static)
- **GLAD**: OpenGL loader (static)
- **ImGui**: Immediate GUI library (static)
- **HuaEngine**: Main engine library (static)

### Applications
- **Sandbox**: Test application
- **Editor**: Engine editor application

## Dependencies

All dependencies are included in the `Dependencies/` folder:
- **spdlog**: Logging library (header-only)
- **glfw**: Window management
- **glad**: OpenGL loader
- **imgui**: GUI library
- **glm**: Math library (header-only)
- **stb_image**: Image loading (header-only)
- **entt**: Entity Component System (header-only)

## Features

### Maintained from Premake
- ✅ Same output directory structure
- ✅ Static runtime linking (MSVC)
- ✅ UTF-8 support
- ✅ Precompiled headers for HuaEngine
- ✅ Platform-specific defines
- ✅ Configuration-specific defines
- ✅ Asset copying for applications
- ✅ All include paths and linking

### CMake Advantages
- Better cross-platform support
- More robust dependency management
- Better integration with modern IDEs
- Easier to extend and maintain
- Better handling of transitive dependencies

## Building Individual Targets

```bash
# Build only the engine
cmake --build . --target HuaEngine --config Debug

# Build only Sandbox
cmake --build . --target Sandbox --config Release

# Build only Editor
cmake --build . --target Editor --config Dist
```

## Platform Support

- ✅ Windows (Visual Studio 2019/2022)
- ✅ Linux (GCC/Clang)
- ✅ macOS (Xcode/Clang)

## Migration Notes

This CMake configuration provides identical functionality to the original Premake setup:

1. **Output directories**: `bin/{Config}-{System}-{Architecture}/{Project}/`
2. **Intermediate directories**: CMake handles this automatically
3. **Include paths**: All original include paths maintained
4. **Linking**: All library dependencies preserved
5. **Defines**: Platform and configuration defines match exactly
6. **Compiler flags**: UTF-8 support and static runtime maintained
