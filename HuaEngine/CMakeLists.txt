# HuaEngine Static Library
cmake_minimum_required(VERSION 3.16)

project(HuaEngine LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files
file(GLOB_RECURSE HUAENGINE_SOURCES
    "src/*.cpp"
    "src/*.h"
)

# Collect STB Image files
file(GLOB STB_IMAGE_SOURCES
    "${STB_IMAGE_INCLUDE_DIR}/*.cpp"
    "${STB_IMAGE_INCLUDE_DIR}/*.h"
)

# Collect ENTT files
file(GLOB_RECURSE ENTT_SOURCES
    "${ENTT_INCLUDE_DIR}/*.hpp"
)

# Create the HuaEngine library
add_library(HuaEngine STATIC
    ${HUAENGINE_SOURCES}
    ${STB_IMAGE_SOURCES}
    ${ENTT_SOURCES}
)

# Include directories
target_include_directories(HuaEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:src>
    PRIVATE
        ${SPDLOG_INCLUDE_DIR}
        ${GLFW_INCLUDE_DIR}
        ${GLAD_INCLUDE_DIR}
        ${IMGUI_INCLUDE_DIR}
        ${GLM_INCLUDE_DIR}
        ${STB_IMAGE_INCLUDE_DIR}
        ${ENTT_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(HuaEngine
    PUBLIC
        glfw
        GLAD
        ImGui
)

# Platform specific linking
if(WIN32)
    target_link_libraries(HuaEngine PUBLIC opengl32)
    target_compile_definitions(HuaEngine PRIVATE HE_BUILD_DLL)
endif()

# Precompiled headers
target_precompile_headers(HuaEngine PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/enginepch.h>"
)

# Platform specific settings
if(WIN32)
    if(MSVC)
        set_target_properties(HuaEngine PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(HuaEngine PRIVATE /utf-8)
    endif()
endif()

# Set output directory
set_target_properties(HuaEngine PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}/HuaEngine"
)
