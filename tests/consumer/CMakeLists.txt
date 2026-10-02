# A stand-alone project that uses an installed tinycodec, the way an outside
# project would. It is configured and built by run_consumer.cmake.
cmake_minimum_required(VERSION 3.20)
project(tinycodec_consumer LANGUAGES CXX)

# Asking for a format that does not exist must fail quietly, not abort.
find_package(tinycodec QUIET COMPONENTS nosuchformat)
if(tinycodec_FOUND)
    message(FATAL_ERROR "find_package succeeded for a component that does not exist")
endif()

find_package(tinycodec 0.1 REQUIRED COMPONENTS json)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE tinycodec::json)
