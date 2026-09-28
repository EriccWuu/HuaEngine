cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(HuaGeneratedModuleProbe LANGUAGES CXX)
    set(HUA_META_STANDALONE ON)
    get_filename_component(HUAENGINE_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    set(HUAENGINE_SOURCE_ROOT "${HUAENGINE_SOURCE_ROOT}" CACHE INTERNAL "Engine repository root")
    include("${HUAENGINE_SOURCE_ROOT}/cmake/HuaMeta.cmake")
    hua_meta_initialize()
    enable_testing()
endif()

add_executable(GeneratedQuerySmoke
    "${CMAKE_CURRENT_SOURCE_DIR}/GeneratedQuerySmoke.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/GeneratedQuerySecondTU.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/GeneratedContracts.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/ComponentModule.cpp")
target_compile_features(GeneratedQuerySmoke PRIVATE cxx_std_20)
set_target_properties(GeneratedQuerySmoke PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
target_include_directories(GeneratedQuerySmoke PRIVATE
    "${HUAENGINE_SOURCE_ROOT}"
    "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src"
    "${HUAENGINE_SOURCE_ROOT}/Dependencies/glm"
    "${HUAENGINE_SOURCE_ROOT}/Dependencies/spdlog/include"
    "${HUAENGINE_SOURCE_ROOT}/Dependencies/imgui"
    "${HUAENGINE_SOURCE_ROOT}/Dependencies/stb_image"
    "${HUAENGINE_SOURCE_ROOT}/Dependencies/glfw/include"
    "${HUAENGINE_SOURCE_ROOT}/Dependencies/glad/include"
    "${HUAENGINE_SOURCE_ROOT}/Dependencies/yaml-cpp/include")
target_compile_definitions(GeneratedQuerySmoke PRIVATE
    HE_PLATFORM_WINDOWS _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:HE_DEBUG>
    $<$<CONFIG:Release>:HE_RELEASE>)
if(MSVC)
    target_compile_options(GeneratedQuerySmoke PRIVATE /utf-8)
    set_property(TARGET GeneratedQuerySmoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(HUA_META_STANDALONE)
    foreach(source TypeRegistry OwnedValue EcsContext Resources World Commands Query Timeline WorldScope WorkerPool)
        target_sources(GeneratedQuerySmoke PRIVATE "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/ECS/Runtime/${source}.cpp")
    endforeach()
    target_sources(GeneratedQuerySmoke PRIVATE "${HUAENGINE_SOURCE_ROOT}/HuaEngine/src/HuaEngine/Reflection/RuntimeReflection.cpp")
    option(HUA_META_ASAN "Instrument the standalone generated module" OFF)
    if(HUA_META_ASAN)
        string(REGEX REPLACE "/RTC[1su]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        string(REPLACE "/ZI" "/Zi" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        target_compile_options(GeneratedQuerySmoke PRIVATE /fsanitize=address /Zi)
        target_compile_definitions(GeneratedQuerySmoke PRIVATE HUAENGINE_ASAN_REQUIRED=1)
        target_link_options(GeneratedQuerySmoke PRIVATE /INCREMENTAL:NO /DEBUG)
        set_property(TARGET GeneratedQuerySmoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        get_filename_component(compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        file(GLOB asan_dlls "${compiler_bin}/clang_rt.asan*dynamic-x86_64.dll")
        if(NOT asan_dlls)
            message(FATAL_ERROR "The selected MSVC installation needs the ASan runtime")
        endif()
        add_custom_command(TARGET GeneratedQuerySmoke POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
            copy_if_different ${asan_dlls} "$<TARGET_FILE_DIR:GeneratedQuerySmoke>" VERBATIM)
    endif()
else()
    target_link_libraries(GeneratedQuerySmoke PRIVATE HuaEngine)
    target_compile_definitions(GeneratedQuerySmoke PRIVATE HUA_META_ENGINE_INTEGRATION=1)
    configure_smoke_target(GeneratedQuerySmoke)
endif()

hua_add_meta_module(NAME Test TARGET GeneratedQuerySmoke
    ENTRY_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/ComponentModule.h")
if(HUA_META_STANDALONE)
    hua_add_meta_umbrella(GeneratedQuerySmoke Test)
endif()
add_test(NAME GeneratedQuerySmoke COMMAND "$<TARGET_FILE:GeneratedQuerySmoke>")
set_tests_properties(GeneratedQuerySmoke PROPERTIES LABELS "ecs;reflection;meta;integration" TIMEOUT 60)
if(HUA_META_ASAN)
    set_tests_properties(GeneratedQuerySmoke PROPERTIES LABELS "ecs;reflection;meta;asan"
        ENVIRONMENT "ASAN_OPTIONS=alloc_dealloc_mismatch=1:halt_on_error=1")
endif()
add_test(NAME MetaFrontendSmoke COMMAND "${HUAENGINE_PYTHON_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/MetaFrontendSmoke.py"
    --meta-config "${HUA_META_Test_CONFIG}"
    --work-dir "${HUAENGINE_TEST_WORK_ROOT}/meta/$<CONFIG>/frontend")
set_tests_properties(MetaFrontendSmoke PROPERTIES LABELS "reflection;meta;integration" TIMEOUT 240)
add_test(NAME MetaBuildSmoke COMMAND "${HUAENGINE_PYTHON_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/MetaBuildSmoke.py"
    --meta-config "${HUA_META_Test_CONFIG}" --cmake "${CMAKE_COMMAND}"
    --visual-studio "${HUAENGINE_META_VISUAL_STUDIO}"
    --work-dir "${HUAENGINE_TEST_WORK_ROOT}/meta/$<CONFIG>/build")
set_tests_properties(MetaBuildSmoke PROPERTIES LABELS "reflection;meta;integration" TIMEOUT 900 RESOURCE_LOCK huaengine_meta_build)
