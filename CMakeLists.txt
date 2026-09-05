cmake_minimum_required(VERSION 3.13)
project(gint
    VERSION 0.9.0
    DESCRIPTION "Fixed-width, strict-semantics, header-only wide integers for C++11"
    HOMEPAGE_URL "https://github.com/KodiakAS/gint"
    LANGUAGES CXX
)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gintCompilerContract.cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(GNUInstallDirs)

add_library(gint INTERFACE)
add_library(gint::gint ALIAS gint)
set_target_properties(gint PROPERTIES EXPORT_NAME gint)
target_compile_features(gint INTERFACE cxx_std_11)
target_include_directories(gint INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(gint_checked INTERFACE)
add_library(gint::checked ALIAS gint_checked)
set_target_properties(gint_checked PROPERTIES EXPORT_NAME checked)
target_link_libraries(gint_checked INTERFACE gint)
target_compile_definitions(gint_checked INTERFACE GINT_ENABLE_DIVZERO_CHECKS)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(GINT_IS_TOP_LEVEL ON)
else()
    set(GINT_IS_TOP_LEVEL OFF)
endif()

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer for tests" OFF)
option(GINT_BUILD_TESTS "Build test suite" ${GINT_IS_TOP_LEVEL})
option(GINT_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(GINT_BUILD_TESTS_CXX17 "Build C++17 test suite (gint_tests_cxx17)" OFF)
option(GINT_BUILD_TESTS_RELEASE "Build Release/O3 test suite (gint_tests_release)" ${GINT_IS_TOP_LEVEL})
option(GINT_INSTALL "Generate gint install rules" ${GINT_IS_TOP_LEVEL})

set(GINT_TEST_OPTIONS -O0 -g)
set(GINT_TEST_RELEASE_OPTIONS -O3 -DNDEBUG)
set(GINT_TEST_LINK_OPTIONS "")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND GINT_TEST_OPTIONS -Wall -Wextra -Werror)
endif()

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND GINT_TEST_OPTIONS --coverage)
        set(GINT_TEST_LINK_OPTIONS --coverage)
    endif()
endif()

if(ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND GINT_TEST_OPTIONS -fsanitize=address,undefined -fno-omit-frame-pointer)
        list(APPEND GINT_TEST_LINK_OPTIONS -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "ENABLE_SANITIZERS requires GCC or Clang")
    endif()
endif()

