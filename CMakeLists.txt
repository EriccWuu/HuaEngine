cmake_minimum_required(VERSION 3.16)

# Enable compile commands export for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include custom configuration
include(${CMAKE_SOURCE_DIR}/cmake/Config.cmake)

# Set the project name and version
project(HuaEngine VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
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
add_subdirectory(Editor)
add_subdirectory(ProjectHub)
add_subdirectory(CLI)

set(SMOKE_RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}/smoke")

function(configure_smoke_target target_name)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${SMOKE_RUNTIME_OUTPUT_DIRECTORY}"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target_name}>"
    )

    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${config} config_upper)
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${config_upper}
                "${CMAKE_BINARY_DIR}/bin/${config}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}/smoke"
        )

        if(MSVC)
            set_target_properties(${target_name} PROPERTIES
                PDB_OUTPUT_DIRECTORY_${config_upper}
                    "${CMAKE_BINARY_DIR}/bin/${config}-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}/smoke"
            )
        endif()
    endforeach()

    if(MSVC)
        set_target_properties(${target_name} PROPERTIES
            PDB_OUTPUT_DIRECTORY "${SMOKE_RUNTIME_OUTPUT_DIRECTORY}"
        )
    endif()
endfunction()

function(copy_cli_host_to_smoke target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:HuaEngineCLI>"
            "$<TARGET_FILE_DIR:${target_name}>/$<TARGET_FILE_NAME:HuaEngineCLI>"
        VERBATIM
    )
endfunction()

add_executable(ECSCoreSmoke Tests/ECSCoreSmoke.cpp)
target_include_directories(ECSCoreSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ECSCoreSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ECSCoreSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ECSCoreSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ECSCoreSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ECSCoreSmoke)

add_executable(ECSQuerySchedulerSmoke Tests/ECSQuerySchedulerSmoke.cpp)
target_include_directories(ECSQuerySchedulerSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ECSQuerySchedulerSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ECSQuerySchedulerSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ECSQuerySchedulerSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ECSQuerySchedulerSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ECSQuerySchedulerSmoke)

add_executable(ECSSceneSerializationSmoke Tests/ECSSceneSerializationSmoke.cpp)
target_include_directories(ECSSceneSerializationSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ECSSceneSerializationSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ECSSceneSerializationSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ECSSceneSerializationSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ECSSceneSerializationSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ECSSceneSerializationSmoke)

add_executable(ECSBackendSmoke Tests/ECSBackendSmoke.cpp)
target_include_directories(ECSBackendSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ECSBackendSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ECSBackendSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ECSBackendSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ECSBackendSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ECSBackendSmoke)

add_executable(ReflectionSmoke Tests/ReflectionSmoke.cpp)
target_include_directories(ReflectionSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ReflectionSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ReflectionSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ReflectionSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ReflectionSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ReflectionSmoke)

