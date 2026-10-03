# Tests that need more than one format. The formats cannot see each other,
# so these tests live outside formats/.
tinycodec_add_test(tinycodec_cross_format_test
    test_json_msgpack.cpp
    test_msgpack_suite.cpp
    test_toml_conversions.cpp
    test_toml_suite.cpp
)
target_compile_definitions(tinycodec_cross_format_test PRIVATE
    TINYCODEC_JSON_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/formats/json/test/data/JSONTestSuite"
    TINYCODEC_MSGPACK_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/formats/msgpack/test/data/msgpack-test-suite"
    TINYCODEC_TOML_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/formats/toml/test/data/toml-test")
target_link_libraries(tinycodec_cross_format_test PRIVATE tinycodec::json tinycodec::msgpack tinycodec::toml)
