# Run the actual install script inside DESTDIR, including absolute GNUInstallDirs.
# Each package test owns its staging directory and can run concurrently.
set(GINT_INSTALL_CMAKE_DIR "${GINT_INSTALL_LIBDIR}/cmake/gint")
set(GINT_INSTALL_DATA_DIR "${GINT_INSTALL_DATADIR}/gint")
foreach(kind IN ITEMS INCLUDE CMAKE DATA)
    set(dir "${GINT_INSTALL_${kind}_DIR}")
    if(NOT IS_ABSOLUTE "${dir}")
        set(dir "${GINT_INSTALL_PREFIX}/${dir}")
    endif()
    get_filename_component(logical_${kind} "${dir}" ABSOLUTE)
    get_filename_component(staged_${kind} "${stage_root}${dir}" ABSOLUTE)
    if(NOT staged_${kind} STREQUAL "${stage_root}${logical_${kind}}")
        message(FATAL_ERROR "install directory would escape the package test staging root: ${dir}")
    endif()
endforeach()

# CMake writes install_manifest.txt to its producer build even under DESTDIR.
# Configure the same install layout in a build private to this test case.
set(producer_build "${stage_root}-producer")
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
