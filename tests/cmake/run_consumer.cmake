if(NOT DEFINED GINT_CONSUMER_KIND OR NOT DEFINED GINT_SOURCE_DIR OR NOT DEFINED GINT_BINARY_DIR)
    message(FATAL_ERROR "GINT_CONSUMER_KIND, GINT_SOURCE_DIR, and GINT_BINARY_DIR are required")
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
    set(install_prefix "${GINT_BINARY_DIR}/consumer-${consumer_case}-prefix")
    file(REMOVE_RECURSE "${install_prefix}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DCMAKE_INSTALL_PREFIX=${install_prefix}"
            -P "${GINT_BINARY_DIR}/cmake_install.cmake"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "failed to install gint package: ${result}")
    endif()

    if(NOT DEFINED GINT_INSTALL_CMAKE_DIR)
        set(GINT_INSTALL_CMAKE_DIR "lib/cmake/gint")
    endif()
    if(NOT DEFINED GINT_INSTALL_DATA_DIR)
        set(GINT_INSTALL_DATA_DIR "share/gint")
    endif()
    set(expected_install_files
        "include/gint/core.h"
        "include/gint/gint.h"
        "${GINT_INSTALL_CMAKE_DIR}/gintConfig.cmake"
        "${GINT_INSTALL_CMAKE_DIR}/gintConfigVersion.cmake"
        "${GINT_INSTALL_CMAKE_DIR}/gintCompilerContract.cmake"
        "${GINT_INSTALL_CMAKE_DIR}/gintTargets.cmake"
        "${GINT_INSTALL_DATA_DIR}/LICENSE"
    )
    foreach(relative_path IN LISTS expected_install_files)
        if(NOT EXISTS "${install_prefix}/${relative_path}")
            message(FATAL_ERROR "installed package is missing ${relative_path}")
        endif()
    endforeach()

    file(GLOB_RECURSE installed_files
        LIST_DIRECTORIES FALSE
        RELATIVE "${install_prefix}"
        "${install_prefix}/*"
    )
    list(SORT installed_files)
    list(SORT expected_install_files)
    if(NOT "${installed_files}" STREQUAL "${expected_install_files}")
        message(FATAL_ERROR
            "installed package manifest mismatch\n"
            "expected: ${expected_install_files}\n"
            "actual:   ${installed_files}"
        )
    endif()

    file(GLOB_RECURSE installed_libraries LIST_DIRECTORIES FALSE
        "${install_prefix}/*.a"
        "${install_prefix}/*.dll"
        "${install_prefix}/*.dylib"
        "${install_prefix}/*.lib"
        "${install_prefix}/*.so"
        "${install_prefix}/*.so.*"
    )
    if(installed_libraries)
        message(FATAL_ERROR "header-only package installed binary libraries: ${installed_libraries}")
    endif()

    set(configure_args -DCMAKE_PREFIX_PATH=${install_prefix})
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
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_build}"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "failed to build ${GINT_CONSUMER_KIND} consumer: ${result}")
endif()
