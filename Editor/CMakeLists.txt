# Editor Application
cmake_minimum_required(VERSION 3.16)

project(Editor LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files
file(GLOB_RECURSE EDITOR_SOURCES
    "src/*.cpp"
    "src/*.h"
)

# Create the Editor executable
add_executable(Editor ${EDITOR_SOURCES})

# Create source groups to maintain folder structure in Visual Studio
foreach(source IN ITEMS ${EDITOR_SOURCES})
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

# Include directories
target_include_directories(Editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(Editor PRIVATE
    HuaEngine
    ImGui
)

# Platform specific settings
if(WIN32)
    target_compile_definitions(Editor PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(Editor PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(Editor PRIVATE /utf-8 /FS)
    endif()
endif()

# Set output directory
set_target_properties(Editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}"
)

# Copy shared engine resources to output directory
if(EXISTS "${CMAKE_SOURCE_DIR}/Resources")
    add_custom_command(TARGET Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:Editor>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources"
        "$<TARGET_FILE_DIR:Editor>/Resources"
    )
endif()
