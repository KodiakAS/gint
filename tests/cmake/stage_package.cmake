# Run the actual install script inside DESTDIR, including absolute GNUInstallDirs.
# Each package test owns its staging directory and can run concurrently.
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

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "DESTDIR=${stage_root}"
        "${CMAKE_COMMAND}" "-DCMAKE_INSTALL_PREFIX=${GINT_INSTALL_PREFIX}"
        "-DCMAKE_INSTALL_CONFIG_NAME=${GINT_CONSUMER_CONFIG}"
        -P "${GINT_BINARY_DIR}/cmake_install.cmake"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "failed to stage gint package: ${result}")
endif()