if(GINT_BUILD_TESTS OR GINT_BUILD_BENCHMARKS)
    find_package(fmt CONFIG QUIET)
    if(NOT fmt_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(fmt REQUIRED IMPORTED_TARGET GLOBAL fmt)
        add_library(fmt::fmt ALIAS PkgConfig::fmt)
    endif()
endif()

if(GINT_BUILD_TESTS)
    find_package(GTest REQUIRED)
    if(NOT TARGET GTest::gtest AND TARGET GTest::GTest)
        add_library(GTest::gtest INTERFACE IMPORTED)
        set_property(TARGET GTest::gtest PROPERTY INTERFACE_LINK_LIBRARIES GTest::GTest)
    endif()
    if(NOT TARGET GTest::gtest_main AND TARGET GTest::Main)
        add_library(GTest::gtest_main INTERFACE IMPORTED)
        set_property(TARGET GTest::gtest_main PROPERTY INTERFACE_LINK_LIBRARIES GTest::Main)
    endif()
    enable_testing()
    include(GoogleTest)
    include(CheckCXXCompilerFlag)

    function(add_gint_consumer_test target source standard)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE gint::gint)
        set_target_properties(${target} PROPERTIES
            CXX_STANDARD ${standard}
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
        endif()
        add_test(NAME consumer.${target} COMMAND ${target})
    endfunction()

    set(GINT_ODR_STATIC_SOURCES
        tests/consumer/odr_static_members.cpp
        tests/consumer/odr_static_members_other.cpp
    )
    add_gint_consumer_test(gint_consumer_odr_cxx11 "${GINT_ODR_STATIC_SOURCES}" 11)
    add_gint_consumer_test(gint_consumer_odr_cxx14 "${GINT_ODR_STATIC_SOURCES}" 14)
    add_gint_consumer_test(gint_consumer_core_header tests/consumer/core_header.cpp 11)
    add_gint_consumer_test(gint_consumer_core_then_umbrella tests/consumer/core_then_umbrella.cpp 11)
    add_gint_consumer_test(gint_consumer_header_contract tests/consumer/header_contract.cpp 11)
    add_gint_consumer_test(gint_consumer_gcc48_compat tests/consumer/gcc48_compat.cpp 11)
    add_gint_consumer_test(gint_consumer_no_exceptions tests/consumer/no_exceptions.cpp 11)
    target_compile_options(gint_consumer_no_exceptions PRIVATE -fno-exceptions)

    set(GINT_CONFIG_ISOLATION_SOURCES
        tests/consumer/config_checked.cpp
        tests/consumer/config_unchecked.cpp
        tests/consumer/config_gcc_tuned.cpp
        tests/consumer/config_isolation_main.cpp
    )
    add_gint_consumer_test(gint_consumer_config_isolation "${GINT_CONFIG_ISOLATION_SOURCES}" 11)
    list(REVERSE GINT_CONFIG_ISOLATION_SOURCES)
    add_gint_consumer_test(gint_consumer_config_isolation_reversed "${GINT_CONFIG_ISOLATION_SOURCES}" 11)

    set(GINT_CXX23_FLAG -std=c++2b)
    check_cxx_compiler_flag("${GINT_CXX23_FLAG}" GINT_HAS_CXX23_FLAG)
    if(GINT_HAS_CXX23_FLAG)
        add_executable(gint_consumer_cxx23_werror tests/consumer/cxx23_werror.cpp)
        target_include_directories(gint_consumer_cxx23_werror PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_compile_options(gint_consumer_cxx23_werror PRIVATE ${GINT_CXX23_FLAG})
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(gint_consumer_cxx23_werror PRIVATE -Wall -Wextra -Wdeprecated-declarations -Werror)
        endif()
        add_test(NAME consumer.gint_consumer_cxx23_werror COMMAND gint_consumer_cxx23_werror)
    endif()

    add_test(NAME consumer.header_contract_negative
        COMMAND ${CMAKE_COMMAND}
            -DGINT_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DGINT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DGINT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_header_contract.cmake
    )
    add_test(NAME consumer.cmake_compiler_contract
        COMMAND ${CMAKE_COMMAND}
            -DGINT_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DGINT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DGINT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_cmake_compiler_contract.cmake
    )

    # Snapshot the selected toolchain as a cache preload file. Bracket quoting
    # preserves paths with spaces and list-valued architecture settings.
    set(GINT_CONSUMER_TOOLCHAIN_VARIABLES
        CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_ARG1 CMAKE_TOOLCHAIN_FILE
        CMAKE_CXX_COMPILER_TARGET CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN
        CMAKE_SYSROOT CMAKE_SYSROOT_COMPILE CMAKE_SYSROOT_LINK
        CMAKE_OSX_SYSROOT CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET
    )
    set(gint_consumer_flag_variables CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS)
    set(gint_consumer_configurations Debug Release RelWithDebInfo MinSizeRel
        ${CMAKE_BUILD_TYPE} ${CMAKE_CONFIGURATION_TYPES})
    list(REMOVE_DUPLICATES gint_consumer_configurations)
    foreach(configuration IN LISTS gint_consumer_configurations)
        string(TOUPPER "${configuration}" configuration_upper)
        list(APPEND gint_consumer_flag_variables
            CMAKE_CXX_FLAGS_${configuration_upper}
            CMAKE_EXE_LINKER_FLAGS_${configuration_upper})
    endforeach()
    list(REMOVE_DUPLICATES gint_consumer_flag_variables)
    list(APPEND GINT_CONSUMER_TOOLCHAIN_VARIABLES ${gint_consumer_flag_variables})
    set(gint_consumer_toolchain "${CMAKE_CURRENT_BINARY_DIR}/consumer-toolchain.cmake")
    file(WRITE "${gint_consumer_toolchain}" "# Generated consumer toolchain cache.\n")
    foreach(variable IN LISTS GINT_CONSUMER_TOOLCHAIN_VARIABLES)
        if(DEFINED ${variable})
            file(APPEND "${gint_consumer_toolchain}"
                "set(${variable} [==[${${variable}}]==] CACHE STRING \"Parent toolchain\" FORCE)\n")
        endif()
    endforeach()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/consumer-compiler-check.cmake"
        "if(NOT CMAKE_CXX_COMPILER STREQUAL [==[${CMAKE_CXX_COMPILER}]==] OR\n"
        "   NOT CMAKE_CXX_COMPILER_ID STREQUAL [==[${CMAKE_CXX_COMPILER_ID}]==] OR\n"
        "   NOT CMAKE_CXX_COMPILER_VERSION STREQUAL [==[${CMAKE_CXX_COMPILER_VERSION}]==])\n"
        "    message(FATAL_ERROR \"consumer compiler differs from parent toolchain\")\n"
        "endif()\n")

    foreach(variable IN LISTS gint_consumer_flag_variables)
        if(DEFINED ${variable})
            file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/consumer-compiler-check.cmake"
                "if(NOT ${variable} STREQUAL [==[${${variable}}]==])\n"
                "    message(FATAL_ERROR \"consumer ${variable} differs from parent toolchain\")\n"
                "endif()\n")
        endif()
    endforeach()

    add_test(NAME consumer.subproject
        COMMAND ${CMAKE_COMMAND}
            -DGINT_CONSUMER_KIND=subproject
            -DGINT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DGINT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_consumer.cmake
    )
    if(GINT_INSTALL)
        math(EXPR GINT_TEST_NEWER_PATCH "${PROJECT_VERSION_PATCH} + 1")
        if(PROJECT_VERSION_MINOR GREATER 0)
            math(EXPR GINT_TEST_OTHER_MINOR "${PROJECT_VERSION_MINOR} - 1")
        else()
            math(EXPR GINT_TEST_OTHER_MINOR "${PROJECT_VERSION_MINOR} + 1")
        endif()
        set(GINT_PACKAGE_TEST_INSTALL_ARGS
            -DGINT_INSTALL_CMAKE_DIR=${CMAKE_INSTALL_LIBDIR}/cmake/gint
            -DGINT_INSTALL_DATA_DIR=${CMAKE_INSTALL_DATADIR}/gint
        )

        add_test(NAME consumer.package
            COMMAND ${CMAKE_COMMAND}
                ${GINT_PACKAGE_TEST_INSTALL_ARGS}
                -DGINT_CONSUMER_KIND=package
                -DGINT_EXPECT_PACKAGE_VERSION=${PROJECT_VERSION}
                -DGINT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DGINT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_consumer.cmake
        )
        add_test(NAME consumer.package.version_exact
            COMMAND ${CMAKE_COMMAND}
                ${GINT_PACKAGE_TEST_INSTALL_ARGS}
                -DGINT_CONSUMER_KIND=package
                -DGINT_CONSUMER_CASE=package-version-exact
                -DGINT_PACKAGE_VERSION=${PROJECT_VERSION}
                -DGINT_PACKAGE_VERSION_EXACT=ON
                -DGINT_EXPECT_PACKAGE_VERSION=${PROJECT_VERSION}
                -DGINT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DGINT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_consumer.cmake
        )
        add_test(NAME consumer.package.version_compatible
            COMMAND ${CMAKE_COMMAND}
                ${GINT_PACKAGE_TEST_INSTALL_ARGS}
                -DGINT_CONSUMER_KIND=package
                -DGINT_CONSUMER_CASE=package-version-compatible
                -DGINT_PACKAGE_VERSION=${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
                -DGINT_EXPECT_PACKAGE_VERSION=${PROJECT_VERSION}
                -DGINT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DGINT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_consumer.cmake
        )
        add_test(NAME consumer.package.version_newer_rejected
            COMMAND ${CMAKE_COMMAND}
                ${GINT_PACKAGE_TEST_INSTALL_ARGS}
                -DGINT_CONSUMER_KIND=package
                -DGINT_CONSUMER_CASE=package-version-newer-rejected
                -DGINT_PACKAGE_VERSION=${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${GINT_TEST_NEWER_PATCH}
                -DGINT_EXPECT_CONFIGURE_FAILURE=ON
                -DGINT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DGINT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_consumer.cmake
        )
        add_test(NAME consumer.package.version_other_minor_rejected
            COMMAND ${CMAKE_COMMAND}
                ${GINT_PACKAGE_TEST_INSTALL_ARGS}
                -DGINT_CONSUMER_KIND=package
                -DGINT_CONSUMER_CASE=package-version-other-minor-rejected
                -DGINT_PACKAGE_VERSION=${PROJECT_VERSION_MAJOR}.${GINT_TEST_OTHER_MINOR}
                -DGINT_EXPECT_CONFIGURE_FAILURE=ON
                -DGINT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DGINT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/run_consumer.cmake
        )
        add_test(NAME consumer.package.compiler_contract
            COMMAND ${CMAKE_COMMAND}
                -DGINT_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                -DGINT_INSTALL_CMAKE_DIR=${CMAKE_INSTALL_LIBDIR}/cmake/gint
                -DGINT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DGINT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/check_installed_compiler_contract.cmake
        )
    endif()

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
        tests/hash_test.cpp
        tests/numeric_limits_test.cpp
        tests/property_test.cpp
        tests/shift_test.cpp
        tests/stream_test.cpp
    )

    # Keep each policy/optimization variant in its own executable. Only the
    # target setup is shared; Release intentionally omits debug instrumentation.
    function(add_gint_unit_test target sources standard prefix)
        cmake_parse_arguments(TEST "RELEASE" "" "DEFINITIONS;LIBRARIES" ${ARGN})
        add_executable(${target} ${sources})
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(${target} PROPERTIES
            CXX_STANDARD ${standard} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
        if(TEST_RELEASE)
            target_compile_options(${target} PRIVATE ${GINT_TEST_RELEASE_OPTIONS})
        else()
            target_compile_options(${target} PRIVATE ${GINT_TEST_OPTIONS})
            if(GINT_TEST_LINK_OPTIONS)
                target_link_options(${target} PRIVATE ${GINT_TEST_LINK_OPTIONS})
            endif()
        endif()
        target_compile_definitions(${target} PRIVATE ${TEST_DEFINITIONS})
        target_link_libraries(${target} PRIVATE GTest::gtest_main ${TEST_LIBRARIES})
        gtest_discover_tests(${target}
            TEST_PREFIX "${prefix}" NO_PRETTY_VALUES DISCOVERY_TIMEOUT 60)
    endfunction()

    set(GINT_CORE_TEST_ARGS DEFINITIONS GINT_ENABLE_FMT GINT_TEST_ACCESS LIBRARIES fmt::fmt)
    set(GINT_FASTPATH_TEST_ARGS DEFINITIONS GINT_GCC_TUNED_PATHS=1 GINT_CLANG_TUNED_PATHS=0)
    add_gint_unit_test(gint_tests "${GINT_TEST_SOURCES}" 11 "" ${GINT_CORE_TEST_ARGS})

    # Exercise the same C++11 cases under O3 to catch optimization-sensitive bugs.
    if(GINT_BUILD_TESTS_RELEASE AND NOT ENABLE_COVERAGE)
        add_gint_unit_test(gint_tests_release "${GINT_TEST_SOURCES}" 11 "release."
            RELEASE ${GINT_CORE_TEST_ARGS})
    endif()

    add_gint_unit_test(gint_tests_no_divzero_checks tests/divzero_unchecked_test.cpp 11 "no_divzero_checks.")
    add_gint_unit_test(gint_tests_no_divzero_checks_fastpaths tests/divzero_unchecked_test.cpp 11
        "no_divzero_checks_fastpaths." ${GINT_FASTPATH_TEST_ARGS})

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        foreach(policy IN ITEMS checked unchecked)
            set(target gint_divzero_${policy}_dso)
            add_library(${target} SHARED tests/divzero_dso_library.cpp)
            target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
            set_target_properties(${target} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
            target_compile_options(${target} PRIVATE ${GINT_TEST_OPTIONS})
            if(GINT_TEST_LINK_OPTIONS)
                target_link_options(${target} PRIVATE ${GINT_TEST_LINK_OPTIONS})
            endif()
        endforeach()
        target_compile_definitions(gint_divzero_checked_dso PRIVATE GINT_ENABLE_DIVZERO_CHECKS)

        add_executable(gint_tests_divzero_dso tests/divzero_dso_loader_test.cpp)
        target_compile_definitions(gint_tests_divzero_dso
            PRIVATE
                GINT_DSO_CHECKED_PATH="$<TARGET_FILE:gint_divzero_checked_dso>"
                GINT_DSO_UNCHECKED_PATH="$<TARGET_FILE:gint_divzero_unchecked_dso>"
        )
        set_target_properties(gint_tests_divzero_dso PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
        target_compile_options(gint_tests_divzero_dso PRIVATE ${GINT_TEST_OPTIONS})
        if(GINT_TEST_LINK_OPTIONS)
            target_link_options(gint_tests_divzero_dso PRIVATE ${GINT_TEST_LINK_OPTIONS})
        endif()
        target_link_libraries(gint_tests_divzero_dso PRIVATE GTest::gtest_main ${CMAKE_DL_LIBS})
        add_dependencies(gint_tests_divzero_dso gint_divzero_checked_dso gint_divzero_unchecked_dso)
        gtest_discover_tests(gint_tests_divzero_dso
            TEST_PREFIX divzero_dso.
            NO_PRETTY_VALUES
            DISCOVERY_TIMEOUT 60
        )
    endif()

    add_gint_unit_test(gint_tests_gcc_tuned_fastpaths tests/gcc_tuned_fastpaths_test.cpp 11
        "gcc_tuned_fastpaths." ${GINT_FASTPATH_TEST_ARGS})
    add_gint_unit_test(gint_tests_divzero_checks tests/exceptions_overflow_test.cpp 11 "divzero_checks.")

    # Optionally compile the core unit test suite under C++17 when explicitly enabled.
    if(GINT_BUILD_TESTS_CXX17)
        check_cxx_compiler_flag("-std=c++17" HAS_CXX17)
        if(HAS_CXX17)
            add_gint_unit_test(gint_tests_cxx17 "${GINT_TEST_SOURCES}" 17 "" ${GINT_CORE_TEST_ARGS})
        endif()
    endif()
endif()

if(GINT_BUILD_BENCHMARKS)
    include(CheckCXXCompilerFlag)
    find_package(benchmark REQUIRED)

    set(GINT_BENCH_COMPILE_OPTIONS -O3 -DNDEBUG)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        check_cxx_compiler_flag("-mno-outline" GINT_HAS_MNO_OUTLINE)
        if(GINT_HAS_MNO_OUTLINE)
            list(APPEND GINT_BENCH_COMPILE_OPTIONS -mno-outline)
        endif()
    endif()

    function(add_gint_bench_target target bits compare)
        add_executable(${target}
            bench/benchmark_int256.cpp
        )
        set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
        target_include_directories(${target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        )
        target_compile_definitions(${target} PRIVATE GINT_ENABLE_FMT GINT_BENCH_BITS=${bits})
        if(compare)
            target_compile_definitions(${target} PRIVATE GINT_ENABLE_CH_COMPARE GINT_ENABLE_BOOST_COMPARE)
        endif()
        target_link_libraries(${target} PRIVATE fmt::fmt benchmark::benchmark)
        target_compile_options(${target} PRIVATE ${GINT_BENCH_COMPILE_OPTIONS})
    endfunction()

    foreach(bits IN ITEMS 128 256 512 1024)
        add_gint_bench_target(perf_benchmark_int${bits} ${bits} OFF)
        add_gint_bench_target(perf_compare_int${bits} ${bits} ON)
    endforeach()

endif()

if(GINT_INSTALL)
    include(CMakePackageConfigHelpers)

    install(TARGETS gint gint_checked EXPORT gintTargets)
    install(FILES
        include/gint/core.h
        include/gint/gint.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gint
    )
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/gint)
    install(EXPORT gintTargets
        FILE gintTargets.cmake
        NAMESPACE gint::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gint
    )
    install(FILES cmake/gintCompilerContract.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gint
    )

    configure_package_config_file(
        cmake/gintConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/gintConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gint
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/gintConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/gintConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/gintConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gint
    )
endif()
