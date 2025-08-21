# Premake到CMake迁移分析

## 项目结构分析

### 原Premake配置概述

这个HuaEngine项目使用Premake5构建系统，包含以下组件：

#### 工作区配置 (premake5.lua)
- **架构**: x64
- **配置**: Debug, Release, Dist
- **输出目录**: `bin/{config}-{system}-{arch}/{project}`
- **中间目录**: `bin-int/{config}-{system}-{arch}/{project}`

#### 项目结构
1. **HuaEngine** - 静态库 (C++17)
   - 核心引擎代码
   - 预编译头: enginepch.h
   - 依赖: GLFW, GLAD, ImGui, OpenGL
   - 包含: spdlog, glm, stb_image, entt

2. **Sandbox** - 控制台应用 (C++17)
   - 测试应用程序
   - 依赖: HuaEngine, ImGui

3. **Editor** - 控制台应用 (C++17)
   - 引擎编辑器
   - 依赖: HuaEngine, ImGui

4. **第三方库**:
   - **GLFW** - 窗口管理库
   - **GLAD** - OpenGL加载器
   - **ImGui** - 即时GUI库

### 依赖关系图
```
┌─────────────────┐    ┌─────────────────┐
│     Sandbox     │    │     Editor      │
└─────────┬───────┘    └─────────┬───────┘
          │                      │
          └──────────┬───────────┘
                     │
             ┌───────▼───────┐
             │   HuaEngine   │
             └───────┬───────┘
                     │
        ┌────────────┼────────────┐
        │            │            │
    ┌───▼───┐   ┌────▼────┐   ┌───▼───┐
    │ GLFW  │   │  GLAD   │   │ ImGui │
    └───────┘   └─────────┘   └───────┘
```

## CMake实现对比

### 功能对等性

| 功能 | Premake5 | CMake | 状态 |
|------|----------|-------|------|
| 多配置支持 | Debug/Release/Dist | ✅ | 完全支持 |
| 静态库构建 | ✅ | ✅ | 完全支持 |
| 预编译头 | ✅ | ✅ | 完全支持 |
| 平台检测 | ✅ | ✅ | 完全支持 |
| 输出目录控制 | ✅ | ✅ | 完全支持 |
| 依赖管理 | ✅ | ✅ | 更强大 |
| 编译器标志 | ✅ | ✅ | 完全支持 |
| 静态运行时 | ✅ | ✅ | 完全支持 |

### 原Premake配置要点

#### 1. 工作区级别设置
```lua
workspace "HuaEngine"
    architecture "x64"
    configurations { "Debug", "Release", "Dist" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
```

#### 2. 包含目录定义
```lua
IncludeDirs = {}
IncludeDirs["spdlog"] = "%{wks.location}/Dependencies/spdlog/include"
IncludeDirs["glfw"] = "%{wks.location}/Dependencies/glfw/include"
-- ... 其他目录
```

#### 3. HuaEngine项目配置
```lua
project "HuaEngine"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"
    
    pchheader ("enginepch.h")
    pchsource ("%{prj.name}/src/enginepch.cpp")
```

#### 4. 平台特定设置
```lua
filter "system:windows"
    systemversion "latest"
    defines {
        "HE_PLATFORM_WINDOWS",
        "HE_BUILD_DLL",
        "GLFW_INCLUDE_NONE"
    }
```

### CMake实现优势

#### 1. 更好的跨平台支持
- 自动检测编译器和平台
- 更强大的Generator支持
- 更好的IDE集成

#### 2. 现代化的依赖管理
```cmake
target_link_libraries(HuaEngine
    PUBLIC
        glfw
        GLAD
        ImGui
)
```

#### 3. 传递性依赖处理
```cmake
target_include_directories(HuaEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:src>
)
```

#### 4. 自动资源复制
```cmake
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:Sandbox>/assets"
)
```

## 迁移步骤

### 1. 创建主CMakeLists.txt
- 定义项目和版本
- 设置C++标准
- 配置输出目录
- 定义包含目录变量

### 2. 处理第三方依赖
- GLFW: 使用现有CMakeLists.txt
- GLAD: 创建简单的静态库
- ImGui: 创建静态库

### 3. 配置主引擎库
- 收集所有源文件
- 设置包含目录
- 配置预编译头
- 链接依赖库

### 4. 配置应用程序
- Sandbox和Editor作为可执行文件
- 设置正确的依赖关系
- 配置资源复制

### 5. 平台特定处理
- Windows: MSVC设置
- Linux: GCC设置
- macOS: Clang设置

## 构建方式对比

### Premake5
```batch
# 生成项目文件
premake5 vs2022

# 在Visual Studio中构建
# 或使用MSBuild
msbuild HuaEngine.sln /p:Configuration=Debug
```

### CMake
```batch
# 生成项目文件
cmake -B build -G "Visual Studio 17 2022" -A x64

# 构建
cmake --build build --config Debug

# 或在Visual Studio中打开 build/HuaEngine.sln
```

## 总结

CMake实现提供了与原Premake配置完全相同的功能，同时带来了：

### 优势
- ✅ 更好的跨平台支持
- ✅ 更强大的依赖管理
- ✅ 更好的IDE集成
- ✅ 更现代的构建系统
- ✅ 更活跃的社区支持
- ✅ 更好的第三方库集成

### 保持的功能
- ✅ 相同的输出目录结构
- ✅ 相同的项目依赖关系
- ✅ 相同的编译器设置
- ✅ 相同的平台定义
- ✅ 相同的构建配置

这个CMake配置可以完全替代原有的Premake配置，同时提供更好的维护性和扩展性。
