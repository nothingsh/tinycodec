cmake_minimum_required(VERSION 3.20)
project(tinycodec VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" TINYCODEC_IS_TOP_LEVEL)
option(TINYCODEC_BUILD_TESTS "Build the tinycodec tests" ${TINYCODEC_IS_TOP_LEVEL})
option(TINYCODEC_BUILD_TOOLS "Build the tinycodec command line tools" ${TINYCODEC_IS_TOP_LEVEL})

# Settings shared by every library target.
function(tinycodec_configure_library target)
    target_compile_features(${target} PUBLIC cxx_std_17)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-exceptions)
    endif()
endfunction()

# Creates a doctest executable from the given sources and registers it with CTest.
function(tinycodec_add_test name)
    add_executable(${name} ${PROJECT_SOURCE_DIR}/tests/doctest_main.cpp ${ARGN})
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/third_party/doctest
        ${PROJECT_SOURCE_DIR}/tests/support)
    target_compile_features(${name} PRIVATE cxx_std_17)
    set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

if(TINYCODEC_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(core)
