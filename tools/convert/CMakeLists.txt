add_executable(tinycodec-convert main.cpp)
target_link_libraries(tinycodec-convert PRIVATE tinycodec::json tinycodec::msgpack)
target_compile_features(tinycodec-convert PRIVATE cxx_std_17)
set_target_properties(tinycodec-convert PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tinycodec-convert PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS tinycodec-convert RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(TINYCODEC_BUILD_TESTS)
    # Each test runs the tool through test/run_convert.cmake, which compares
    # the exit code, standard output and standard error with what is expected.
    # The expected standard output is a file, compared byte for byte.
    function(tinycodec_add_convert_test name)
        cmake_parse_arguments(ARG "" "EXIT_CODE;STDIN_FILE;STDOUT_FILE;STDERR_TEXT" "ARGS" ${ARGN})
        string(REPLACE ";" "|" joined_args "${ARG_ARGS}")
        add_test(NAME ${name}
            COMMAND ${CMAKE_COMMAND}
                -DCONVERT=$<TARGET_FILE:tinycodec-convert>
                -DACTUAL_FILE=${CMAKE_CURRENT_BINARY_DIR}/${name}.stdout
                "-DARGS=${joined_args}"
                -DEXIT_CODE=${ARG_EXIT_CODE}
                "-DSTDIN_FILE=${ARG_STDIN_FILE}"
                "-DSTDOUT_FILE=${ARG_STDOUT_FILE}"
                "-DSTDERR_TEXT=${ARG_STDERR_TEXT}"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/test/run_convert.cmake
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
    endfunction()

    tinycodec_add_convert_test(tinycodec_convert_default_indent
        ARGS sample.json
        EXIT_CODE 0
        STDOUT_FILE sample.indent2.json)
    tinycodec_add_convert_test(tinycodec_convert_indent_4
        ARGS --indent 4 sample.json
        EXIT_CODE 0
        STDOUT_FILE sample.indent4.json)
    tinycodec_add_convert_test(tinycodec_convert_compact
        ARGS --indent 0 sample.json
        EXIT_CODE 0
        STDOUT_FILE sample.compact.json)
    tinycodec_add_convert_test(tinycodec_convert_stdin
        ARGS --indent 0
        STDIN_FILE sample.json
        EXIT_CODE 0
        STDOUT_FILE sample.compact.json)
    tinycodec_add_convert_test(tinycodec_convert_stdin_dash
        ARGS -
        STDIN_FILE sample.json
        EXIT_CODE 0
        STDOUT_FILE sample.indent2.json)
    tinycodec_add_convert_test(tinycodec_convert_stdin_parse_error
        STDIN_FILE invalid.json
        EXIT_CODE 1
        STDERR_TEXT "<stdin>:3:16: UnexpectedChar\n")
    tinycodec_add_convert_test(tinycodec_convert_parse_error
        ARGS invalid.json
        EXIT_CODE 1
        STDERR_TEXT "invalid.json:3:16: UnexpectedChar\n")
    tinycodec_add_convert_test(tinycodec_convert_empty_file
        ARGS empty.json
        EXIT_CODE 1
        STDERR_TEXT "empty.json:1:1: UnexpectedEnd\n")
    tinycodec_add_convert_test(tinycodec_convert_missing_file
        ARGS no-such-file.json
        EXIT_CODE 2
        STDERR_TEXT "tinycodec-convert: cannot read no-such-file.json\n")
    tinycodec_add_convert_test(tinycodec_convert_bad_indent
        ARGS --indent abc sample.json
        EXIT_CODE 2
        STDERR_TEXT "usage: tinycodec-convert [--from FORMAT] [--to FORMAT] [--indent N] [FILE]\n")
    tinycodec_add_convert_test(tinycodec_convert_too_many_files
        ARGS sample.json sample.json
        EXIT_CODE 2
        STDERR_TEXT "usage: tinycodec-convert [--from FORMAT] [--to FORMAT] [--indent N] [FILE]\n")

    # Between JSON and MessagePack.
    tinycodec_add_convert_test(tinycodec_convert_json_to_msgpack
        ARGS --to msgpack sample.json
        EXIT_CODE 0
        STDOUT_FILE sample.msgpack)
    tinycodec_add_convert_test(tinycodec_convert_json_to_msgpack_ignores_indent
        ARGS --indent 4 --to msgpack sample.json
        EXIT_CODE 0
        STDOUT_FILE sample.msgpack)
    tinycodec_add_convert_test(tinycodec_convert_msgpack_to_json
        ARGS --from msgpack sample.msgpack
        EXIT_CODE 0
        STDOUT_FILE sample.indent2.json)
    tinycodec_add_convert_test(tinycodec_convert_msgpack_to_json_indent_4
        ARGS --from msgpack --indent 4 sample.msgpack
        EXIT_CODE 0
        STDOUT_FILE sample.indent4.json)
    tinycodec_add_convert_test(tinycodec_convert_msgpack_to_json_compact
        ARGS --indent 0 --from msgpack sample.msgpack
        EXIT_CODE 0
        STDOUT_FILE sample.compact.json)
    tinycodec_add_convert_test(tinycodec_convert_msgpack_to_shortest_msgpack
        ARGS --from msgpack --to msgpack sample.long.msgpack
        EXIT_CODE 0
        STDOUT_FILE sample.msgpack)
    tinycodec_add_convert_test(tinycodec_convert_msgpack_stdin
        ARGS --from msgpack --indent 0
        STDIN_FILE sample.msgpack
        EXIT_CODE 0
        STDOUT_FILE sample.compact.json)
    tinycodec_add_convert_test(tinycodec_convert_msgpack_parse_error
        ARGS --from msgpack truncated.msgpack
        EXIT_CODE 1
        STDERR_TEXT "truncated.msgpack: offset 11: UnexpectedEnd\n")
    tinycodec_add_convert_test(tinycodec_convert_msgpack_stdin_parse_error
        ARGS --from msgpack --to msgpack
        STDIN_FILE truncated.msgpack
        EXIT_CODE 1
        STDERR_TEXT "<stdin>: offset 11: UnexpectedEnd\n")
    tinycodec_add_convert_test(tinycodec_convert_bytes_to_json
        ARGS --from msgpack bytes.msgpack
        EXIT_CODE 1
        STDERR_TEXT "bytes.msgpack: offset 6: cannot be written as json\n")
    tinycodec_add_convert_test(tinycodec_convert_json_parse_error_to_msgpack
        ARGS --to msgpack invalid.json
        EXIT_CODE 1
        STDERR_TEXT "invalid.json:3:16: UnexpectedChar\n")
    tinycodec_add_convert_test(tinycodec_convert_unknown_format
        ARGS --from yaml sample.json
        EXIT_CODE 2
        STDERR_TEXT "usage: tinycodec-convert [--from FORMAT] [--to FORMAT] [--indent N] [FILE]\n")
    tinycodec_add_convert_test(tinycodec_convert_missing_from_argument
        ARGS sample.json --from
        EXIT_CODE 2
        STDERR_TEXT "usage: tinycodec-convert [--from FORMAT] [--to FORMAT] [--indent N] [FILE]\n")
    tinycodec_add_convert_test(tinycodec_convert_missing_to_argument
        ARGS sample.json --to
        EXIT_CODE 2
        STDERR_TEXT "usage: tinycodec-convert [--from FORMAT] [--to FORMAT] [--indent N] [FILE]\n")
endif()
