include_guard(GLOBAL)

function(hua_meta_initialize)
    if(NOT HUAENGINE_SOURCE_ROOT)
        set(HUAENGINE_SOURCE_ROOT "${CMAKE_SOURCE_DIR}" CACHE INTERNAL "Engine repository root")
    endif()
    set(HUAENGINE_META_EXECUTABLE "$ENV{HUAENGINE_META_EXECUTABLE}" CACHE FILEPATH "Locked HuaMeta host tool")
    set(HUAENGINE_META_GENERATOR "${HUAENGINE_SOURCE_ROOT}/Tools/Reflection/reflection_tool.py" CACHE FILEPATH "Reflection backend script")
    set(HUAENGINE_PYTHON_EXECUTABLE "$ENV{HUAENGINE_PYTHON_EXECUTABLE}" CACHE FILEPATH "Locked Python 3.13.15 interpreter")
    set(HUAENGINE_META_RESOURCE_DIR "$ENV{HUAENGINE_META_RESOURCE_DIR}" CACHE PATH "Locked LLVM 23 builtin headers")
    set(HUAENGINE_META_VISUAL_STUDIO "${CMAKE_GENERATOR_INSTANCE}" CACHE PATH "Visual Studio installation for the scan compiler")
    set(HUAENGINE_META_NINJA "${HUAENGINE_META_VISUAL_STUDIO}/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
        CACHE FILEPATH "Ninja executable for isolated scan configurations")
    set(HUAENGINE_META_POWERSHELL "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
        CACHE FILEPATH "System Windows PowerShell 5.1")
    foreach(path_key HUAENGINE_META_EXECUTABLE HUAENGINE_META_GENERATOR HUAENGINE_PYTHON_EXECUTABLE HUAENGINE_META_RESOURCE_DIR
            HUAENGINE_META_NINJA HUAENGINE_META_POWERSHELL HUAENGINE_META_VISUAL_STUDIO)
        file(TO_CMAKE_PATH "${${path_key}}" normalized)
        set(${path_key} "${normalized}" CACHE FILEPATH "Locked meta toolchain path" FORCE)
    endforeach()
    foreach(required HUAENGINE_META_EXECUTABLE HUAENGINE_META_GENERATOR HUAENGINE_PYTHON_EXECUTABLE HUAENGINE_META_RESOURCE_DIR
            HUAENGINE_META_NINJA HUAENGINE_META_POWERSHELL)
        if(NOT ${required} OR NOT EXISTS "${${required}}")
            message(FATAL_ERROR "${required} must name the locked toolchain path. Run BootstrapEcsTools.ps1 and configure with explicit paths.")
        endif()
    endforeach()
    execute_process(COMMAND "${HUAENGINE_PYTHON_EXECUTABLE}" --version
        RESULT_VARIABLE python_status OUTPUT_VARIABLE python_version ERROR_VARIABLE python_error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT python_status EQUAL 0 OR NOT python_version STREQUAL "Python 3.13.15")
        message(FATAL_ERROR "Meta generation requires locked Python 3.13.15: ${python_version} ${python_error}")
    endif()
endfunction()

function(hua_add_meta_umbrella target)
    set(HUA_META_UMBRELLA_INCLUDES "")
    set(HUA_META_REGISTER_COMPONENTS "")
    set(validation_arguments "")
    set(validation_dependencies "")
    foreach(module IN LISTS ARGN)
        get_property(reflection_only GLOBAL PROPERTY HUA_META_${module}_REFLECTION_ONLY)
        if(reflection_only)
            message(FATAL_ERROR "Reflection-only module ${module} cannot register ECS components")
        endif()
        get_property(module_config GLOBAL PROPERTY HUA_META_${module}_CONFIG)
        list(APPEND validation_arguments --meta-config "${module_config}")
        list(APPEND validation_dependencies HuaMeta${module})
        string(APPEND HUA_META_UMBRELLA_INCLUDES "#include <${module}/GeneratedEcs.h>\n")
        string(APPEND HUA_META_REGISTER_COMPONENTS "        if (auto result = ${module}::RegisterComponents(registry); !result) return result.GetError();\n")
    endforeach()
    add_custom_target(${target}MetaValidation
        COMMAND "${HUAENGINE_PYTHON_EXECUTABLE}" "${HUAENGINE_META_GENERATOR}"
            validate-modules ${validation_arguments}
        DEPENDS ${validation_dependencies}
        COMMENT "Validate the ${target} module identities"
        VERBATIM)
    add_dependencies(${target} ${target}MetaValidation)
    set(umbrella "${CMAKE_BINARY_DIR}/generated/$<CONFIG>/HuaEngine/Generated")
    foreach(extension h cpp)
        file(READ "${HUAENGINE_SOURCE_ROOT}/cmake/MetaUmbrella.${extension}.in" template)
        string(CONFIGURE "${template}" configured @ONLY)
        file(GENERATE OUTPUT "${umbrella}/GeneratedEcs.${extension}" CONTENT "${configured}")
    endforeach()
    target_sources(${target} PRIVATE "${umbrella}/GeneratedEcs.cpp")
    target_include_directories(${target} BEFORE PUBLIC "${CMAKE_BINARY_DIR}/generated/$<CONFIG>")
