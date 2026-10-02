#include "doctest.h"

#include <string>

#include "string_parser.h"
#include "tinycodec/error.h"

using namespace tinycodec;
using namespace tinycodec::json;

namespace {

struct Result {
    ErrorCode code = ErrorCode::Ok;
    std::string text;
    size_t stop = 0;   // Offset of *stop from the start of the input.
};

// `body` is what follows the opening quote, e.g. "abc\"" for the JSON string "abc".
Result Parse(const std::string& body) {
    Result result;
    result.text = "previous content";  // ParseString must clear this.
    const char* stop = nullptr;
    result.code = ParseString(body.data(), body.data() + body.size(), &result.text, &stop);
    result.stop = static_cast<size_t>(stop - body.data());
    return result;
}

void CheckDecodes(const std::string& body, const std::string& expected) {
    CAPTURE(body);
    Result result = Parse(body);
    REQUIRE(result.code == ErrorCode::Ok);
    CHECK(result.text == expected);
    CHECK(result.stop == body.size());
}

void CheckFails(const std::string& body, ErrorCode expectedCode, size_t expectedStop) {
    CAPTURE(body);
    Result result = Parse(body);
    CHECK(result.code == expectedCode);
    CHECK(result.stop == expectedStop);
}

}  // namespace

TEST_CASE("plain text is copied up to the closing quote") {
    CheckDecodes("\"", "");
    CheckDecodes("hello\"", "hello");
    CheckDecodes("with space and / slash\"", "with space and / slash");
    CheckDecodes("\x7f\"", "\x7f");
}

TEST_CASE("parsing stops just past the closing quote") {
    Result result = Parse("ab\",\"cd\"");
    REQUIRE(result.code == ErrorCode::Ok);
    CHECK(result.text == "ab");
    CHECK(result.stop == 3);
}

TEST_CASE("simple escapes are decoded") {
    CheckDecodes("\\\"\"", "\"");
    CheckDecodes("\\\\\"", "\\");
    CheckDecodes("\\/\"", "/");
    CheckDecodes("\\b\\f\\n\\r\\t\"", "\b\f\n\r\t");
    CheckDecodes("a\\nb\"", "a\nb");
}

TEST_CASE("unicode escapes are decoded to UTF-8") {
    CheckDecodes("\\u0041\"", "A");
    CheckDecodes("\\u00e9\"", "\xC3\xA9");            // U+00E9, two bytes.
    CheckDecodes("\\u00E9\"", "\xC3\xA9");            // Upper-case hex digits.
    CheckDecodes("\\u4f60\"", "\xE4\xBD\xA0");        // U+4F60, three bytes.
    CheckDecodes("\\uFFFF\"", "\xEF\xBF\xBF");
    CheckDecodes("\\uD83D\\uDE00\"", "\xF0\x9F\x98\x80");   // U+1F600 as a surrogate pair.
    CheckDecodes("\\uDBFF\\uDFFF\"", "\xF4\x8F\xBF\xBF");   // U+10FFFF, the last code point.
    CheckDecodes("x\\u0041y\"", "xAy");
}

TEST_CASE("an escaped NUL becomes a NUL byte") {
    CheckDecodes("a\\u0000b\"", std::string("a\0b", 3));
}

TEST_CASE("well-formed UTF-8 is passed through") {
    CheckDecodes("\xC3\xA9\"", "\xC3\xA9");
    CheckDecodes("\xE4\xBD\xA0\xE5\xA5\xBD\"", "\xE4\xBD\xA0\xE5\xA5\xBD");
    CheckDecodes("\xF0\x9F\x98\x80\"", "\xF0\x9F\x98\x80");
    CheckDecodes("\xEF\xBF\xBF\"", "\xEF\xBF\xBF");           // U+FFFF.
    CheckDecodes("\xF4\x8F\xBF\xBF\"", "\xF4\x8F\xBF\xBF");   // U+10FFFF.
    CheckDecodes("\xED\x9F\xBF\"", "\xED\x9F\xBF");           // U+D7FF, just below the surrogates.
    CheckDecodes("\xEE\x80\x80\"", "\xEE\x80\x80");           // U+E000, just above them.
}

