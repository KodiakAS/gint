cmake_minimum_required(VERSION 3.10)
project(gint LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(GINT_BUILD_TESTS "Build test suite" ON)
option(GINT_BUILD_BENCHMARKS "Build benchmarks" OFF)

set(GINT_TEST_OPTIONS -O0 -g)
set(GINT_TEST_LINK_OPTIONS "")

if(ENABLE_COVERAGE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND GINT_TEST_OPTIONS --coverage)
        set(GINT_TEST_LINK_OPTIONS --coverage)
    endif()
endif()

if(GINT_BUILD_TESTS OR GINT_BUILD_BENCHMARKS)
    find_package(fmt CONFIG QUIET)
    if(NOT fmt_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(fmt REQUIRED IMPORTED_TARGET fmt)
        add_library(fmt::fmt ALIAS PkgConfig::fmt)
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(GINT_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    include(GoogleTest)
    include(CheckCXXCompilerFlag)
    function(add_gint_test name source std)
        add_executable(${name} ${source})
        target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(${name} PROPERTIES CXX_STANDARD ${std} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
        target_compile_options(${name} PRIVATE ${GINT_TEST_OPTIONS})
        if(GINT_TEST_LINK_OPTIONS)
            target_link_options(${name} PRIVATE ${GINT_TEST_LINK_OPTIONS})
        endif()
        target_link_libraries(${name} PRIVATE GTest::gtest_main)
        gtest_discover_tests(${name} NO_PRETTY_VALUES)
    endfunction()

    # Basic tests
    add_gint_test(gint_test_construction tests/construction_test.cpp 11)
    add_gint_test(gint_test_conversion tests/conversion_test.cpp 11)
    add_gint_test(gint_test_comparison tests/comparison_test.cpp 11)
    add_gint_test(gint_test_boundary tests/boundary_test.cpp 11)
    add_gint_test(gint_test_numeric_limits tests/numeric_limits_test.cpp 11)
    add_gint_test(gint_test_stream tests/stream_test.cpp 11)
    add_gint_test(gint_test_exceptions_overflow tests/exceptions_overflow_test.cpp 11)

    add_gint_test(gint_arith_basic_test tests/arithmetic_basic_test.cpp 11)
    target_compile_definitions(gint_arith_basic_test PRIVATE GINT_ENABLE_FMT)
    target_link_libraries(gint_arith_basic_test PRIVATE fmt::fmt)

    add_gint_test(gint_bitwise_test tests/bitwise_test.cpp 11)
    target_compile_definitions(gint_bitwise_test PRIVATE GINT_ENABLE_FMT)
    target_link_libraries(gint_bitwise_test PRIVATE fmt::fmt)

    add_gint_test(gint_shift_test tests/shift_test.cpp 11)
    add_gint_test(gint_arith_divmod_test tests/arithmetic_divmod_test.cpp 11)

    # Fmt support tests
    add_gint_test(gint_fmt_test tests/fmt_support_test.cpp 11)
    target_compile_definitions(gint_fmt_test PRIVATE GINT_ENABLE_FMT)
    target_link_libraries(gint_fmt_test PRIVATE fmt::fmt)

    # Compile tests for various C++ standards
    check_cxx_compiler_flag("-std=c++17" HAS_CXX17)
    check_cxx_compiler_flag("-std=c++20" HAS_CXX20)

    if(HAS_CXX17)
        add_gint_test(gint_compile_cpp17 tests/compile_test.cpp 17)
    endif()
    if(HAS_CXX20)
        add_gint_test(gint_compile_cpp20 tests/compile_test.cpp 20)
    endif()

    # Unified test runner to improve coverage aggregation (single binary)
    # Collect all test sources
    file(GLOB GINT_ALL_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
    # Exclude tests that require conflicting compile-time macros or are redundant
    list(REMOVE_ITEM GINT_ALL_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/exceptions_overflow_test.cpp
        )

    add_executable(gint_all_tests ${GINT_ALL_TEST_SOURCES})
    target_include_directories(gint_all_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    # Prefer C++20 if available, otherwise C++17, else C++11
    if(HAS_CXX20)
        set_target_properties(gint_all_tests PROPERTIES CXX_STANDARD 20)
    elseif(HAS_CXX17)
        set_target_properties(gint_all_tests PROPERTIES CXX_STANDARD 17)
    else()
        set_target_properties(gint_all_tests PROPERTIES CXX_STANDARD 11)
    endif()
    target_compile_options(gint_all_tests PRIVATE ${GINT_TEST_OPTIONS})
    if(GINT_TEST_LINK_OPTIONS)
        target_link_options(gint_all_tests PRIVATE ${GINT_TEST_LINK_OPTIONS})
    endif()
    target_link_libraries(gint_all_tests PRIVATE GTest::gtest_main fmt::fmt)
    # Enable fmt integration across all-tests target
    target_compile_definitions(gint_all_tests PRIVATE GINT_ENABLE_FMT)
    # Prefix test names to make them discoverable uniquely if using CTest
    gtest_discover_tests(gint_all_tests NO_PRETTY_VALUES TEST_PREFIX "all.")
endif()

if(GINT_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)

    # General performance benchmarks
    add_executable(perf
        bench/performance.cpp
    )
    target_include_directories(perf PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_features(perf PRIVATE cxx_std_11)
    target_compile_definitions(perf PRIVATE GINT_ENABLE_FMT)
    target_link_libraries(perf PRIVATE fmt::fmt benchmark::benchmark)
    target_compile_options(perf PRIVATE -O3 -DNDEBUG)

    # Int128 comparison benchmarks
    add_executable(perf_compare_int128
        bench/compare_int128.cpp
    )
    target_compile_features(perf_compare_int128 PRIVATE cxx_std_11)
    target_include_directories(perf_compare_int128 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(perf_compare_int128 PRIVATE GINT_ENABLE_FMT)
    target_link_libraries(perf_compare_int128 PRIVATE fmt::fmt benchmark::benchmark)
    target_compile_options(perf_compare_int128 PRIVATE -O3 -DNDEBUG)

    # Int256 comparison benchmarks
    add_executable(perf_compare_int256
        bench/compare_int256.cpp
    )
    target_compile_features(perf_compare_int256 PRIVATE cxx_std_11)
    target_include_directories(perf_compare_int256 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(perf_compare_int256 PRIVATE GINT_ENABLE_FMT)
    target_link_libraries(perf_compare_int256 PRIVATE fmt::fmt benchmark::benchmark)
    target_compile_options(perf_compare_int256 PRIVATE -O3 -DNDEBUG)

    # Int256 comparison benchmarks with random values
    add_executable(perf_compare_int256_random
        bench/compare_int256_random.cpp
    )
    target_compile_features(perf_compare_int256_random PRIVATE cxx_std_11)
    target_include_directories(perf_compare_int256_random PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(perf_compare_int256_random PRIVATE GINT_ENABLE_FMT)
    target_link_libraries(perf_compare_int256_random PRIVATE fmt::fmt benchmark::benchmark)
    target_compile_options(perf_compare_int256_random PRIVATE -O3 -DNDEBUG)
endif()
