# Sandbox Application
cmake_minimum_required(VERSION 3.16)

project(Sandbox LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files
file(GLOB_RECURSE SANDBOX_SOURCES
    "src/*.cpp"
    "src/*.h"
)

# Create the Sandbox executable
add_executable(Sandbox ${SANDBOX_SOURCES})

# Include directories
target_include_directories(Sandbox PRIVATE
    ${SPDLOG_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
    ${ENTT_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(Sandbox PRIVATE
    HuaEngine
    ImGui
)

# Platform specific settings
if(WIN32)
    if(MSVC)
        set_target_properties(Sandbox PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(Sandbox PRIVATE /utf-8)
    endif()
endif()

# Set output directory
set_target_properties(Sandbox PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}/Sandbox"
)

# Copy assets to output directory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET Sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:Sandbox>/assets"
    )
endif()
