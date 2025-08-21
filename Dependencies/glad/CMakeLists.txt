# GLAD Library
cmake_minimum_required(VERSION 3.16)

project(GLAD LANGUAGES C)

# Create the GLAD library
add_library(GLAD STATIC
    src/glad.c
    include/glad/glad.h
    include/KHR/khrplatform.h
)

# Include directories
target_include_directories(GLAD
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Platform specific settings
if(WIN32)
    if(MSVC)
        set_target_properties(GLAD PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()
endif()

# Set output directory
set_target_properties(GLAD PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}/GLAD"
)
