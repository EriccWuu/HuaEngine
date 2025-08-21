# Editor Application
cmake_minimum_required(VERSION 3.16)

project(Editor LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files
file(GLOB_RECURSE EDITOR_SOURCES
    "src/*.cpp"
    "src/*.h"
)

# Create the Editor executable
add_executable(Editor ${EDITOR_SOURCES})

# Include directories
target_include_directories(Editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
    ${ENTT_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(Editor PRIVATE
    HuaEngine
    ImGui
)

# Platform specific settings
if(WIN32)
    if(MSVC)
        set_target_properties(Editor PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(Editor PRIVATE /utf-8)
    endif()
endif()

# Set output directory
set_target_properties(Editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}/Editor"
)

# Copy assets to output directory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:Editor>/assets"
    )
endif()