endfunction()

function(hua_add_meta_module)
    cmake_parse_arguments(META "REFLECTION_ONLY" "NAME;TARGET;ENTRY_HEADER" "" ${ARGN})
    if(NOT META_NAME MATCHES "^[A-Za-z_][A-Za-z0-9_]*$" OR NOT TARGET "${META_TARGET}" OR NOT EXISTS "${META_ENTRY_HEADER}")
        message(FATAL_ERROR "hua_add_meta_module requires NAME, TARGET and an existing ENTRY_HEADER")
    endif()
    set(module_root "${CMAKE_BINARY_DIR}/meta/$<CONFIG>/${META_NAME}")
    set(scan_root "${module_root}/scan")
    set(output_root "${CMAKE_BINARY_DIR}/generated/$<CONFIG>/${META_NAME}")
    set(scan_tu "${module_root}/${META_NAME}.cpp")
    set(settings "${module_root}/scan-settings.cmake")
    set(config "${module_root}/meta-config.json")
    set(manifest "${module_root}/manifest.json")
    set(depfile "${module_root}/manifest.d")
    set(generator "${HUAENGINE_META_GENERATOR}")
    if(META_REFLECTION_ONLY)
        set(reflection_only true)
    else()
        set(reflection_only false)
    endif()

    file(GENERATE OUTPUT "${scan_tu}" CONTENT "#include \"${META_ENTRY_HEADER}\"\n")
    file(GENERATE OUTPUT "${settings}" CONTENT
"set(HUA_META_SCAN_MODULES ${META_NAME})
set(HUA_META_SCAN_${META_NAME}_TU [==[${scan_tu}]==])
set(HUA_META_SCAN_${META_NAME}_INCLUDES [==[$<TARGET_GENEX_EVAL:${META_TARGET},$<TARGET_PROPERTY:${META_TARGET},INCLUDE_DIRECTORIES>>]==])
set(HUA_META_SCAN_${META_NAME}_DEFINITIONS [==[$<TARGET_GENEX_EVAL:${META_TARGET},$<TARGET_PROPERTY:${META_TARGET},COMPILE_DEFINITIONS>>]==])
set(HUA_META_SCAN_${META_NAME}_OPTIONS [==[$<TARGET_GENEX_EVAL:${META_TARGET},$<TARGET_PROPERTY:${META_TARGET},COMPILE_OPTIONS>>]==])
set(HUA_META_SCAN_${META_NAME}_RUNTIME [==[$<TARGET_GENEX_EVAL:${META_TARGET},$<TARGET_PROPERTY:${META_TARGET},MSVC_RUNTIME_LIBRARY>>]==])
" TARGET ${META_TARGET})
    file(GENERATE OUTPUT "${config}" CONTENT
"{
  \"config_version\": 1,
  \"module\": \"${META_NAME}\",
  \"configuration\": \"$<CONFIG>\",
  \"reflection_only\": ${reflection_only},
  \"repository_root\": \"${HUAENGINE_SOURCE_ROOT}\",
  \"compile_database\": \"${scan_root}\",
  \"translation_unit\": \"${scan_tu}\",
  \"entry_header\": \"${META_ENTRY_HEADER}\",
  \"hua_meta\": \"${HUAENGINE_META_EXECUTABLE}\",
  \"resource_dir\": \"${HUAENGINE_META_RESOURCE_DIR}\",
  \"python\": \"${HUAENGINE_PYTHON_EXECUTABLE}\",
  \"generator\": \"${generator}\",
  \"manifest\": \"${manifest}\",
  \"output_dir\": \"${output_root}\"
}
")

    add_custom_command(OUTPUT "${scan_root}/compile_commands.json"
        COMMAND "${HUAENGINE_META_POWERSHELL}" -NoLogo -NoProfile -ExecutionPolicy Bypass
            -File "${HUAENGINE_SOURCE_ROOT}/cmake/ConfigureMetaScan.ps1"
            -CMake "${CMAKE_COMMAND}" -SourceRoot "${HUAENGINE_SOURCE_ROOT}"
            -BuildRoot "${scan_root}" -Settings "${settings}" -Configuration "$<CONFIG>"
            -Ninja "${HUAENGINE_META_NINJA}" -Compiler "${CMAKE_CXX_COMPILER}"
            -VisualStudio "${HUAENGINE_META_VISUAL_STUDIO}"
        DEPENDS "${settings}" "${scan_tu}"
            "${HUAENGINE_SOURCE_ROOT}/cmake/MetaScan/CMakeLists.txt"
            "${HUAENGINE_SOURCE_ROOT}/cmake/ConfigureMetaScan.ps1"
            "${HUAENGINE_SOURCE_ROOT}/Tools/Meta/EnterMsvcEnvironment.ps1"
        COMMENT "Configure ${META_NAME} $<CONFIG> compile database without PCH"
        VERBATIM)
    add_custom_command(OUTPUT "${manifest}"
        COMMAND "${CMAKE_COMMAND}" "-DMODE=scan" "-DMETA_CONFIG=${config}" "-DDEPFILE=${depfile}"
            -P "${HUAENGINE_SOURCE_ROOT}/cmake/RunMeta.cmake"
        DEPENDS "${scan_root}/compile_commands.json" "${config}" "${scan_tu}" "${META_ENTRY_HEADER}"
            "${HUAENGINE_META_EXECUTABLE}" "${generator}" "${HUAENGINE_SOURCE_ROOT}/cmake/RunMeta.cmake"
        DEPFILE "${depfile}"
        COMMENT "Scan ${META_NAME} $<CONFIG> with locked Clang"
        VERBATIM)
    set(outputs "${output_root}/GeneratedReflection.h" "${output_root}/GeneratedReflection.cpp")
    if(NOT META_REFLECTION_ONLY)
        list(APPEND outputs "${output_root}/GeneratedEcs.h" "${output_root}/GeneratedEcs.cpp")
    endif()
    add_custom_command(OUTPUT ${outputs}
        BYPRODUCTS "${output_root}/generation-stamp.json"
        COMMAND "${CMAKE_COMMAND}" "-DMODE=generate" "-DMETA_CONFIG=${config}"
            -P "${HUAENGINE_SOURCE_ROOT}/cmake/RunMeta.cmake"
        DEPENDS "${manifest}" "${config}" "${generator}" "${HUAENGINE_SOURCE_ROOT}/cmake/RunMeta.cmake"
        COMMENT "Generate ${META_NAME} $<CONFIG> metadata"
        VERBATIM)
    add_custom_target(HuaMeta${META_NAME} DEPENDS ${outputs})
    add_dependencies(${META_TARGET} HuaMeta${META_NAME})
    target_sources(${META_TARGET} PRIVATE "${output_root}/GeneratedReflection.cpp")
    if(NOT META_REFLECTION_ONLY)
        target_sources(${META_TARGET} PRIVATE "${output_root}/GeneratedEcs.cpp")
    endif()
    target_include_directories(${META_TARGET} BEFORE PUBLIC "${CMAKE_BINARY_DIR}/generated/$<CONFIG>")
    set_property(GLOBAL APPEND PROPERTY HUA_META_MODULES "${META_NAME}")
    set_property(GLOBAL APPEND PROPERTY HUA_META_MANIFESTS "${manifest}")
    set_property(GLOBAL PROPERTY HUA_META_${META_NAME}_CONFIG "${config}")
    set_property(GLOBAL PROPERTY HUA_META_${META_NAME}_REFLECTION_ONLY "${META_REFLECTION_ONLY}")
    set(HUA_META_${META_NAME}_CONFIG "${config}" PARENT_SCOPE)
    set(HUA_META_${META_NAME}_OUTPUT "${output_root}" PARENT_SCOPE)
endfunction()
