cmake_minimum_required(VERSION 3.13)
project(gint LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(GINT_BUILD_TESTS "Build test suite" ON)
option(GINT_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(GINT_BUILD_TESTS_CXX17 "Build C++17 test suite (gint_tests_cxx17)" OFF)
option(GINT_BUILD_TESTS_RELEASE "Build Release/O3 test suite (gint_tests_release)" ON)

set(GINT_TEST_OPTIONS -O0 -g)
set(GINT_TEST_RELEASE_OPTIONS -O3 -DNDEBUG)
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

    # Aggregate core unit tests into a single executable for readability and speed.
    # Divzero-check variants use separate executables because the macro policy is
    # target-wide at runtime and must not be mixed inside one test binary.
    set(GINT_TEST_SOURCES
        tests/arithmetic_basic_test.cpp
        tests/arithmetic_divmod_test.cpp
        tests/arithmetic_mul_test.cpp
        tests/bitwise_test.cpp
        tests/boundary_test.cpp
        tests/comparison_test.cpp
        tests/construction_test.cpp
        tests/conversion_test.cpp
        tests/float_interop_edge_test.cpp
        tests/fmt_support_test.cpp
        tests/numeric_limits_test.cpp
        tests/shift_test.cpp
        tests/stream_test.cpp
    )

    add_executable(gint_tests ${GINT_TEST_SOURCES})
    target_include_directories(gint_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(gint_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_compile_options(gint_tests PRIVATE ${GINT_TEST_OPTIONS})
    if(GINT_TEST_LINK_OPTIONS)
        target_link_options(gint_tests PRIVATE ${GINT_TEST_LINK_OPTIONS})
    endif()
    # fmt support is required by some tests; enable globally for the test target
    target_compile_definitions(gint_tests PRIVATE GINT_ENABLE_FMT GINT_TEST_ACCESS)
    target_link_libraries(gint_tests PRIVATE GTest::gtest_main fmt::fmt)

    # Increase discovery timeout to avoid flaky timeouts when listing tests
    gtest_discover_tests(gint_tests
        NO_PRETTY_VALUES
        DISCOVERY_TIMEOUT 60
    )

    # Run the same C++11 unit tests under Release/O3 by default. This catches
    # optimization-sensitive issues in the header-only fast paths while keeping
    # the primary test target debuggable.
    if(GINT_BUILD_TESTS_RELEASE AND NOT ENABLE_COVERAGE)
        add_executable(gint_tests_release ${GINT_TEST_SOURCES})
        target_include_directories(gint_tests_release PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(gint_tests_release PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
        target_compile_options(gint_tests_release PRIVATE ${GINT_TEST_RELEASE_OPTIONS})
        target_compile_definitions(gint_tests_release PRIVATE GINT_ENABLE_FMT GINT_TEST_ACCESS)
        target_link_libraries(gint_tests_release PRIVATE GTest::gtest_main fmt::fmt)
        gtest_discover_tests(gint_tests_release
            NO_PRETTY_VALUES
            TEST_PREFIX release.
            DISCOVERY_TIMEOUT 60
        )
    endif()

    add_executable(gint_tests_no_divzero_checks tests/divzero_unchecked_test.cpp)
    target_include_directories(gint_tests_no_divzero_checks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(gint_tests_no_divzero_checks PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_compile_options(gint_tests_no_divzero_checks PRIVATE ${GINT_TEST_OPTIONS})
    if(GINT_TEST_LINK_OPTIONS)
        target_link_options(gint_tests_no_divzero_checks PRIVATE ${GINT_TEST_LINK_OPTIONS})
    endif()
    target_link_libraries(gint_tests_no_divzero_checks PRIVATE GTest::gtest_main)
    gtest_discover_tests(gint_tests_no_divzero_checks
        TEST_PREFIX no_divzero_checks.
        NO_PRETTY_VALUES
        DISCOVERY_TIMEOUT 60
    )

    add_executable(gint_tests_no_divzero_checks_fastpaths tests/divzero_unchecked_test.cpp)
    target_include_directories(gint_tests_no_divzero_checks_fastpaths PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(gint_tests_no_divzero_checks_fastpaths PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_compile_options(gint_tests_no_divzero_checks_fastpaths PRIVATE ${GINT_TEST_OPTIONS})
    target_compile_definitions(gint_tests_no_divzero_checks_fastpaths PRIVATE GINT_GCC_TUNED_PATHS=1 GINT_CLANG_TUNED_PATHS=0)
    if(GINT_TEST_LINK_OPTIONS)
        target_link_options(gint_tests_no_divzero_checks_fastpaths PRIVATE ${GINT_TEST_LINK_OPTIONS})
    endif()
    target_link_libraries(gint_tests_no_divzero_checks_fastpaths PRIVATE GTest::gtest_main)
    gtest_discover_tests(gint_tests_no_divzero_checks_fastpaths
        TEST_PREFIX no_divzero_checks_fastpaths.
        NO_PRETTY_VALUES
        DISCOVERY_TIMEOUT 60
    )

    add_executable(gint_tests_gcc_tuned_fastpaths tests/gcc_tuned_fastpaths_test.cpp)
    target_include_directories(gint_tests_gcc_tuned_fastpaths PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(gint_tests_gcc_tuned_fastpaths PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_compile_options(gint_tests_gcc_tuned_fastpaths PRIVATE ${GINT_TEST_OPTIONS})
    target_compile_definitions(gint_tests_gcc_tuned_fastpaths PRIVATE GINT_GCC_TUNED_PATHS=1 GINT_CLANG_TUNED_PATHS=0)
    if(GINT_TEST_LINK_OPTIONS)
        target_link_options(gint_tests_gcc_tuned_fastpaths PRIVATE ${GINT_TEST_LINK_OPTIONS})
    endif()
    target_link_libraries(gint_tests_gcc_tuned_fastpaths PRIVATE GTest::gtest_main)
    gtest_discover_tests(gint_tests_gcc_tuned_fastpaths
        TEST_PREFIX gcc_tuned_fastpaths.
        NO_PRETTY_VALUES
        DISCOVERY_TIMEOUT 60
    )

    add_executable(gint_tests_divzero_checks tests/exceptions_overflow_test.cpp)
    target_include_directories(gint_tests_divzero_checks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(gint_tests_divzero_checks PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_compile_options(gint_tests_divzero_checks PRIVATE ${GINT_TEST_OPTIONS})
    if(GINT_TEST_LINK_OPTIONS)
        target_link_options(gint_tests_divzero_checks PRIVATE ${GINT_TEST_LINK_OPTIONS})
    endif()
    target_link_libraries(gint_tests_divzero_checks PRIVATE GTest::gtest_main)
    gtest_discover_tests(gint_tests_divzero_checks
        TEST_PREFIX divzero_checks.
        NO_PRETTY_VALUES
        DISCOVERY_TIMEOUT 60
    )

    # Optionally compile the core unit test suite under C++17 when explicitly enabled
    if(GINT_BUILD_TESTS_CXX17)
        check_cxx_compiler_flag("-std=c++17" HAS_CXX17)
        if(HAS_CXX17)
            add_executable(gint_tests_cxx17 ${GINT_TEST_SOURCES})
            target_include_directories(gint_tests_cxx17 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
            set_target_properties(gint_tests_cxx17 PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
            target_compile_options(gint_tests_cxx17 PRIVATE ${GINT_TEST_OPTIONS})
            if(GINT_TEST_LINK_OPTIONS)
                target_link_options(gint_tests_cxx17 PRIVATE ${GINT_TEST_LINK_OPTIONS})
            endif()
            target_compile_definitions(gint_tests_cxx17 PRIVATE GINT_ENABLE_FMT GINT_TEST_ACCESS)
            target_link_libraries(gint_tests_cxx17 PRIVATE GTest::gtest_main fmt::fmt)
            gtest_discover_tests(gint_tests_cxx17 NO_PRETTY_VALUES DISCOVERY_TIMEOUT 60)
        endif()
    endif()
endif()

if(GINT_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)

    add_executable(perf_benchmark_int256
        bench/benchmark_int256.cpp
    )
    set_target_properties(perf_benchmark_int256 PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_include_directories(perf_benchmark_int256 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )
    target_compile_definitions(perf_benchmark_int256 PRIVATE GINT_ENABLE_FMT)
    target_link_libraries(perf_benchmark_int256 PRIVATE fmt::fmt benchmark::benchmark)
    target_compile_options(perf_benchmark_int256 PRIVATE -O3 -DNDEBUG)

    add_executable(perf_compare_int256
        bench/benchmark_int256.cpp
    )
    set_target_properties(perf_compare_int256 PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_include_directories(perf_compare_int256 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )
    target_compile_definitions(perf_compare_int256 PRIVATE GINT_ENABLE_FMT GINT_ENABLE_CH_COMPARE GINT_ENABLE_BOOST_COMPARE)
    target_link_libraries(perf_compare_int256 PRIVATE fmt::fmt benchmark::benchmark)
    target_compile_options(perf_compare_int256 PRIVATE -O3 -DNDEBUG)

endif()
