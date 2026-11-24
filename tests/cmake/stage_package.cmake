# The fixture stages once for all package/version consumers. The installed
# compiler contract supplies its own independent stage_root.
if(NOT DEFINED stage_root)
    set(shared_stage ON)
    if(GINT_CONSUMER_MULTI_CONFIG AND NOT GINT_CONSUMER_CONFIG)
        message(FATAL_ERROR "multi-config consumer tests require ctest -C <configuration>")
    endif()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/package_install_layout.cmake")

# CMake writes install_manifest.txt to its producer build even under DESTDIR.
# Keep that manifest in a private producer build.
set(producer_build "${stage_root}-producer")
if(shared_stage)
    file(REMOVE_RECURSE "${stage_root}" "${producer_build}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/consumer_generator.cmake")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${GINT_SOURCE_DIR}" -B "${producer_build}"
        ${generator_args} -C "${GINT_BINARY_DIR}/consumer-toolchain.cmake"
        -DGINT_INSTALL=ON -DGINT_BUILD_TESTS=OFF -DGINT_BUILD_BENCHMARKS=OFF
        "-DCMAKE_INSTALL_PREFIX=${GINT_INSTALL_PREFIX}"
        "-DCMAKE_INSTALL_INCLUDEDIR=${GINT_INSTALL_INCLUDE_DIR}"
        "-DCMAKE_INSTALL_LIBDIR=${GINT_INSTALL_LIBDIR}"
        "-DCMAKE_INSTALL_DATADIR=${GINT_INSTALL_DATADIR}"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "failed to configure package test producer: ${result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "DESTDIR=${stage_root}"
        "${CMAKE_COMMAND}" "-DCMAKE_INSTALL_PREFIX=${GINT_INSTALL_PREFIX}"
        "-DCMAKE_INSTALL_CONFIG_NAME=${GINT_CONSUMER_CONFIG}"
        -P "${producer_build}/cmake_install.cmake"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "failed to stage gint package: ${result}")
endif()

if(shared_stage)
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
endif()