add_executable(SerializationSmoke Tests/SerializationSmoke.cpp)
target_include_directories(SerializationSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(SerializationSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(SerializationSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(SerializationSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(SerializationSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(SerializationSmoke)

add_executable(MaterialSerializationSmoke Tests/MaterialSerializationSmoke.cpp)
target_include_directories(MaterialSerializationSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(MaterialSerializationSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(MaterialSerializationSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(MaterialSerializationSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(MaterialSerializationSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(MaterialSerializationSmoke)

add_executable(ProjectServiceSmoke Tests/ProjectServiceSmoke.cpp)
target_include_directories(ProjectServiceSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ProjectServiceSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ProjectServiceSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ProjectServiceSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ProjectServiceSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ProjectServiceSmoke)

add_executable(SceneServiceSmoke Tests/SceneServiceSmoke.cpp)
target_include_directories(SceneServiceSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(SceneServiceSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(SceneServiceSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(SceneServiceSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(SceneServiceSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(SceneServiceSmoke)

add_executable(AssetServiceSmoke Tests/AssetServiceSmoke.cpp)
target_include_directories(AssetServiceSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(AssetServiceSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(AssetServiceSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(AssetServiceSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(AssetServiceSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(AssetServiceSmoke)

add_executable(ScriptServiceSmoke Tests/ScriptServiceSmoke.cpp)
target_include_directories(ScriptServiceSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ScriptServiceSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ScriptServiceSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ScriptServiceSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ScriptServiceSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ScriptServiceSmoke)

add_executable(ValidationServiceSmoke Tests/ValidationServiceSmoke.cpp)
target_include_directories(ValidationServiceSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ValidationServiceSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ValidationServiceSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ValidationServiceSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ValidationServiceSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ValidationServiceSmoke)

add_executable(ApplicationServicesSmoke Tests/ApplicationServicesSmoke.cpp)
target_include_directories(ApplicationServicesSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ApplicationServicesSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ApplicationServicesSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ApplicationServicesSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ApplicationServicesSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ApplicationServicesSmoke)

add_executable(ApplicationOperationsSmoke Tests/ApplicationOperationsSmoke.cpp)
target_include_directories(ApplicationOperationsSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ApplicationOperationsSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ApplicationOperationsSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ApplicationOperationsSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ApplicationOperationsSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ApplicationOperationsSmoke)

add_executable(AgentHostAdapterSmoke Tests/AgentHostAdapterSmoke.cpp)
target_include_directories(AgentHostAdapterSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(AgentHostAdapterSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(AgentHostAdapterSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(AgentHostAdapterSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(AgentHostAdapterSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(AgentHostAdapterSmoke)

add_executable(CLIHostSmoke Tests/CLIHostSmoke.cpp)
target_include_directories(CLIHostSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/CLI/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(CLIHostSmoke PRIVATE HuaEngineCLICore)
if(WIN32)
    target_compile_definitions(CLIHostSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(CLIHostSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(CLIHostSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(CLIHostSmoke)

add_executable(CLIWorkflowSmoke Tests/CLIWorkflowSmoke.cpp)
add_dependencies(CLIWorkflowSmoke HuaEngineCLI)
if(WIN32)
    if(MSVC)
        set_target_properties(CLIWorkflowSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(CLIWorkflowSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(CLIWorkflowSmoke)
copy_cli_host_to_smoke(CLIWorkflowSmoke)

add_executable(CLIContractSmoke Tests/CLIContractSmoke.cpp)
add_dependencies(CLIContractSmoke HuaEngineCLI)
if(WIN32)
    if(MSVC)
        set_target_properties(CLIContractSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(CLIContractSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(CLIContractSmoke)
copy_cli_host_to_smoke(CLIContractSmoke)

add_executable(RenderingOperationsSmoke Tests/RenderingOperationsSmoke.cpp)
target_include_directories(RenderingOperationsSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(RenderingOperationsSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(RenderingOperationsSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(RenderingOperationsSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(RenderingOperationsSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(RenderingOperationsSmoke)

add_executable(HostConsistencySmoke Tests/HostConsistencySmoke.cpp)
add_dependencies(HostConsistencySmoke HuaEngineCLI)
target_include_directories(HostConsistencySmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(HostConsistencySmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(HostConsistencySmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(HostConsistencySmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(HostConsistencySmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(HostConsistencySmoke)
copy_cli_host_to_smoke(HostConsistencySmoke)

add_executable(ProjectWorkbenchSmoke
    Tests/ProjectWorkbenchSmoke.cpp
    Editor/src/Workbench/EditorSessionStorage.cpp
)
target_include_directories(ProjectWorkbenchSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ProjectWorkbenchSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ProjectWorkbenchSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ProjectWorkbenchSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ProjectWorkbenchSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ProjectWorkbenchSmoke)

add_executable(EditorInteractionSmoke
    Tests/EditorInteractionSmoke.cpp
    Editor/src/Selection.cpp
    Editor/src/Interaction/ContextMenuRegistry.cpp
    Editor/src/Interaction/DragDropIntentRegistry.cpp
    Editor/src/Interaction/EditorCommandRouter.cpp
    Editor/src/Interaction/EditorCommandStack.cpp
    Editor/src/Interaction/EditorInteractionHost.cpp
    Editor/src/Interaction/EditorSceneCommands.cpp
    Editor/src/Interaction/ShortcutRegistry.cpp
)
target_include_directories(EditorInteractionSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(EditorInteractionSmoke PRIVATE HuaEngine ImGui)
if(WIN32)
    target_compile_definitions(EditorInteractionSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(EditorInteractionSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(EditorInteractionSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(EditorInteractionSmoke)

# Set ProjectHub as the startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ProjectHub)

# Organize projects in Visual Studio solution folders
set_property(TARGET glfw PROPERTY FOLDER "Dependencies")
set_property(TARGET GLAD PROPERTY FOLDER "Dependencies")
set_property(TARGET ImGui PROPERTY FOLDER "Dependencies")
set_property(TARGET HuaEngineCLICore PROPERTY FOLDER "Hosts")
set_property(TARGET HuaEngineCLI PROPERTY FOLDER "Hosts")
set_property(TARGET ProjectHub PROPERTY FOLDER "Hosts")
set_property(TARGET ECSCoreSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ECSQuerySchedulerSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ECSSceneSerializationSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ECSBackendSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ReflectionSmoke PROPERTY FOLDER "Tests")
set_property(TARGET SerializationSmoke PROPERTY FOLDER "Tests")
set_property(TARGET MaterialSerializationSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ProjectServiceSmoke PROPERTY FOLDER "Tests")
set_property(TARGET SceneServiceSmoke PROPERTY FOLDER "Tests")
set_property(TARGET AssetServiceSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ScriptServiceSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ValidationServiceSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ApplicationServicesSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ApplicationOperationsSmoke PROPERTY FOLDER "Tests")
set_property(TARGET AgentHostAdapterSmoke PROPERTY FOLDER "Tests")
set_property(TARGET CLIHostSmoke PROPERTY FOLDER "Tests")
set_property(TARGET CLIWorkflowSmoke PROPERTY FOLDER "Tests")
set_property(TARGET CLIContractSmoke PROPERTY FOLDER "Tests")
set_property(TARGET RenderingOperationsSmoke PROPERTY FOLDER "Tests")
set_property(TARGET HostConsistencySmoke PROPERTY FOLDER "Tests")
set_property(TARGET ProjectWorkbenchSmoke PROPERTY FOLDER "Tests")
set_property(TARGET EditorInteractionSmoke PROPERTY FOLDER "Tests")
