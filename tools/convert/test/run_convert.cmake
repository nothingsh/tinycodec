# Runs tinycodec-convert and checks the result. Variables, passed with -D:
#   CONVERT      path of the executable
#   ARGS         its arguments, separated by "|"
#   EXIT_CODE    expected exit code
#   STDIN_FILE   file to feed to standard input; empty means none
#   STDOUT_FILE  file holding the expected standard output; empty means none
#   STDERR_TEXT  expected standard error, with "\n" for line breaks

string(REPLACE "|" ";" arguments "${ARGS}")
set(input_option "")
if(NOT STDIN_FILE STREQUAL "")
    set(input_option INPUT_FILE "${STDIN_FILE}")
endif()
execute_process(
    COMMAND "${CONVERT}" ${arguments}
    ${input_option}
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE actual_stdout
    ERROR_VARIABLE actual_stderr)

set(expected_stdout "")
if(NOT STDOUT_FILE STREQUAL "")
    file(READ "${STDOUT_FILE}" expected_stdout)
endif()
string(REPLACE "\\n" "\n" expected_stderr "${STDERR_TEXT}")

if(NOT exit_code STREQUAL EXIT_CODE)
    message(FATAL_ERROR "exit code: expected [${EXIT_CODE}], got [${exit_code}]\nstderr: ${actual_stderr}")
endif()
if(NOT actual_stdout STREQUAL expected_stdout)
    message(FATAL_ERROR "stdout: expected [${expected_stdout}], got [${actual_stdout}]")
endif()
if(NOT actual_stderr STREQUAL expected_stderr)
    message(FATAL_ERROR "stderr: expected [${expected_stderr}], got [${actual_stderr}]")
endif()
