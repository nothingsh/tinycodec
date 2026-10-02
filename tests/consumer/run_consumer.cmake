# Installs tinycodec from BUILD_DIR into a fresh prefix under WORK_DIR, then
# configures, builds and runs the project in SOURCE_DIR against that prefix.
# CXX_COMPILER, CXX_FLAGS and BUILD_TYPE are those of the tinycodec build, so
# that the consumer is compiled the same way (this matters for sanitizers).

file(REMOVE_RECURSE "${WORK_DIR}")

function(run_step description)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${description} failed:\n${output}")
    endif()
endfunction()

run_step("install"
    "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${WORK_DIR}/prefix")
run_step("configure consumer"
    "${CMAKE_COMMAND}" -S "${SOURCE_DIR}" -B "${WORK_DIR}/build"
        "-DCMAKE_PREFIX_PATH=${WORK_DIR}/prefix"
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
        "-DCMAKE_CXX_FLAGS=${CXX_FLAGS}"
        "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}")
run_step("build consumer"
    "${CMAKE_COMMAND}" --build "${WORK_DIR}/build")
run_step("run consumer"
    "${WORK_DIR}/build/consumer")
