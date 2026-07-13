if(NOT DEFINED GINT_CXX_COMPILER OR NOT DEFINED GINT_SOURCE_DIR OR NOT DEFINED GINT_BINARY_DIR)
    message(FATAL_ERROR "GINT_CXX_COMPILER, GINT_SOURCE_DIR, and GINT_BINARY_DIR are required")
endif()

set(contract_dir "${GINT_BINARY_DIR}/consumer-cmake-compiler-contract")
file(REMOVE_RECURSE "${contract_dir}")
file(MAKE_DIRECTORY "${contract_dir}")

function(expect_configure_failure name compiler_id compiler_version simulate_id expected_diagnostic)
    set(toolchain "${contract_dir}/${name}-toolchain.cmake")
    file(WRITE "${toolchain}"
        "set(CMAKE_CXX_COMPILER [==[${GINT_CXX_COMPILER}]==])\n"
        "set(CMAKE_CXX_COMPILER_FORCED TRUE)\n"
        "set(CMAKE_CXX_COMPILER_ID_RUN TRUE)\n"
        "set(CMAKE_CXX_COMPILER_ID [==[${compiler_id}]==])\n"
        "set(CMAKE_CXX_COMPILER_VERSION [==[${compiler_version}]==])\n"
        "set(CMAKE_CXX_SIMULATE_ID [==[${simulate_id}]==])\n"
    )

    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -S "${GINT_SOURCE_DIR}"
            -B "${contract_dir}/${name}-build"
            "-DCMAKE_TOOLCHAIN_FILE=${toolchain}"
            -DGINT_BUILD_TESTS=OFF
            -DGINT_BUILD_BENCHMARKS=OFF
            -DGINT_INSTALL=OFF
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )
    if(result EQUAL 0)
        message(FATAL_ERROR "${name} unexpectedly configured successfully")
    endif()

    set(output "${stdout}\n${stderr}")
    string(FIND "${output}" "${expected_diagnostic}" diagnostic_position)
    if(diagnostic_position EQUAL -1)
        message(FATAL_ERROR
            "${name} failed without the expected diagnostic '${expected_diagnostic}':\n${output}"
        )
    endif()
endfunction()

expect_configure_failure(
    clang_cl
    Clang
    18.1.0
    MSVC
    "gint does not support MSVC or clang-cl"
)
expect_configure_failure(
    unsupported_frontend
    IntelLLVM
    2025.0.0
    ""
    "gint supports GCC, Clang, and AppleClang only"
)
expect_configure_failure(
    gcc_4_8_4
    GNU
    4.8.4
    ""
    "gint requires GCC 4.8.5 or later"
)
