if(NOT EXISTS "${TEST_EXECUTABLE}" OR NOT IS_ABSOLUTE "${TEST_WORKSPACE}")
    message(FATAL_ERROR "A built test executable and an absolute test workspace are required")
endif()

file(MAKE_DIRECTORY "${TEST_WORKSPACE}" "${TEST_WORKSPACE}/a")
set(ENV{TEMP} "${TEST_WORKSPACE}")
set(ENV{TMP} "${TEST_WORKSPACE}")
set(ENV{TMPDIR} "${TEST_WORKSPACE}")
set(ENV{LOCALAPPDATA} "${TEST_WORKSPACE}/a")
set(ENV{APPDATA} "${TEST_WORKSPACE}/a")
set(ENV{PYTHONDONTWRITEBYTECODE} 1)
if(TEST_PYTHON)
    get_filename_component(python_directory "${TEST_PYTHON}" DIRECTORY)
    set(ENV{HUAENGINE_PYTHON_EXECUTABLE} "${TEST_PYTHON}")
    if(WIN32)
        set(ENV{PATH} "${python_directory};$ENV{PATH}")
    else()
        set(ENV{PATH} "${python_directory}:$ENV{PATH}")
    endif()
endif()

execute_process(
    COMMAND "${TEST_EXECUTABLE}" ${TEST_ARGUMENTS}
    WORKING_DIRECTORY "${TEST_WORKSPACE}"
    RESULT_VARIABLE test_exit
    OUTPUT_VARIABLE test_stdout
    ERROR_VARIABLE test_stderr)
file(WRITE "${TEST_WORKSPACE}/stdout.log" "${test_stdout}")
file(WRITE "${TEST_WORKSPACE}/stderr.log" "${test_stderr}")
message("${test_stdout}${test_stderr}")

if(NOT DEFINED TEST_EXPECTED_EXIT)
    set(TEST_EXPECTED_EXIT 0)
endif()
if(NOT "${test_exit}" STREQUAL "${TEST_EXPECTED_EXIT}")
    message(FATAL_ERROR "Test exited with ${test_exit}; expected ${TEST_EXPECTED_EXIT}")
endif()
if(DEFINED TEST_EXPECTED_OUTPUT)
    string(FIND "${test_stdout}${test_stderr}" "${TEST_EXPECTED_OUTPUT}" expected_output_position)
    if(expected_output_position EQUAL -1)
        message(FATAL_ERROR "Test did not print the required diagnostic: ${TEST_EXPECTED_OUTPUT}")
    endif()
endif()
