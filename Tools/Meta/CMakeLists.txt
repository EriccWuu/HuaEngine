cmake_minimum_required(VERSION 3.24)

# Keep the host tool ABI separate from the engine's multi-config runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
project(HuaMeta LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_GENERATOR STREQUAL "Ninja")
    message(FATAL_ERROR "HuaMeta requires an independent Ninja/Release build.")
endif()

find_package(LLVM 23.1.2 EXACT CONFIG REQUIRED)
find_package(Clang 23.1.2 EXACT CONFIG REQUIRED)

add_executable(HuaMeta HuaMeta.cpp MetaScanner.cpp)
target_include_directories(HuaMeta SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
separate_arguments(HUA_LLVM_DEFINITIONS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_options(HuaMeta PRIVATE ${HUA_LLVM_DEFINITIONS})
target_link_libraries(HuaMeta PRIVATE clangTooling)
if(MSVC)
    target_compile_options(HuaMeta PRIVATE /utf-8 /W4)
    if(NOT LLVM_ENABLE_RTTI)
        target_compile_options(HuaMeta PRIVATE /GR-)
    endif()
endif()

get_filename_component(HUA_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(HUA_META_PROBE_ENTRY "${HUA_REPOSITORY_ROOT}/Tests/Meta/Fixtures/ComponentModule.h")
set(HUA_META_PROBE_TU "${HUA_REPOSITORY_ROOT}/Tests/Meta/Fixtures/Scan.cpp")
add_library(HuaMetaProbeInput OBJECT EXCLUDE_FROM_ALL "${HUA_META_PROBE_TU}" fixtures/Invalid.cpp)
set_target_properties(HuaMetaProbeInput PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
target_include_directories(HuaMetaProbeInput PRIVATE
    "${HUA_REPOSITORY_ROOT}/HuaEngine/src"
    "${HUA_REPOSITORY_ROOT}/Dependencies/glm"
    "${HUA_REPOSITORY_ROOT}/Dependencies/spdlog/include"
    "${HUA_REPOSITORY_ROOT}/Dependencies/imgui"
    "${HUA_REPOSITORY_ROOT}/Dependencies/stb_image"
    "${HUA_REPOSITORY_ROOT}/Dependencies/glfw/include"
    "${HUA_REPOSITORY_ROOT}/Dependencies/glad/include"
    "${HUA_REPOSITORY_ROOT}/Dependencies/yaml-cpp/include"
)
target_compile_definitions(HuaMetaProbeInput PRIVATE HE_PLATFORM_WINDOWS HE_BUILD_DLL HE_RELEASE GLFW_INCLUDE_NONE _CRT_SECURE_NO_WARNINGS)
target_compile_options(HuaMetaProbeInput PRIVATE /utf-8)

set(HUA_META_EVIDENCE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/evidence" CACHE PATH "Probe evidence directory")
file(MAKE_DIRECTORY "${HUA_META_EVIDENCE_ROOT}")
enable_testing()
add_test(NAME HuaMetaProbe COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:HuaMeta>"
    "-DBUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
    "-DOUTPUT=${HUA_META_EVIDENCE_ROOT}/probe.json"
    "-DRESOURCE_DIR=${LLVM_INSTALL_PREFIX}/lib/clang/23"
    "-DREPOSITORY_ROOT=${HUA_REPOSITORY_ROOT}"
    "-DINPUT=${HUA_META_PROBE_TU}" "-DENTRY_HEADER=${HUA_META_PROBE_ENTRY}"
    -P "${HUA_REPOSITORY_ROOT}/cmake/VerifyMetaProbe.cmake")
set_tests_properties(HuaMetaProbe PROPERTIES LABELS "meta;toolchain" TIMEOUT 180)
add_test(NAME HuaMetaRejectsInvalidInput COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:HuaMeta>"
    "-DBUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
    "-DOUTPUT=${HUA_META_EVIDENCE_ROOT}/invalid-probe.json"
    "-DRESOURCE_DIR=${LLVM_INSTALL_PREFIX}/lib/clang/23"
    "-DREPOSITORY_ROOT=${HUA_REPOSITORY_ROOT}"
    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/Invalid.cpp"
    "-DENTRY_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/Invalid.cpp" -DEXPECT_FAILURE=ON
    -P "${HUA_REPOSITORY_ROOT}/cmake/VerifyMetaProbe.cmake")
set_tests_properties(HuaMetaRejectsInvalidInput PROPERTIES LABELS "meta;toolchain" TIMEOUT 180)

message(STATUS "HuaMeta LLVM ${LLVM_PACKAGE_VERSION}; RTTI=${LLVM_ENABLE_RTTI}; EH=${LLVM_ENABLE_EH}; CRT=${CMAKE_MSVC_RUNTIME_LIBRARY}")
