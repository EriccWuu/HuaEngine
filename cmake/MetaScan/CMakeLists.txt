cmake_minimum_required(VERSION 3.24)
project(HuaEngineMetaScan LANGUAGES CXX)

if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
    message(FATAL_ERROR "Meta scans require one Ninja compile database per configuration")
endif()
if(NOT HUA_META_SCAN_SETTINGS OR NOT EXISTS "${HUA_META_SCAN_SETTINGS}")
    message(FATAL_ERROR "HUA_META_SCAN_SETTINGS must name the configured target settings file")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include("${HUA_META_SCAN_SETTINGS}")

foreach(module IN LISTS HUA_META_SCAN_MODULES)
    add_library(HuaMetaScan${module} OBJECT EXCLUDE_FROM_ALL "${HUA_META_SCAN_${module}_TU}")
    set_target_properties(HuaMetaScan${module} PROPERTIES
        DISABLE_PRECOMPILE_HEADERS ON
        MSVC_RUNTIME_LIBRARY "${HUA_META_SCAN_${module}_RUNTIME}")
    target_include_directories(HuaMetaScan${module} PRIVATE ${HUA_META_SCAN_${module}_INCLUDES})
    target_compile_definitions(HuaMetaScan${module} PRIVATE ${HUA_META_SCAN_${module}_DEFINITIONS})
    target_compile_options(HuaMetaScan${module} PRIVATE ${HUA_META_SCAN_${module}_OPTIONS})
endforeach()
