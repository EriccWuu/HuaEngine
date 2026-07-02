# Headless / CLI Host
cmake_minimum_required(VERSION 3.16)

project(HuaEngineHeadless LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HEADLESS_CORE_SOURCES
    src/HeadlessApplication.cpp
    src/HeadlessCommandRunner.cpp
    src/HeadlessJsonWriter.cpp
)

set(HEADLESS_CORE_HEADERS
    src/HeadlessApplication.h
    src/HeadlessCommandRunner.h
    src/HeadlessJsonWriter.h
)

add_library(HuaEngineHeadlessCore STATIC
    ${HEADLESS_CORE_SOURCES}
    ${HEADLESS_CORE_HEADERS}
)

target_include_directories(HuaEngineHeadlessCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)

target_link_libraries(HuaEngineHeadlessCore PUBLIC HuaEngine)

if(WIN32)
    target_compile_definitions(HuaEngineHeadlessCore PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(HuaEngineHeadlessCore PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(HuaEngineHeadlessCore PRIVATE /utf-8)
    endif()
endif()

add_executable(HuaEngineHeadless src/main.cpp)

target_include_directories(HuaEngineHeadless PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)

target_link_libraries(HuaEngineHeadless PRIVATE HuaEngineHeadlessCore)

if(WIN32)
    target_compile_definitions(HuaEngineHeadless PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(HuaEngineHeadless PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(HuaEngineHeadless PRIVATE /utf-8)
    endif()
endif()

set_target_properties(HuaEngineHeadless PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}"
)
