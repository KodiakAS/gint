if(NOT DEFINED GINT_CONSUMER_KIND OR NOT DEFINED GINT_SOURCE_DIR OR NOT DEFINED GINT_BINARY_DIR)
    message(FATAL_ERROR "GINT_CONSUMER_KIND, GINT_SOURCE_DIR, and GINT_BINARY_DIR are required")
endif()

if(GINT_CONSUMER_MULTI_CONFIG AND NOT GINT_CONSUMER_CONFIG)
    message(FATAL_ERROR "multi-config consumer tests require ctest -C <configuration>")
endif()
set(build_args)
if(GINT_CONSUMER_CONFIG)
    list(APPEND build_args --config "${GINT_CONSUMER_CONFIG}")
endif()

set(consumer_source "${GINT_SOURCE_DIR}/tests/consumer/${GINT_CONSUMER_KIND}")
if(DEFINED GINT_CONSUMER_CASE)
    set(consumer_case "${GINT_CONSUMER_CASE}")
else()
    set(consumer_case "${GINT_CONSUMER_KIND}")
endif()
set(consumer_build "${GINT_BINARY_DIR}/consumer-${consumer_case}")
file(REMOVE_RECURSE "${consumer_build}")

if(GINT_CONSUMER_KIND STREQUAL "package")
    set(stage_root "${GINT_BINARY_DIR}/consumer-${consumer_case}-prefix")
    file(REMOVE_RECURSE "${stage_root}")
    include("${CMAKE_CURRENT_LIST_DIR}/stage_package.cmake")
    set(expected_install_files
        "${staged_INCLUDE}/gint/gint.h"
        "${staged_CMAKE}/gintConfig.cmake"
        "${staged_CMAKE}/gintConfigVersion.cmake"
        "${staged_CMAKE}/gintCompilerContract.cmake"
        "${staged_CMAKE}/gintTargets.cmake"
        "${staged_DATA}/LICENSE"
    )
    file(GLOB_RECURSE installed_files LIST_DIRECTORIES FALSE "${stage_root}/*")
    list(SORT installed_files)
    list(SORT expected_install_files)
    if(NOT "${installed_files}" STREQUAL "${expected_install_files}")
        message(FATAL_ERROR
            "installed package manifest mismatch\n"
            "expected: ${expected_install_files}\n"
            "actual:   ${installed_files}"
        )
    endif()

    set(configure_args "-DCMAKE_PREFIX_PATH=${stage_root}${GINT_INSTALL_PREFIX}")
    # Prefix-only discovery covers the default layout. Custom libdirs may
    # place the config outside CMake's standard package search locations.
    if(NOT GINT_INSTALL_CMAKE_DIR STREQUAL "lib/cmake/gint")
        list(APPEND configure_args "-Dgint_DIR=${staged_CMAKE}")
    endif()
    if(IS_ABSOLUTE "${GINT_INSTALL_INCLUDE_DIR}" OR IS_ABSOLUTE "${GINT_INSTALL_CMAKE_DIR}")
        list(APPEND configure_args "-DGINT_LOGICAL_INCLUDE_DIR=${logical_INCLUDE}"
            "-DGINT_STAGED_INCLUDE_DIR=${staged_INCLUDE}")
    endif()
    if(DEFINED GINT_PACKAGE_VERSION)
        list(APPEND configure_args -DGINT_FIND_VERSION=${GINT_PACKAGE_VERSION})
    endif()
    if(GINT_PACKAGE_VERSION_EXACT)
        list(APPEND configure_args -DGINT_FIND_EXACT=ON)
    endif()
    if(DEFINED GINT_EXPECT_PACKAGE_VERSION)
        list(APPEND configure_args -DGINT_EXPECT_VERSION=${GINT_EXPECT_PACKAGE_VERSION})
    endif()
elseif(GINT_CONSUMER_KIND STREQUAL "subproject")
    set(configure_args -DGINT_SOURCE_DIR=${GINT_SOURCE_DIR})
else()
    message(FATAL_ERROR "unknown consumer kind: ${GINT_CONSUMER_KIND}")
endif()

if(GINT_CONSUMER_GENERATOR)
    list(APPEND configure_args -G "${GINT_CONSUMER_GENERATOR}")
endif()
if(GINT_CONSUMER_GENERATOR_PLATFORM)
    list(APPEND configure_args -A "${GINT_CONSUMER_GENERATOR_PLATFORM}")
endif()
if(GINT_CONSUMER_GENERATOR_TOOLSET)
    list(APPEND configure_args -T "${GINT_CONSUMER_GENERATOR_TOOLSET}")
endif()

list(APPEND configure_args
    -C "${GINT_BINARY_DIR}/consumer-toolchain.cmake"
    "-DGINT_COMPILER_CHECK=${GINT_BINARY_DIR}/consumer-compiler-check.cmake"
)

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${consumer_source}" -B "${consumer_build}" ${configure_args}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr
)

if(GINT_EXPECT_CONFIGURE_FAILURE)
    if(result EQUAL 0)
        message(FATAL_ERROR
            "${consumer_case} unexpectedly accepted package version ${GINT_PACKAGE_VERSION}"
        )
    endif()
    set(configure_output "${configure_stdout}\n${configure_stderr}")
    string(FIND "${configure_output}" "requested version" requested_version_position)
    string(FIND "${configure_output}" "${GINT_PACKAGE_VERSION}" requested_value_position)
    if(requested_version_position EQUAL -1 OR requested_value_position EQUAL -1)
        message(FATAL_ERROR
            "${consumer_case} failed for a reason other than version compatibility:\n${configure_output}"
        )
    endif()
    message(STATUS
        "${consumer_case} rejected package version ${GINT_PACKAGE_VERSION} as expected"
    )
    return()
endif()

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "failed to configure ${consumer_case} consumer: ${result}\n${configure_stdout}\n${configure_stderr}"
    )
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_build}" ${build_args}
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "failed to build ${GINT_CONSUMER_KIND} consumer: ${result}")
endif()
