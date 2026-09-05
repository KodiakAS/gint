# Both source and installed-package entry points must reject unsupported
# compilers. Keep their projects separate, sharing only the probe machinery.
function(expect_configure_failure name compiler_id compiler_version simulate_id expected_diagnostic)
    set(toolchain "${contract_dir}/${name}-toolchain.cmake")
    file(WRITE "${toolchain}"
        "set(CMAKE_CXX_COMPILER [==[${GINT_CXX_COMPILER}]==])\n"
        "set(CMAKE_CXX_COMPILER_FORCED TRUE)\n"
        "set(CMAKE_CXX_COMPILER_ID_RUN TRUE)\n"
        "set(CMAKE_CXX_COMPILER_ID [==[${compiler_id}]==])\n"
        "set(CMAKE_CXX_COMPILER_VERSION [==[${compiler_version}]==])\n"
        "set(CMAKE_CXX_SIMULATE_ID [==[${simulate_id}]==])\n"
    )

    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -S "${contract_source}"
            -B "${contract_dir}/${name}-build"
            "-DCMAKE_TOOLCHAIN_FILE=${toolchain}"
            ${contract_configure_args}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )
    if(result EQUAL 0)
        message(FATAL_ERROR "${name} unexpectedly configured successfully")
    endif()

    set(output "${stdout}\n${stderr}")
    string(FIND "${output}" "${expected_diagnostic}" diagnostic_position)
    if(diagnostic_position EQUAL -1)
        message(FATAL_ERROR
            "${name} failed without the expected diagnostic '${expected_diagnostic}':\n${output}"
        )
    endif()
endfunction()
