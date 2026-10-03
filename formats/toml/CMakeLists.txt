# Floating point numbers are converted with std::from_chars and
# std::to_chars; see formats/json/CMakeLists.txt for why this is checked here.
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
        "tinycodec::toml needs std::from_chars and std::to_chars for double, "
        "which this compiler and standard library do not provide. "
        "With Apple clang this requires a deployment target of macOS 26 or later.")
endif()

add_library(tinycodec_toml STATIC
    src/datetime_parser.cpp
    src/number_parser.cpp
    src/reader.cpp
    src/string_parser.cpp
    src/toml.cpp
    src/writer.cpp
)
add_library(tinycodec::toml ALIAS tinycodec_toml)
set_target_properties(tinycodec_toml PROPERTIES EXPORT_NAME toml)
target_include_directories(tinycodec_toml PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(tinycodec_toml PUBLIC tinycodec::core)
tinycodec_configure_library(tinycodec_toml)

install(TARGETS tinycodec_toml EXPORT tinycodec-toml-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT tinycodec-toml-targets
    NAMESPACE tinycodec::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tinycodec)

if(TINYCODEC_BUILD_TESTS)
    tinycodec_add_test(tinycodec_toml_test
        test/test_datetime_parser.cpp
        test/test_number_parser.cpp
        test/test_reader.cpp
        test/test_string_parser.cpp
        test/test_tables.cpp
        test/test_toml.cpp
        test/test_writer.cpp
    )
    # Lets the tests reach the internal headers in src/.
    target_include_directories(tinycodec_toml_test PRIVATE src)
    target_link_libraries(tinycodec_toml_test PRIVATE tinycodec::toml)
endif()
