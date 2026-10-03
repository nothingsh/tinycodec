#include "doctest.h"

#include <string>
#include <string_view>

#include "string_parser.h"
#include "tinycodec/error.h"

using namespace tinycodec;
using namespace tinycodec::toml;

namespace {

struct Result {
    ErrorCode code = ErrorCode::Ok;
    std::string text;
    bool multiline = false;
    bool borrowed = false;   // The content is a view of the input.
    size_t stop = 0;         // Offset of *stop from the start of the input.
};

// `input` starts with the opening delimiter, e.g. "\"abc\"".
Result Parse(const std::string& input) {
    Result result;
    std::string scratch = "previous content";
    std::string_view out;
    const char* stop = nullptr;
    const char* begin = input.data();
    const char* end = begin + input.size();
    result.code = ParseString(begin, end, &scratch, &out, &result.multiline, &stop);
    result.stop = static_cast<size_t>(stop - begin);
    if (result.code == ErrorCode::Ok) {
        result.text = std::string(out);
        result.borrowed = out.data() >= begin && out.data() + out.size() <= end;
    }
    return result;
}

void CheckDecodes(const std::string& input, const std::string& expected) {
    CAPTURE(input);
    Result result = Parse(input);
    REQUIRE(result.code == ErrorCode::Ok);
    CHECK(result.text == expected);
    CHECK(result.stop == input.size());
}

void CheckFails(const std::string& input, ErrorCode expectedCode, size_t expectedStop) {
    CAPTURE(input);
    Result result = Parse(input);
    CHECK(result.code == expectedCode);
    CHECK(result.stop == expectedStop);
}

}  // namespace

TEST_CASE("each kind of string reads up to its closing delimiter") {
    CheckDecodes("\"basic\"", "basic");
    CheckDecodes("'literal'", "literal");
    CheckDecodes("\"\"\"multi\nline\"\"\"", "multi\nline");
    CheckDecodes("'''multi\nline'''", "multi\nline");
    CheckDecodes("\"\"", "");
    CheckDecodes("''", "");
    CheckDecodes("\"\"\"\"\"\"", "");
    CheckDecodes("''''''", "");
}

TEST_CASE("multiline tells the two forms apart") {
    CHECK_FALSE(Parse("\"a\"").multiline);
    CHECK_FALSE(Parse("'a'").multiline);
    CHECK(Parse("\"\"\"a\"\"\"").multiline);
    CHECK(Parse("'''a'''").multiline);
}

TEST_CASE("parsing stops just past the closing delimiter") {
    Result result = Parse("\"ab\" = 1");
    REQUIRE(result.code == ErrorCode::Ok);
    CHECK(result.text == "ab");
    CHECK(result.stop == 4);

    result = Parse("'''a'''b");
    REQUIRE(result.code == ErrorCode::Ok);
    CHECK(result.text == "a");
    CHECK(result.stop == 7);
}

TEST_CASE("content without escapes is a view of the input, decoded content is not") {
    CHECK(Parse("\"plain\"").borrowed);
    CHECK(Parse("'C:\\path'").borrowed);
    CHECK(Parse("\"\"\"a\nb\"\"\"").borrowed);
    CHECK_FALSE(Parse("\"a\\tb\"").borrowed);
    CHECK(Parse("\"a\\tb\"").text == "a\tb");
}

TEST_CASE("basic strings decode the TOML 1.0 escapes") {
    CheckDecodes("\"\\b\\t\\n\\f\\r\\\"\\\\\"", "\b\t\n\f\r\"\\");
    CheckDecodes("\"\\u0041\\u00e9\\u4F60\"", "A\xC3\xA9\xE4\xBD\xA0");
    CheckDecodes("\"\\U0001F600\"", "\xF0\x9F\x98\x80");
    CheckDecodes("\"\\U0010FFFF\\u0000\"", std::string("\xF4\x8F\xBF\xBF\0", 5));
    CheckDecodes("\"\"\"\\u0041\\\"\"\"\"", "A\"");
}

TEST_CASE("literal strings have no escapes") {
    CheckDecodes("'\\n\\u0041'", "\\n\\u0041");
    CheckDecodes("'''\\'''", "\\");
}

TEST_CASE("unknown escapes and bad code points are InvalidEscape at the backslash") {
    CheckFails("\"a\\x41\"", ErrorCode::InvalidEscape, 2);
    CheckFails("\"\\e\"", ErrorCode::InvalidEscape, 1);
    CheckFails("\"\\ \"", ErrorCode::InvalidEscape, 1);
    CheckFails("\"\\u12\"", ErrorCode::InvalidEscape, 1);
    CheckFails("\"\\u12G4\"", ErrorCode::InvalidEscape, 1);
    CheckFails("\"\\uD800\"", ErrorCode::InvalidEscape, 1);
    CheckFails("\"\\uDFFF\"", ErrorCode::InvalidEscape, 1);
    CheckFails("\"\\U00110000\"", ErrorCode::InvalidEscape, 1);
    CheckFails("\"\\UFFFFFFFF\"", ErrorCode::InvalidEscape, 1);
}

