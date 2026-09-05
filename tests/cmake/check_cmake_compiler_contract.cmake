if(NOT DEFINED GINT_CXX_COMPILER OR NOT DEFINED GINT_SOURCE_DIR OR NOT DEFINED GINT_BINARY_DIR)
    message(FATAL_ERROR "GINT_CXX_COMPILER, GINT_SOURCE_DIR, and GINT_BINARY_DIR are required")
endif()

set(contract_dir "${GINT_BINARY_DIR}/consumer-cmake-compiler-contract")
file(REMOVE_RECURSE "${contract_dir}")
file(MAKE_DIRECTORY "${contract_dir}")

set(contract_source "${GINT_SOURCE_DIR}")
set(contract_configure_args -DGINT_BUILD_TESTS=OFF -DGINT_BUILD_BENCHMARKS=OFF -DGINT_INSTALL=OFF)
include("${CMAKE_CURRENT_LIST_DIR}/compiler_contract_helpers.cmake")

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
