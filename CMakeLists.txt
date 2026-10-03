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
add_subdirectory(formats/json)
add_subdirectory(formats/msgpack)
add_subdirectory(formats/toml)

if(TINYCODEC_BUILD_TOOLS)
    add_subdirectory(tools/convert)
endif()

if(TINYCODEC_BUILD_TESTS)
    add_subdirectory(tests/cross_format)
endif()

# Package files, so that an installed tinycodec can be found with
#   find_package(tinycodec REQUIRED COMPONENTS json)
include(CMakePackageConfigHelpers)
set(TINYCODEC_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/tinycodec)
configure_package_config_file(
    cmake/tinycodecConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/tinycodecConfig.cmake
    INSTALL_DESTINATION ${TINYCODEC_INSTALL_CMAKEDIR})
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/tinycodecConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/tinycodecConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/tinycodecConfigVersion.cmake
    DESTINATION ${TINYCODEC_INSTALL_CMAKEDIR})

if(TINYCODEC_BUILD_TESTS)
    # Installs this build into a scratch directory and builds tests/consumer
    # against it, the way an outside project would use the library.
    add_test(NAME tinycodec_consumer
        COMMAND ${CMAKE_COMMAND}
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/consumer
            -DWORK_DIR=${CMAKE_BINARY_DIR}/consumer-test
            -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
            "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
            "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/consumer/run_consumer.cmake)
endif()
