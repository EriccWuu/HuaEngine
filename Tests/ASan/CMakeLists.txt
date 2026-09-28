cmake_minimum_required(VERSION 3.24)
project(HuaEngineECSAsan LANGUAGES CXX)

if(NOT MSVC OR NOT WIN32)
    message(FATAL_ERROR "This standalone target validates the Windows MSVC AddressSanitizer configuration")
endif()

get_filename_component(HUAENGINE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ASan is incompatible with MSVC's basic runtime checks and incremental linking.
string(REGEX REPLACE "/RTC[1su]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
string(REPLACE "/ZI" "/Zi" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

add_executable(ECSLifecycleAsan
    "${HUAENGINE_SOURCE_ROOT}/Tests/ECSLifecycleSmoke.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/TypeRegistry.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/OwnedValue.cpp"
)
add_executable(ECSStorageAsan
    "${HUAENGINE_SOURCE_ROOT}/Tests/ECSStorageSmoke.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/TypeRegistry.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/OwnedValue.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/EcsContext.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Resources.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/World.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Commands.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Query.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Timeline.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/WorldScope.cpp"
)
add_executable(ECSQueryAsan
    "${HUAENGINE_SOURCE_ROOT}/Tests/ECSQueryRuntimeSmoke.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/TypeRegistry.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/OwnedValue.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/EcsContext.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Resources.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/World.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Commands.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Query.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Timeline.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/WorldScope.cpp"
)
add_executable(ECSTimelineAsan
    "${HUAENGINE_SOURCE_ROOT}/Tests/ECSTimelineSmoke.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/TypeRegistry.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/OwnedValue.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/EcsContext.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Resources.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/World.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Commands.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Query.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Timeline.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/WorldScope.cpp"
)

add_executable(ECSParallelAsan
    "${HUAENGINE_SOURCE_ROOT}/Tests/ECSParallelSmoke.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/TypeRegistry.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/OwnedValue.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/EcsContext.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Resources.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/World.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Commands.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Query.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/Timeline.cpp"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/WorldScope.cpp"
)
get_target_property(compaction_core_sources ECSStorageAsan SOURCES)
list(FILTER compaction_core_sources EXCLUDE REGEX "/Tests/ECSStorageSmoke.cpp$")
add_executable(ECSCompactionAsan
    "${HUAENGINE_SOURCE_ROOT}/Tests/ECSCompactionSmoke.cpp"
    ${compaction_core_sources})
foreach(core_target ECSStorageAsan ECSQueryAsan ECSTimelineAsan ECSParallelAsan ECSCompactionAsan)
    target_sources(${core_target} PRIVATE
        "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/WorkerPool.cpp")
endforeach()

get_filename_component(msvc_binary_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(asan_architecture x86_64)
else()
    set(asan_architecture i386)
endif()
file(GLOB asan_runtime_dlls "${msvc_binary_directory}/clang_rt.asan*dynamic-${asan_architecture}.dll")
if(NOT asan_runtime_dlls)
    message(FATAL_ERROR "The selected MSVC installation does not contain the AddressSanitizer runtime DLL")
endif()
enable_testing()
foreach(asan_target ECSLifecycleAsan ECSStorageAsan ECSQueryAsan ECSTimelineAsan ECSParallelAsan ECSCompactionAsan)
    target_include_directories(${asan_target} PRIVATE "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src")
    target_compile_definitions(${asan_target} PRIVATE HUAENGINE_ASAN_REQUIRED=1)
    target_compile_options(${asan_target} PRIVATE /fsanitize=address /Zi /utf-8)
    target_link_options(${asan_target} PRIVATE /INCREMENTAL:NO /DEBUG)
    set_target_properties(${asan_target} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_custom_command(TARGET ${asan_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${asan_runtime_dlls} "$<TARGET_FILE_DIR:${asan_target}>"
        VERBATIM)
    add_test(NAME ${asan_target}
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_EXECUTABLE=$<TARGET_FILE:${asan_target}>"
            "-DTEST_WORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/t/$<CONFIG>/${asan_target}"
            -P "${HUAENGINE_SOURCE_ROOT}/cmake/RunSmoke.cmake")
    set_tests_properties(${asan_target} PROPERTIES
        LABELS "ecs;asan"
        ENVIRONMENT "ASAN_OPTIONS=alloc_dealloc_mismatch=1:halt_on_error=1"
        TIMEOUT 120)
endforeach()
