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

set(HUAENGINE_RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCHITECTURE}")

add_custom_target(HuaEngineResources ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        "${HUAENGINE_RUNTIME_OUTPUT_DIRECTORY}/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources"
        "${HUAENGINE_RUNTIME_OUTPUT_DIRECTORY}/Resources"
    COMMENT "Copying HuaEngine resources"
    VERBATIM
)

set(HUAENGINE_DXC_SOURCE_DIRECTORY "${CMAKE_SOURCE_DIR}/Dependencies/DXC/bin/win-x64")
add_custom_target(HuaEngineShaderTools ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${HUAENGINE_RUNTIME_OUTPUT_DIRECTORY}/ShaderTools/DXC"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${HUAENGINE_DXC_SOURCE_DIRECTORY}/dxc.exe"
        "${HUAENGINE_DXC_SOURCE_DIRECTORY}/dxcompiler.dll"
        "${HUAENGINE_DXC_SOURCE_DIRECTORY}/dxil.dll"
        "${CMAKE_SOURCE_DIR}/Dependencies/DXC/VERSION"
        "${HUAENGINE_RUNTIME_OUTPUT_DIRECTORY}/ShaderTools/DXC"
    COMMENT "Copying fixed DXC shader tools"
    VERBATIM
)

# Include directories
set(SPDLOG_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/spdlog/include")
set(GLFW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/glfw/include")
set(GLAD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/glad/include")
set(IMGUI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/imgui")
set(GLM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/glm")
set(STB_IMAGE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/stb_image")
set(ENTT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/entt")
set(TINYOBJLOADER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/tinyobjloader")

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

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM_PREVIOUS ${CMAKE_POLICY_VERSION_MINIMUM})
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(Dependencies/yaml-cpp)
set(CMAKE_POLICY_VERSION_MINIMUM ${CMAKE_POLICY_VERSION_MINIMUM_PREVIOUS})

set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
add_subdirectory(Dependencies/SPIRV-Cross)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(Dependencies/glslang)

# Add subdirectories for main projects
add_subdirectory(HuaEngine)
add_subdirectory(Editor)
add_subdirectory(ProjectHub)
add_subdirectory(CLI)

add_dependencies(HuaEngine HuaEngineResources)
add_dependencies(HuaEngine HuaEngineShaderTools)
add_dependencies(Editor HuaEngineResources)
add_dependencies(ProjectHub HuaEngineResources)
add_dependencies(HuaEngineCLI HuaEngineResources)

# Editor launches ProjectHub as a sibling host from its output directory.
add_dependencies(Editor ProjectHub)

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

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${target_name}>/ShaderTools/DXC"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${HUAENGINE_DXC_SOURCE_DIRECTORY}/dxc.exe"
            "${HUAENGINE_DXC_SOURCE_DIRECTORY}/dxcompiler.dll"
            "${HUAENGINE_DXC_SOURCE_DIRECTORY}/dxil.dll"
            "${CMAKE_SOURCE_DIR}/Dependencies/DXC/VERSION"
            "$<TARGET_FILE_DIR:${target_name}>/ShaderTools/DXC"
        VERBATIM
    )
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

add_executable(ReflectionGeneratedSmoke Tests/ReflectionGeneratedSmoke.cpp)
target_include_directories(ReflectionGeneratedSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ReflectionGeneratedSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ReflectionGeneratedSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ReflectionGeneratedSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ReflectionGeneratedSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ReflectionGeneratedSmoke)

add_executable(ReflectionRuntimeProviderSmoke Tests/ReflectionRuntimeProviderSmoke.cpp)
target_include_directories(ReflectionRuntimeProviderSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ReflectionRuntimeProviderSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ReflectionRuntimeProviderSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ReflectionRuntimeProviderSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ReflectionRuntimeProviderSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ReflectionRuntimeProviderSmoke)

add_executable(ReflectionToolSmoke Tests/ReflectionToolSmoke.cpp)
if(WIN32)
    if(MSVC)
        set_target_properties(ReflectionToolSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ReflectionToolSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ReflectionToolSmoke)

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

add_executable(SerializationPolicySmoke Tests/SerializationPolicySmoke.cpp)
target_include_directories(SerializationPolicySmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(SerializationPolicySmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(SerializationPolicySmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(SerializationPolicySmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(SerializationPolicySmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(SerializationPolicySmoke)

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

add_executable(AssetMetaSmoke Tests/AssetMetaSmoke.cpp)
target_include_directories(AssetMetaSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(AssetMetaSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(AssetMetaSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(AssetMetaSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(AssetMetaSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(AssetMetaSmoke)

add_executable(AssetLibrarySmoke Tests/AssetLibrarySmoke.cpp)
target_include_directories(AssetLibrarySmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(AssetLibrarySmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(AssetLibrarySmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(AssetLibrarySmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(AssetLibrarySmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(AssetLibrarySmoke)

add_executable(ShaderInterfaceSmoke Tests/ShaderInterfaceSmoke.cpp)
target_include_directories(ShaderInterfaceSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ShaderInterfaceSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ShaderInterfaceSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ShaderInterfaceSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ShaderInterfaceSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ShaderInterfaceSmoke)

add_executable(AssetImportSmoke Tests/AssetImportSmoke.cpp)
target_include_directories(AssetImportSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(AssetImportSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(AssetImportSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(AssetImportSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(AssetImportSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(AssetImportSmoke)

add_executable(BuiltinAssetResourcesSmoke Tests/BuiltinAssetResourcesSmoke.cpp)
target_include_directories(BuiltinAssetResourcesSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(BuiltinAssetResourcesSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(BuiltinAssetResourcesSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(BuiltinAssetResourcesSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(BuiltinAssetResourcesSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(BuiltinAssetResourcesSmoke)
add_dependencies(BuiltinAssetResourcesSmoke HuaEngineResources)

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

add_executable(CLIReflectionSmoke Tests/CLIReflectionSmoke.cpp)
add_dependencies(CLIReflectionSmoke HuaEngineCLI)
if(WIN32)
    if(MSVC)
        set_target_properties(CLIReflectionSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(CLIReflectionSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(CLIReflectionSmoke)
copy_cli_host_to_smoke(CLIReflectionSmoke)

add_executable(RenderGraphSmoke Tests/RenderGraphSmoke.cpp)
target_include_directories(RenderGraphSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
)
target_link_libraries(RenderGraphSmoke PRIVATE HuaEngine)
if(WIN32)
    if(MSVC)
        set_target_properties(RenderGraphSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(RenderGraphSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(RenderGraphSmoke)

add_executable(InputSystemSmoke Tests/InputSystemSmoke.cpp)
target_include_directories(InputSystemSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
)
target_link_libraries(InputSystemSmoke PRIVATE HuaEngine)
configure_smoke_target(InputSystemSmoke)

add_executable(EditorInputSmoke
    Tests/EditorInputSmoke.cpp
    Editor/src/Input/EditorCommandRegistry.cpp
    Editor/src/Input/EditorInputBindingRegistry.cpp
    Editor/src/Input/EditorInputContextService.cpp
    Editor/src/Input/EditorInputResolver.cpp
    Editor/src/Input/EditorInputService.cpp
    Editor/src/Input/EditorInputBindingStorage.cpp
)
target_include_directories(EditorInputSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
)
target_link_libraries(EditorInputSmoke PRIVATE HuaEngine yaml-cpp::yaml-cpp)
configure_smoke_target(EditorInputSmoke)

add_executable(RHIResourceCreationSmoke Tests/RHIResourceCreationSmoke.cpp)
target_include_directories(RHIResourceCreationSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(RHIResourceCreationSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(RHIResourceCreationSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(RHIResourceCreationSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(RHIResourceCreationSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(RHIResourceCreationSmoke)

add_executable(RHICommandListBindingSmoke Tests/RHICommandListBindingSmoke.cpp)
target_include_directories(RHICommandListBindingSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(RHICommandListBindingSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(RHICommandListBindingSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(RHICommandListBindingSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(RHICommandListBindingSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(RHICommandListBindingSmoke)

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
	Editor/src/Assets/AssetPickerCatalog.cpp
	Editor/src/Assets/AssetPickerModel.cpp
    Editor/src/Selection.cpp
    Editor/src/Selection/EditorSelectionService.cpp
    Editor/src/Interaction/ContextMenuRegistry.cpp
    Editor/src/Interaction/DragDropIntentRegistry.cpp
    Editor/src/Interaction/EditorCommandRouter.cpp
    Editor/src/Interaction/EditorCommandStack.cpp
    Editor/src/Interaction/EditorInteractionHost.cpp
    Editor/src/Interaction/EditorSceneCommands.cpp
    Editor/src/Interaction/ShortcutRegistry.cpp
	Editor/src/Panels/RuntimeInspector.cpp
	Editor/src/Scene/SceneEntityInspectorEditor.cpp
    Editor/src/Viewport/EditorCameraController.cpp
    Editor/src/Viewport/EditorGrid.cpp
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

add_executable(EditorInspectorRuntimeSmoke
    Tests/EditorInspectorRuntimeSmoke.cpp
    Editor/src/Assets/AssetPickerModel.cpp
    Editor/src/Panels/RuntimeInspector.cpp
)
target_include_directories(EditorInspectorRuntimeSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(EditorInspectorRuntimeSmoke PRIVATE HuaEngine ImGui)
if(WIN32)
    target_compile_definitions(EditorInspectorRuntimeSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(EditorInspectorRuntimeSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(EditorInspectorRuntimeSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(EditorInspectorRuntimeSmoke)

add_executable(EditorAssetPickerSmoke
    Tests/EditorAssetPickerSmoke.cpp
    Editor/src/Assets/AssetPickerModel.cpp
    Editor/src/Assets/AssetPickerCatalog.cpp
)
target_include_directories(EditorAssetPickerSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(EditorAssetPickerSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(EditorAssetPickerSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(EditorAssetPickerSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(EditorAssetPickerSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(EditorAssetPickerSmoke)

add_executable(AssetEditingSmoke
    Tests/AssetEditingSmoke.cpp
	Editor/src/Selection.cpp
	Editor/src/Selection/EditorSelectionService.cpp
    Editor/src/Assets/AssetEditSession.cpp
	Editor/src/Assets/AssetInspectorEditor.cpp
    Editor/src/Assets/AssetPreviewContext.cpp
    Editor/src/Assets/AssetEditorRegistry.cpp
    Editor/src/Assets/AssetInspectorHost.cpp
	Editor/src/Assets/AssetPickerCatalog.cpp
	Editor/src/Assets/AssetPickerModel.cpp
    Editor/src/Assets/Editors/GenericAssetInspector.cpp
    Editor/src/Assets/Editors/MaterialAssetEditor.cpp
    Editor/src/Assets/Editors/ImportSettingsEditors.cpp
    Editor/src/Assets/Editors/ShaderAssetEditor.cpp
    Editor/src/Assets/Editors/SceneAssetEditor.cpp
)
target_include_directories(AssetEditingSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
	${IMGUI_INCLUDE_DIR}
)
target_link_libraries(AssetEditingSmoke PRIVATE HuaEngine ImGui)
if(WIN32 AND MSVC)
    set_target_properties(AssetEditingSmoke PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_options(AssetEditingSmoke PRIVATE /utf-8)
endif()
configure_smoke_target(AssetEditingSmoke)

add_executable(ProjectPanelActionSmoke
    Tests/ProjectPanelActionSmoke.cpp
    Editor/src/Panels/ProjectPanel.cpp
)
target_include_directories(ProjectPanelActionSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ProjectPanelActionSmoke PRIVATE HuaEngine ImGui)
if(WIN32)
    target_compile_definitions(ProjectPanelActionSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ProjectPanelActionSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ProjectPanelActionSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ProjectPanelActionSmoke)

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
set_property(TARGET ReflectionGeneratedSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ReflectionRuntimeProviderSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ReflectionToolSmoke PROPERTY FOLDER "Tests")
set_property(TARGET SerializationSmoke PROPERTY FOLDER "Tests")
set_property(TARGET SerializationPolicySmoke PROPERTY FOLDER "Tests")
set_property(TARGET MaterialSerializationSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ProjectServiceSmoke PROPERTY FOLDER "Tests")
set_property(TARGET SceneServiceSmoke PROPERTY FOLDER "Tests")
set_property(TARGET AssetServiceSmoke PROPERTY FOLDER "Tests")
set_property(TARGET AssetMetaSmoke PROPERTY FOLDER "Tests")
set_property(TARGET AssetLibrarySmoke PROPERTY FOLDER "Tests")
set_property(TARGET AssetImportSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ValidationServiceSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ApplicationServicesSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ApplicationOperationsSmoke PROPERTY FOLDER "Tests")
set_property(TARGET AgentHostAdapterSmoke PROPERTY FOLDER "Tests")
set_property(TARGET CLIHostSmoke PROPERTY FOLDER "Tests")
set_property(TARGET CLIWorkflowSmoke PROPERTY FOLDER "Tests")
set_property(TARGET CLIContractSmoke PROPERTY FOLDER "Tests")
set_property(TARGET CLIReflectionSmoke PROPERTY FOLDER "Tests")
set_property(TARGET RenderGraphSmoke PROPERTY FOLDER "Tests")
set_property(TARGET RHIResourceCreationSmoke PROPERTY FOLDER "Tests")
set_property(TARGET RHICommandListBindingSmoke PROPERTY FOLDER "Tests")
set_property(TARGET RenderingOperationsSmoke PROPERTY FOLDER "Tests")
set_property(TARGET HostConsistencySmoke PROPERTY FOLDER "Tests")
set_property(TARGET ProjectWorkbenchSmoke PROPERTY FOLDER "Tests")
set_property(TARGET EditorInteractionSmoke PROPERTY FOLDER "Tests")
set_property(TARGET EditorInspectorRuntimeSmoke PROPERTY FOLDER "Tests")
set_property(TARGET EditorAssetPickerSmoke PROPERTY FOLDER "Tests")
set_property(TARGET AssetEditingSmoke PROPERTY FOLDER "Tests")
set_property(TARGET ProjectPanelActionSmoke PROPERTY FOLDER "Tests")
