cmake_minimum_required(VERSION 3.16)

# Enable compile commands export for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include custom configuration
include(${CMAKE_SOURCE_DIR}/cmake/Config.cmake)

# Set the project name and version
project(HuaEngine VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Architecture check
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCHITECTURE "x64")
else()
    set(ARCHITECTURE "x86")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE})

# Set the intermediate directory for object files
set(CMAKE_OBJECT_PATH_MAX 260)
foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_BINARY_DIR}/bin/${config}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_BINARY_DIR}/bin/${config}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_BINARY_DIR}/bin/${config}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE})
endforeach()

# Include directories
set(SPDLOG_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/spdlog/include")
set(GLFW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/glfw/include")
set(GLAD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/glad/include")
set(IMGUI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/imgui")
set(GLM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/glm")
set(STB_IMAGE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/stb_image")
set(ENTT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/entt")

# Compiler specific settings
if(MSVC)
    # UTF-8 support
    add_compile_options(/utf-8)
    # Static runtime linking
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # Disable specific warnings
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Platform defines
if(WIN32)
    add_compile_definitions(HE_PLATFORM_WINDOWS)
endif()

# Configuration specific defines
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(HE_DEBUG HE_ENABLE_ASSERTS)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(HE_RELEASE)
elseif(CMAKE_BUILD_TYPE STREQUAL "Dist")
    add_compile_definitions(HE_DIST)
endif()

# Add subdirectories for dependencies
# GLFW configuration
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(Dependencies/glfw)

add_subdirectory(Dependencies/glad)
add_subdirectory(Dependencies/imgui)

# Add subdirectories for main projects
add_subdirectory(HuaEngine)
add_subdirectory(Sandbox)
add_subdirectory(Editor)

# Set Editor as the startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Editor)
