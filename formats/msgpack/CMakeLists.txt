add_library(tinycodec_msgpack STATIC
    src/msgpack.cpp
    src/reader.cpp
    src/writer.cpp
)
add_library(tinycodec::msgpack ALIAS tinycodec_msgpack)
set_target_properties(tinycodec_msgpack PROPERTIES EXPORT_NAME msgpack)
target_include_directories(tinycodec_msgpack PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(tinycodec_msgpack PUBLIC tinycodec::core)
tinycodec_configure_library(tinycodec_msgpack)

install(TARGETS tinycodec_msgpack EXPORT tinycodec-msgpack-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT tinycodec-msgpack-targets
    NAMESPACE tinycodec::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tinycodec)

if(TINYCODEC_BUILD_TESTS)
    tinycodec_add_test(tinycodec_msgpack_test
        test/test_msgpack.cpp
        test/test_reader.cpp
        test/test_writer.cpp
    )
    target_link_libraries(tinycodec_msgpack_test PRIVATE tinycodec::msgpack)
endif()