TEST_CASE("input that ends inside the string is UnexpectedEnd at the end") {
    CheckFails("", ErrorCode::UnexpectedEnd, 0);
    CheckFails("abc", ErrorCode::UnexpectedEnd, 3);
    CheckFails("abc\\", ErrorCode::UnexpectedEnd, 4);
    CheckFails("\\u12", ErrorCode::UnexpectedEnd, 4);
    CheckFails("\\uD83D", ErrorCode::UnexpectedEnd, 6);
    CheckFails("\\uD83D\\", ErrorCode::UnexpectedEnd, 7);
    CheckFails("\\uD83D\\uDE", ErrorCode::UnexpectedEnd, 10);
}

TEST_CASE("unescaped control characters are UnexpectedChar") {
    CheckFails("a\nb\"", ErrorCode::UnexpectedChar, 1);
    CheckFails("\t\"", ErrorCode::UnexpectedChar, 0);
    CheckFails(std::string("a\0b\"", 4), ErrorCode::UnexpectedChar, 1);
    CheckFails("\x1f\"", ErrorCode::UnexpectedChar, 0);
}

TEST_CASE("bad escapes are InvalidEscape at their backslash") {
    CheckFails("ab\\x\"", ErrorCode::InvalidEscape, 2);
    CheckFails("\\a\"", ErrorCode::InvalidEscape, 0);
    CheckFails("\\U0041\"", ErrorCode::InvalidEscape, 0);
    CheckFails("\\u12G4\"", ErrorCode::InvalidEscape, 0);
    CheckFails("\\u12\"", ErrorCode::InvalidEscape, 0);
    CheckFails("ok\\u 041\"", ErrorCode::InvalidEscape, 2);
}

TEST_CASE("unpaired surrogates are InvalidEscape at the first backslash") {
    CheckFails("\\uDC00\"", ErrorCode::InvalidEscape, 0);            // Low surrogate alone.
    CheckFails("\\uD800\"", ErrorCode::InvalidEscape, 0);            // High surrogate alone.
    CheckFails("\\uD800abc\"", ErrorCode::InvalidEscape, 0);
    CheckFails("\\uD800\\n\"", ErrorCode::InvalidEscape, 0);         // Followed by another escape.
    CheckFails("\\uD800\\u0041\"", ErrorCode::InvalidEscape, 0);     // Followed by a non-surrogate.
    CheckFails("\\uD800\\uD800\"", ErrorCode::InvalidEscape, 0);     // Two high surrogates.
    CheckFails("\\uDE00\\uD83D\"", ErrorCode::InvalidEscape, 0);     // Pair in the wrong order.
    CheckFails("x\\uD800\\uZZZZ\"", ErrorCode::InvalidEscape, 1);
}

TEST_CASE("malformed UTF-8 is InvalidUtf8 at its first byte") {
    CheckFails("a\x80\"", ErrorCode::InvalidUtf8, 1);                // A lone continuation byte.
    CheckFails("\xE9\"", ErrorCode::InvalidUtf8, 0);                 // Latin-1, not UTF-8.
    CheckFails("\xC3\"", ErrorCode::InvalidUtf8, 0);                 // Continuation byte missing.
    CheckFails("\xE4\xBD\"", ErrorCode::InvalidUtf8, 0);             // Truncated three-byte sequence.
    CheckFails("\xE4\xBD", ErrorCode::InvalidUtf8, 0);               // Truncated by the end of input.
    CheckFails("\xF0\x9F\x98\"", ErrorCode::InvalidUtf8, 0);         // Truncated four-byte sequence.
    CheckFails("\xC0\xAF\"", ErrorCode::InvalidUtf8, 0);             // Overlong two-byte form.
    CheckFails("\xC1\xBF\"", ErrorCode::InvalidUtf8, 0);
    CheckFails("\xE0\x80\xAF\"", ErrorCode::InvalidUtf8, 0);         // Overlong three-byte form.
    CheckFails("\xF0\x80\x80\xAF\"", ErrorCode::InvalidUtf8, 0);     // Overlong four-byte form.
    CheckFails("\xED\xA0\x80\"", ErrorCode::InvalidUtf8, 0);         // U+D800, a surrogate.
    CheckFails("\xF4\x90\x80\x80\"", ErrorCode::InvalidUtf8, 0);     // Above U+10FFFF.
    CheckFails("\xF5\x80\x80\x80\"", ErrorCode::InvalidUtf8, 0);
    CheckFails("\xFF\"", ErrorCode::InvalidUtf8, 0);
    CheckFails("\xFC\x83\xBF\xBF\xBF\xBF\"", ErrorCode::InvalidUtf8, 0);   // Obsolete six-byte form.
}
