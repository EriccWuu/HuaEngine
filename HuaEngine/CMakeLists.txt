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

# Create source groups to maintain folder structure in Visual Studio
foreach(source IN ITEMS ${HUAENGINE_SOURCES})
    get_filename_component(source_path "${source}" PATH)
    file(RELATIVE_PATH source_path_rel "${CMAKE_CURRENT_SOURCE_DIR}/src" "${source_path}")
    string(REPLACE "/" "\\" group_path "${source_path_rel}")
    # Handle empty group path (root files)
    if("${group_path}" STREQUAL "")
        source_group("Source Files" FILES "${source}")
    else()
        source_group("Source Files\\${group_path}" FILES "${source}")
    endif()
endforeach()

# Create source groups for STB Image
foreach(source IN ITEMS ${STB_IMAGE_SOURCES})
    source_group("Dependencies\\STB Image" FILES "${source}")
endforeach()

# Create source groups for ENTT
foreach(source IN ITEMS ${ENTT_SOURCES})
    source_group("Dependencies\\ENTT" FILES "${source}")
endforeach()

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
    target_compile_definitions(HuaEngine PRIVATE HE_BUILD_DLL GLFW_INCLUDE_NONE)
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
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}/HuaEngine"
)
