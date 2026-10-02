add_library(tinycodec_core STATIC
    src/arena.cpp
    src/builder.cpp
    src/document.cpp
    src/error.cpp
    src/sink.cpp
    src/value.cpp
)
add_library(tinycodec::core ALIAS tinycodec_core)
set_target_properties(tinycodec_core PROPERTIES EXPORT_NAME core)
target_include_directories(tinycodec_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
tinycodec_configure_library(tinycodec_core)

install(TARGETS tinycodec_core EXPORT tinycodec-core-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT tinycodec-core-targets
    NAMESPACE tinycodec::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tinycodec)

if(TINYCODEC_BUILD_TESTS)
    tinycodec_add_test(tinycodec_core_test
        test/test_accept.cpp
        test/test_arena.cpp
        test/test_builder.cpp
        test/test_error.cpp
        test/test_sink.cpp
        test/test_value.cpp
    )
    target_link_libraries(tinycodec_core_test PRIVATE tinycodec::core)
endif()
