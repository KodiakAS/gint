if(NOT DEFINED GINT_CXX_COMPILER OR NOT DEFINED GINT_SOURCE_DIR OR NOT DEFINED GINT_BINARY_DIR)
    message(FATAL_ERROR "GINT_CXX_COMPILER, GINT_SOURCE_DIR, and GINT_BINARY_DIR are required")
endif()

set(source "${GINT_SOURCE_DIR}/tests/consumer/header_contract.cpp")
set(output_dir "${GINT_BINARY_DIR}/consumer-header-contract")
file(MAKE_DIRECTORY "${output_dir}")

set(single_header_include "${output_dir}/single-header/include")
file(REMOVE_RECURSE "${single_header_include}")
file(MAKE_DIRECTORY "${single_header_include}/gint")
file(COPY "${GINT_SOURCE_DIR}/include/gint/gint.h" DESTINATION "${single_header_include}/gint")
execute_process(
    COMMAND "${GINT_CXX_COMPILER}"
        -std=c++11
        -Wall
        -Wextra
        -Werror
        "-I${single_header_include}"
        -c "${source}"
        -o "${output_dir}/single-header.o"
    RESULT_VARIABLE single_header_result
    OUTPUT_VARIABLE single_header_stdout
    ERROR_VARIABLE single_header_stderr
)
if(NOT single_header_result EQUAL 0)
    message(FATAL_ERROR
        "isolated single-header copy failed to compile:"
        "\n${single_header_stdout}\n${single_header_stderr}"
    )
endif()

function(expect_compile_failure name standard expected_diagnostic)
    execute_process(
        COMMAND "${GINT_CXX_COMPILER}"
            "-std=${standard}"
            ${ARGN}
            "-I${GINT_SOURCE_DIR}/include"
            -c "${source}"
            -o "${output_dir}/${name}.o"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )
    if(result EQUAL 0)
        message(FATAL_ERROR "${name} unexpectedly compiled successfully")
    endif()
    set(output "${stdout}\n${stderr}")
    string(FIND "${output}" "${expected_diagnostic}" diagnostic_position)
    if(diagnostic_position EQUAL -1)
        message(FATAL_ERROR
            "${name} failed without the expected diagnostic '${expected_diagnostic}':\n${output}"
        )
    endif()
endfunction()

expect_compile_failure(cxx98 c++98 "gint requires C++11 or later")
expect_compile_failure(msvc_abi c++11 "gint does not support MSVC or clang-cl" -D_MSC_VER=1930)
expect_compile_failure(no_int128 c++11 "gint requires compiler support for __int128" -U__SIZEOF_INT128__)
expect_compile_failure(
    gcc_4_8_4
    c++11
    "gint requires GCC 4.8.5 or later"
    -U__clang__
    -U__GNUC__
    -U__GNUC_MINOR__
    -U__GNUC_PATCHLEVEL__
    -D__GNUC__=4
    -D__GNUC_MINOR__=8
    -D__GNUC_PATCHLEVEL__=4
)
