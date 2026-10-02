# The number code converts doubles with std::from_chars and std::to_chars.
# Not every standard library has those overloads (and on Apple platforms they
# depend on the deployment target), so check now and say so clearly instead
# of failing later in the middle of the build.
include(CheckCXXSourceCompiles)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
check_cxx_source_compiles("
    #include <charconv>
    int main() {
        const char text[] = \"1.5\";
        char buffer[32];
        double value = 0.0;
        std::from_chars(text, text + 3, value);
        return std::to_chars(buffer, buffer + sizeof(buffer), value).ptr == buffer;
    }
" TINYCODEC_HAVE_FLOATING_POINT_CHARCONV)
if(NOT TINYCODEC_HAVE_FLOATING_POINT_CHARCONV)
    message(FATAL_ERROR
        "tinycodec::json needs std::from_chars and std::to_chars for double, "
        "which this compiler and standard library do not provide. "
        "With Apple clang this requires a deployment target of macOS 26 or later.")
endif()

add_library(tinycodec_json STATIC
    src/json.cpp
    src/number_parser.cpp
    src/reader.cpp
    src/string_parser.cpp
    src/writer.cpp
)
add_library(tinycodec::json ALIAS tinycodec_json)
set_target_properties(tinycodec_json PROPERTIES EXPORT_NAME json)
target_include_directories(tinycodec_json PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(tinycodec_json PUBLIC tinycodec::core)
tinycodec_configure_library(tinycodec_json)

install(TARGETS tinycodec_json EXPORT tinycodec-json-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT tinycodec-json-targets
    NAMESPACE tinycodec::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tinycodec)

if(TINYCODEC_BUILD_TESTS)
    tinycodec_add_test(tinycodec_json_test
        test/test_conformance.cpp
        test/test_json.cpp
        test/test_number_parser.cpp
        test/test_reader.cpp
        test/test_string_parser.cpp
        test/test_writer.cpp
    )
    # Lets the tests reach the internal headers in src/.
    target_include_directories(tinycodec_json_test PRIVATE src)
    target_compile_definitions(tinycodec_json_test PRIVATE
        TINYCODEC_JSON_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/data/JSONTestSuite")
    target_link_libraries(tinycodec_json_test PRIVATE tinycodec::json)
endif()
