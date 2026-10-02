add_library(tinycodec_core STATIC
    src/arena.cpp
    src/error.cpp
)
add_library(tinycodec::core ALIAS tinycodec_core)
set_target_properties(tinycodec_core PROPERTIES EXPORT_NAME core)
target_include_directories(tinycodec_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
tinycodec_configure_library(tinycodec_core)

if(TINYCODEC_BUILD_TESTS)
    tinycodec_add_test(tinycodec_core_test
        test/test_arena.cpp
        test/test_error.cpp
    )
    target_link_libraries(tinycodec_core_test PRIVATE tinycodec::core)
endif()
