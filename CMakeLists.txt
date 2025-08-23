cmake_minimum_required(VERSION 3.10)
project(gint LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(WI_BUILD_TESTS "Build test suite" ON)
option(WI_BUILD_BENCHMARKS "Build benchmarks" OFF)

set(WI_TEST_OPTIONS -O0 -g)
set(WI_TEST_LINK_OPTIONS "")

if(ENABLE_COVERAGE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND WI_TEST_OPTIONS --coverage)
        set(WI_TEST_LINK_OPTIONS --coverage)
    endif()
endif()

if(WI_BUILD_TESTS OR WI_BUILD_BENCHMARKS)
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

add_library(gint INTERFACE)

target_include_directories(gint INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gint INTERFACE cxx_std_11)

if(WI_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    include(GoogleTest)
    function(add_gint_test name source std)
        add_executable(${name} ${source})
        target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(${name} PROPERTIES CXX_STANDARD ${std} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS YES)
        target_compile_options(${name} PRIVATE ${WI_TEST_OPTIONS})
        if(WI_TEST_LINK_OPTIONS)
            target_link_options(${name} PRIVATE ${WI_TEST_LINK_OPTIONS})
        endif()
        target_link_libraries(${name} PRIVATE GTest::gtest_main)
        gtest_discover_tests(${name} NO_PRETTY_VALUES)
    endfunction()

    add_gint_test(gint_test tests/gint_test.cpp 11)
    target_compile_definitions(gint_test PRIVATE GINT_ENABLE_FMT)
    target_link_libraries(gint_test PRIVATE fmt::fmt)

    add_gint_test(gint_fmt_test tests/fmt_support_test.cpp 11)
    target_compile_definitions(gint_fmt_test PRIVATE GINT_ENABLE_FMT)
    target_link_libraries(gint_fmt_test PRIVATE fmt::fmt)
endif()

if(WI_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)

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
