# CLI Host
cmake_minimum_required(VERSION 3.16)

project(HuaEngineCLI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CLI_CORE_SOURCES
    src/CLIAssetCommands.cpp
    src/CLIApplication.cpp
    src/CLICommandCatalog.cpp
    src/CLICommandContext.cpp
    src/CLICommandRunner.cpp
    src/CLIJsonWriter.cpp
    src/CLIMetaCommands.cpp
    src/CLIOptionParser.cpp
    src/CLIProjectCommands.cpp
    src/CLIReflectionCommands.cpp
    src/CLISceneCommands.cpp
    src/CLIScriptCommands.cpp
    src/CLIValidationCommands.cpp
)

set(CLI_CORE_HEADERS
    src/CLIAssetCommands.h
    src/CLIApplication.h
    src/CLICommandCatalog.h
    src/CLICommandContext.h
    src/CLICommandRunner.h
    src/CLIJsonWriter.h
    src/CLIMetaCommands.h
    src/CLIOptionParser.h
    src/CLIProjectCommands.h
    src/CLIReflectionCommands.h
    src/CLISceneCommands.h
    src/CLIScriptCommands.h
    src/CLIValidationCommands.h
)

add_library(HuaEngineCLICore STATIC
    ${CLI_CORE_SOURCES}
    ${CLI_CORE_HEADERS}
)

target_include_directories(HuaEngineCLICore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)

target_link_libraries(HuaEngineCLICore PUBLIC HuaEngine)

if(WIN32)
    target_compile_definitions(HuaEngineCLICore PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(HuaEngineCLICore PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(HuaEngineCLICore PRIVATE /utf-8)
    endif()
endif()

add_executable(HuaEngineCLI src/main.cpp)

target_include_directories(HuaEngineCLI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)

target_link_libraries(HuaEngineCLI PRIVATE HuaEngineCLICore)

if(WIN32)
    target_compile_definitions(HuaEngineCLI PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(HuaEngineCLI PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(HuaEngineCLI PRIVATE /utf-8)
    endif()
endif()

set_target_properties(HuaEngineCLI PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}"
)
