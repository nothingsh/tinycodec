#include "doctest.h"

#include <cstdio>
#include <string>

#include "tinycodec/sink.h"

using namespace tinycodec;

TEST_CASE("StringSink appends everything it is given") {
    StringSink sink;
    CHECK(sink.Str().empty());
    CHECK(sink.Write("abc"));
    CHECK(sink.Write(""));
    CHECK(sink.Write(std::string_view("d\0e", 3)));
    CHECK(sink.Str() == std::string("abcd\0e", 6));
}

TEST_CASE("FileSink writes to the stream and leaves it open") {
    FILE* file = std::tmpfile();
    REQUIRE(file != nullptr);
    {
        FileSink sink(file);
        CHECK(sink.Write("hello "));
        CHECK(sink.Write(""));
        CHECK(sink.Write("world"));
    }

    std::rewind(file);
    char buffer[32] = {};
    size_t count = std::fread(buffer, 1, sizeof(buffer) - 1, file);
    CHECK(std::string(buffer, count) == "hello world");
    std::fclose(file);
}

TEST_CASE("FileSink reports a failed write") {
    FILE* file = std::tmpfile();
    REQUIRE(file != nullptr);
    FILE* readOnly = std::freopen(nullptr, "r", file);
    if (readOnly == nullptr) {
        return;  // The platform cannot reopen a temporary file; nothing to test.
    }
    FileSink sink(readOnly);
    CHECK_FALSE(sink.Write("x"));
    std::fclose(readOnly);
}
