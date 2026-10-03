#include "doctest.h"

#include <string>

#include "tinycodec/utf8.h"

using namespace tinycodec;

namespace {

size_t SequenceLength(const std::string& bytes) {
    return Utf8SequenceLength(bytes.data(), bytes.data() + bytes.size());
}

}  // namespace

TEST_CASE("Utf8SequenceLength accepts ASCII, including NUL, as one byte") {
    CHECK(SequenceLength("a") == 1);
    CHECK(SequenceLength(std::string(1, '\0')) == 1);
    CHECK(SequenceLength("\x7F") == 1);
    CHECK(SequenceLength("ab") == 1);   // Only the first sequence counts.
}

TEST_CASE("Utf8SequenceLength accepts the smallest and largest sequence of each length") {
    CHECK(SequenceLength("\xC2\x80") == 2);               // U+0080
    CHECK(SequenceLength("\xDF\xBF") == 2);               // U+07FF
    CHECK(SequenceLength("\xE0\xA0\x80") == 3);           // U+0800
    CHECK(SequenceLength("\xED\x9F\xBF") == 3);           // U+D7FF, just below the surrogates
    CHECK(SequenceLength("\xEE\x80\x80") == 3);           // U+E000, just above them
    CHECK(SequenceLength("\xEF\xBF\xBF") == 3);           // U+FFFF
    CHECK(SequenceLength("\xF0\x90\x80\x80") == 4);       // U+10000
    CHECK(SequenceLength("\xF4\x8F\xBF\xBF") == 4);       // U+10FFFF
}

TEST_CASE("Utf8SequenceLength rejects malformed sequences") {
    CHECK(SequenceLength("\x80") == 0);                   // A lone continuation byte.
    CHECK(SequenceLength("\xBF") == 0);
    CHECK(SequenceLength("\xC0\x80") == 0);               // Overlong two-byte forms.
    CHECK(SequenceLength("\xC1\xBF") == 0);
    CHECK(SequenceLength("\xE0\x9F\xBF") == 0);           // Overlong three-byte form.
    CHECK(SequenceLength("\xF0\x8F\xBF\xBF") == 0);       // Overlong four-byte form.
    CHECK(SequenceLength("\xED\xA0\x80") == 0);           // U+D800
    CHECK(SequenceLength("\xED\xBF\xBF") == 0);           // U+DFFF
    CHECK(SequenceLength("\xF4\x90\x80\x80") == 0);       // U+110000
    CHECK(SequenceLength("\xF5\x80\x80\x80") == 0);
    CHECK(SequenceLength("\xFF") == 0);
    CHECK(SequenceLength("\xC3\x41") == 0);               // Second byte is not a continuation.
    CHECK(SequenceLength("\xE4\xBD\x41") == 0);           // Third byte is not a continuation.
}

TEST_CASE("Utf8SequenceLength rejects a sequence cut short by the end") {
    CHECK(SequenceLength("\xC3") == 0);
    CHECK(SequenceLength("\xE4\xBD") == 0);
    CHECK(SequenceLength("\xF0\x9F\x98") == 0);
}

TEST_CASE("ValidUtf8Prefix covers the whole text when it is well-formed") {
    CHECK(ValidUtf8Prefix("") == 0);
    CHECK(ValidUtf8Prefix("plain") == 5);
    CHECK(ValidUtf8Prefix("caf\xC3\xA9 \xE4\xBD\xA0 \xF0\x9F\x98\x80") == 14);
    CHECK(ValidUtf8Prefix(std::string("a\0b", 3)) == 3);
}

TEST_CASE("ValidUtf8Prefix stops at the first byte of the first malformed sequence") {
    CHECK(ValidUtf8Prefix("\x80") == 0);
    CHECK(ValidUtf8Prefix("ab\x80" "cd") == 2);
    CHECK(ValidUtf8Prefix("\xC3\xA9\xED\xA0\x80") == 2);   // A surrogate after U+00E9.
    CHECK(ValidUtf8Prefix("abc\xE4\xBD") == 3);              // Truncated at the end.
    CHECK(ValidUtf8Prefix("a\xF4\x90\x80\x80" "b") == 1);
}
