if(NOT DEFINED GINT_CXX_COMPILER OR NOT DEFINED GINT_SOURCE_DIR OR NOT DEFINED GINT_BINARY_DIR)
    message(FATAL_ERROR "GINT_CXX_COMPILER, GINT_SOURCE_DIR, and GINT_BINARY_DIR are required")
endif()

set(contract_dir "${GINT_BINARY_DIR}/consumer-installed-compiler-contract")
set(install_prefix "${contract_dir}/prefix")
set(consumer_source "${contract_dir}/source")
file(REMOVE_RECURSE "${contract_dir}")
file(MAKE_DIRECTORY "${consumer_source}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DCMAKE_INSTALL_PREFIX=${install_prefix}"
        -P "${GINT_BINARY_DIR}/cmake_install.cmake"
    RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "failed to install gint package: ${install_result}")
endif()

if(NOT DEFINED GINT_INSTALL_CMAKE_DIR)
    set(GINT_INSTALL_CMAKE_DIR "lib/cmake/gint")
endif()

file(WRITE "${consumer_source}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.13)\n"
    "project(gint_installed_compiler_contract LANGUAGES CXX)\n"
    "find_package(gint CONFIG REQUIRED)\n"
)
function(expect_installed_configure_failure name compiler_id compiler_version simulate_id expected_diagnostic)
    set(consumer_build "${contract_dir}/${name}-build")
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
            -S "${consumer_source}"
            -B "${consumer_build}"
            "-DCMAKE_TOOLCHAIN_FILE=${toolchain}"
            "-Dgint_DIR=${install_prefix}/${GINT_INSTALL_CMAKE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )
    if(result EQUAL 0)
        message(FATAL_ERROR "installed gint package unexpectedly accepted ${name}")
    endif()

    set(output "${stdout}\n${stderr}")
    string(FIND "${output}" "${expected_diagnostic}" diagnostic_position)
    if(diagnostic_position EQUAL -1)
        message(FATAL_ERROR
            "installed package rejected ${name} without the expected diagnostic '${expected_diagnostic}':\n${output}"
        )
    endif()
endfunction()

expect_installed_configure_failure(
    clang_cl
    Clang
    18.1.0
    MSVC
    "gint does not support MSVC or clang-cl"
)
expect_installed_configure_failure(
    gcc_4_8_4
    GNU
    4.8.4
    ""
    "gint requires GCC 4.8.5 or later"
)