TEST_CASE("the input ending inside a string or escape is UnexpectedEnd at the end") {
    CheckFails("\"abc", ErrorCode::UnexpectedEnd, 4);
    CheckFails("'abc", ErrorCode::UnexpectedEnd, 4);
    CheckFails("\"\"\"abc\"\"", ErrorCode::UnexpectedEnd, 8);
    CheckFails("'''abc''", ErrorCode::UnexpectedEnd, 8);
    CheckFails("\"\\", ErrorCode::UnexpectedEnd, 2);
    CheckFails("\"\\u00", ErrorCode::UnexpectedEnd, 5);
    CheckFails("\"\"\"a\\  ", ErrorCode::UnexpectedEnd, 7);
}

TEST_CASE("a line break ends a single-line string with UnexpectedChar") {
    CheckFails("\"ab\ncd\"", ErrorCode::UnexpectedChar, 3);
    CheckFails("'ab\ncd'", ErrorCode::UnexpectedChar, 3);
    CheckFails("\"ab\r\ncd\"", ErrorCode::UnexpectedChar, 3);
}

TEST_CASE("control characters other than tab are UnexpectedChar") {
    CheckDecodes("\"a\tb\"", "a\tb");
    CheckDecodes("'a\tb'", "a\tb");
    CheckFails(std::string("\"a\0b\"", 5), ErrorCode::UnexpectedChar, 2);
    CheckFails("\"a\x1F\"", ErrorCode::UnexpectedChar, 2);
    CheckFails("'a\x7F'", ErrorCode::UnexpectedChar, 2);
    CheckFails("\"\"\"a\x08\"\"\"", ErrorCode::UnexpectedChar, 4);
    CheckFails("'''a\x7F'''", ErrorCode::UnexpectedChar, 4);
}

TEST_CASE("multi-line strings accept LF and CRLF but not a lone CR") {
    CheckDecodes("\"\"\"a\r\nb\"\"\"", "a\r\nb");
    CheckDecodes("'''a\r\nb'''", "a\r\nb");
    CheckFails("\"\"\"a\rb\"\"\"", ErrorCode::UnexpectedChar, 4);
    CheckFails("'''a\r'''", ErrorCode::UnexpectedChar, 4);
}

TEST_CASE("a line break right after the opening delimiter is dropped") {
    CheckDecodes("\"\"\"\nabc\"\"\"", "abc");
    CheckDecodes("'''\r\nabc'''", "abc");
    CheckDecodes("\"\"\"\n\nabc\"\"\"", "\nabc");
    CheckDecodes("'''\n'''", "");
}

TEST_CASE("one or two quotes are content, also right before the closing delimiter") {
    CheckDecodes("\"\"\"a\"b\"\"c\"\"\"", "a\"b\"\"c");
    CheckDecodes("\"\"\"a\"\"\"\"", "a\"");
    CheckDecodes("\"\"\"a\"\"\"\"\"", "a\"\"");
    CheckDecodes("'''a''''", "a'");
    CheckDecodes("'''a'''''", "a''");
    CheckDecodes("\"\"\"\"\"\"\"", "\"");
}

TEST_CASE("six quotes in a row in a multi-line string are UnexpectedChar at the sixth") {
    CheckFails("\"\"\"a\"\"\"\"\"\"", ErrorCode::UnexpectedChar, 9);
    CheckFails("'''a''''''", ErrorCode::UnexpectedChar, 9);
}

TEST_CASE("a line-ending backslash removes the line break and the whitespace after it") {
    CheckDecodes("\"\"\"a \\\n   b\"\"\"", "a b");
    CheckDecodes("\"\"\"a\\  \t\r\n\n  \t b\"\"\"", "ab");
    CheckDecodes("\"\"\"a\\\n\"\"\"", "a");
    CheckFails("\"\"\"a\\  b\"\"\"", ErrorCode::InvalidEscape, 4);
    CheckFails("\"\"\"a\\\n\rb\"\"\"", ErrorCode::UnexpectedChar, 6);
    // Only in basic multi-line strings: in a single-line string it is an
    // unknown escape, in a literal string just a backslash.
    CheckFails("\"a\\\nb\"", ErrorCode::InvalidEscape, 2);
    CheckDecodes("'''a\\\nb'''", "a\\\nb");
}
