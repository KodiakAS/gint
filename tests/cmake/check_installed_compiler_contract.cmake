if(NOT DEFINED GINT_CXX_COMPILER OR NOT DEFINED GINT_SOURCE_DIR OR NOT DEFINED GINT_BINARY_DIR)
    message(FATAL_ERROR "GINT_CXX_COMPILER, GINT_SOURCE_DIR, and GINT_BINARY_DIR are required")
endif()

set(contract_dir "${GINT_BINARY_DIR}/consumer-installed-compiler-contract")
set(install_prefix "${contract_dir}/prefix")
set(consumer_source "${contract_dir}/source")
set(consumer_build "${contract_dir}/build")
set(toolchain "${contract_dir}/clang-cl-toolchain.cmake")
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
file(WRITE "${toolchain}"
    "set(CMAKE_CXX_COMPILER [==[${GINT_CXX_COMPILER}]==])\n"
    "set(CMAKE_CXX_COMPILER_FORCED TRUE)\n"
    "set(CMAKE_CXX_COMPILER_ID_RUN TRUE)\n"
    "set(CMAKE_CXX_COMPILER_ID Clang)\n"
    "set(CMAKE_CXX_SIMULATE_ID MSVC)\n"
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
    message(FATAL_ERROR "installed gint package unexpectedly accepted clang-cl")
endif()

set(output "${stdout}\n${stderr}")
string(FIND "${output}" "gint does not support MSVC or clang-cl" diagnostic_position)
if(diagnostic_position EQUAL -1)
    message(FATAL_ERROR
        "installed package failed without the expected clang-cl diagnostic:\n${output}"
    )
endif()
