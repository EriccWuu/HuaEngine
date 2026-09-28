if(NOT BUILD_TESTING)
    return()
endif()

set(HUAENGINE_PYTHON_EXECUTABLE "$ENV{HUAENGINE_PYTHON_EXECUTABLE}"
    CACHE FILEPATH "Python interpreter used by reflection tests")
if(HUAENGINE_PYTHON_EXECUTABLE)
    set(Python3_EXECUTABLE "${HUAENGINE_PYTHON_EXECUTABLE}")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(HUAENGINE_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}"
    CACHE FILEPATH "Python interpreter used by reflection tests" FORCE)
set(HUAENGINE_TEST_WORK_ROOT "${CMAKE_BINARY_DIR}/t"
    CACHE PATH "Root for isolated test workspaces; use a short path on Windows")

function(huaengine_add_smoke_test test_name target_name labels)
    string(SHA256 test_identity "${test_name}")
    string(SUBSTRING "${test_identity}" 0 6 test_identity)
    # Leave room for asset GUIDs and content hashes below the Windows path limit.
    set(test_workspace "${HUAENGINE_TEST_WORK_ROOT}/$<IF:$<CONFIG:Debug>,d,$<IF:$<CONFIG:Release>,r,s>>/${test_identity}")
    add_test(NAME ${test_name}
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_EXECUTABLE=$<TARGET_FILE:${target_name}>"
            "-DTEST_WORKSPACE=${test_workspace}"
            "-DTEST_ARGUMENTS=${ARGN}"
            "-DTEST_PYTHON=${HUAENGINE_PYTHON_EXECUTABLE}"
            -P "${CMAKE_SOURCE_DIR}/cmake/RunSmoke.cmake")
    set_tests_properties(${test_name} PROPERTIES
        LABELS "${labels}"
        TIMEOUT 300)
endfunction()

get_property(smoke_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target_name IN LISTS smoke_targets)
    if(NOT target_name MATCHES "Smoke$")
        continue()
    endif()
    if(target_name MATCHES "^ECS")
        set(test_labels ecs)
        if(target_name STREQUAL ECSSceneSerializationSmoke OR target_name STREQUAL ECSContextSmoke OR target_name STREQUAL ECSStorageIntegrationSmoke)
            list(APPEND test_labels integration)
        endif()
        if(target_name STREQUAL ECSContextSmoke)
            list(APPEND test_labels reflection)
        endif()
    elseif(target_name MATCHES "^(Reflection|CLIReflection)")
        set(test_labels reflection)
    else()
        set(test_labels integration)
    endif()
    if(target_name STREQUAL CLIReflectionSmoke)
        huaengine_add_smoke_test(${target_name} ${target_name} "${test_labels}"
            "${CMAKE_BINARY_DIR}/meta/$<CONFIG>/Core/meta-config.json")
        add_dependencies(${target_name} HuaEngineMetaValidation)
    else()
        huaengine_add_smoke_test(${target_name} ${target_name} "${test_labels}")
    endif()
endforeach()

# These tests share the machine's graphics device, even with isolated files.
set_tests_properties(ApplicationOperationsSmoke RHIResourceCreationSmoke RHICommandListBindingSmoke RenderingOperationsSmoke
    PROPERTIES RESOURCE_LOCK huaengine_gpu)

huaengine_add_smoke_test(ECSBenchmarkSmall ECSBenchmark benchmark
    --entities 1000 --scenario dense --samples 3 --seed 20260928)

# Require must remain active when NDEBUG is defined in Release builds.
add_test(NAME ECSBackendRequireGuard
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_EXECUTABLE=$<TARGET_FILE:ECSBackendSmoke>"
        "-DTEST_WORKSPACE=${HUAENGINE_TEST_WORK_ROOT}/$<IF:$<CONFIG:Debug>,d,$<IF:$<CONFIG:Release>,r,s>>/guard"
        "-DTEST_ARGUMENTS=--verify-require"
        "-DTEST_PYTHON=${HUAENGINE_PYTHON_EXECUTABLE}"
        "-DTEST_EXPECTED_EXIT=1"
        "-DTEST_EXPECTED_OUTPUT=Require guard is active"
        -P "${CMAKE_SOURCE_DIR}/cmake/RunSmoke.cmake")
set_tests_properties(ECSBackendRequireGuard PROPERTIES LABELS ecs TIMEOUT 30)
