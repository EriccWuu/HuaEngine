# ProjectHub Application
cmake_minimum_required(VERSION 3.16)

project(ProjectHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE PROJECTHUB_SOURCES
    "src/*.cpp"
    "src/*.h"
)

add_executable(ProjectHub
    ${PROJECTHUB_SOURCES}
    ${CMAKE_SOURCE_DIR}/Editor/src/Workbench/EditorSessionStorage.cpp
)

foreach(source IN ITEMS ${PROJECTHUB_SOURCES})
    get_filename_component(source_path "${source}" PATH)
    file(RELATIVE_PATH source_path_rel "${CMAKE_CURRENT_SOURCE_DIR}/src" "${source_path}")
    string(REPLACE "/" "\\" group_path "${source_path_rel}")
    if("${group_path}" STREQUAL "")
        source_group("Source Files" FILES "${source}")
    else()
        source_group("Source Files\\${group_path}" FILES "${source}")
    endif()
endforeach()

source_group("Source Files\\Shared" FILES
    ${CMAKE_SOURCE_DIR}/Editor/src/Workbench/EditorSessionStorage.cpp
)

target_include_directories(ProjectHub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
    ${ENTT_INCLUDE_DIR}
)

target_link_libraries(ProjectHub PRIVATE
    HuaEngine
    ImGui
)

if(WIN32)
    target_compile_definitions(ProjectHub PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ProjectHub PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ProjectHub PRIVATE /utf-8)
    endif()
endif()

set_target_properties(ProjectHub PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/Resources")
    add_custom_command(TARGET ProjectHub POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:ProjectHub>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources"
        "$<TARGET_FILE_DIR:ProjectHub>/Resources"
    )
endif()
