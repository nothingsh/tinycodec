#include "doctest.h"

#include <string>
#include <string_view>
#include <vector>

#include "recording_visitor.h"
#include "tinycodec/error.h"
#include "tinycodec/json/reader.h"

using namespace tinycodec;

namespace {

using Events = std::vector<std::string>;

Events ParseEvents(std::string_view text) {
    RecordingVisitor visitor;
    json::Reader reader;
    Error error = reader.Parse(text, visitor);
    CAPTURE(text);
    REQUIRE(error.Ok());
    return visitor.events;
}

void CheckError(std::string_view text, ErrorCode code, size_t offset, int line, int column) {
    CAPTURE(text);
    RecordingVisitor visitor;
    json::Reader reader;
    Error error = reader.Parse(text, visitor);
    CHECK(ErrorName(error.code) == std::string(ErrorName(code)));
    CHECK(error.offset == offset);
    CHECK(error.line == line);
    CHECK(error.column == column);
}

std::string Nested(int depth) {
    return std::string(depth, '[') + std::string(depth, ']');
}

}  // namespace

TEST_CASE("any JSON value can be the root") {
    CHECK(ParseEvents("null") == Events{"Null"});
    CHECK(ParseEvents("true") == Events{"Bool(true)"});
    CHECK(ParseEvents("false") == Events{"Bool(false)"});
    CHECK(ParseEvents("42") == Events{"Int(42)"});
    CHECK(ParseEvents("-1.5") == Events{"Double(-1.500000)"});
    CHECK(ParseEvents("18446744073709551615") == Events{"Uint(18446744073709551615)"});
    CHECK(ParseEvents("\"hi\"") == Events{"String(hi)"});
    CHECK(ParseEvents("[]") == Events{"EnterArray", "ExitArray"});
    CHECK(ParseEvents("{}") == Events{"EnterObject", "ExitObject"});
}

TEST_CASE("a successful parse returns an Error that is Ok") {
    RecordingVisitor visitor;
    json::Reader reader;
    Error error = reader.Parse("[1]", visitor);
    CHECK(error.Ok());
    CHECK(error.offset == 0);
    CHECK(error.line == 0);
    CHECK(error.column == 0);
}

TEST_CASE("arrays and objects produce events in document order") {
    CHECK(ParseEvents("[1,\"a\",null]") == Events{"EnterArray", "Int(1)", "String(a)", "Null", "ExitArray"});
    CHECK(ParseEvents("{\"a\":1,\"b\":[true,{}]}") == Events{
        "EnterObject",
        "Key(a)", "Int(1)",
        "Key(b)", "EnterArray", "Bool(true)", "EnterObject", "ExitObject", "ExitArray",
        "ExitObject"});
}

TEST_CASE("whitespace is allowed around every token") {
    CHECK(ParseEvents(" \t\r\n[ 1 , 2 ]\n") == Events{"EnterArray", "Int(1)", "Int(2)", "ExitArray"});
    CHECK(ParseEvents("{ \"a\" : 1 , \"b\" : 2 }") == Events{
        "EnterObject", "Key(a)", "Int(1)", "Key(b)", "Int(2)", "ExitObject"});
    CHECK(ParseEvents("[ ]") == Events{"EnterArray", "ExitArray"});
    CHECK(ParseEvents("{\n}") == Events{"EnterObject", "ExitObject"});
}

TEST_CASE("strings and keys are decoded before they are reported") {
    CHECK(ParseEvents("{\"k\\n\":\"\\u0041\\\"\"}") == Events{
        "EnterObject", "Key(k\n)", "String(A\")", "ExitObject"});
    CHECK(ParseEvents("{\"\":\"\"}") == Events{"EnterObject", "Key()", "String()", "ExitObject"});
}

TEST_CASE("repeated keys are reported as they appear") {
    CHECK(ParseEvents("{\"a\":1,\"a\":2}") == Events{
        "EnterObject", "Key(a)", "Int(1)", "Key(a)", "Int(2)", "ExitObject"});
}

TEST_CASE("input that ends too early is UnexpectedEnd at the end") {
    CheckError("", ErrorCode::UnexpectedEnd, 0, 1, 1);
    CheckError("   ", ErrorCode::UnexpectedEnd, 3, 1, 4);
    CheckError("[", ErrorCode::UnexpectedEnd, 1, 1, 2);
    CheckError("[1", ErrorCode::UnexpectedEnd, 2, 1, 3);
    CheckError("[1,", ErrorCode::UnexpectedEnd, 3, 1, 4);
    CheckError("{", ErrorCode::UnexpectedEnd, 1, 1, 2);
    CheckError("{\"a\"", ErrorCode::UnexpectedEnd, 4, 1, 5);
    CheckError("{\"a\":", ErrorCode::UnexpectedEnd, 5, 1, 6);
    CheckError("{\"a\":1", ErrorCode::UnexpectedEnd, 6, 1, 7);
    CheckError("{\"a\":1,", ErrorCode::UnexpectedEnd, 7, 1, 8);
    CheckError("tru", ErrorCode::UnexpectedEnd, 3, 1, 4);
    CheckError("\"abc", ErrorCode::UnexpectedEnd, 4, 1, 5);
}

TEST_CASE("a character that is not allowed is UnexpectedChar at that character") {
    CheckError("x", ErrorCode::UnexpectedChar, 0, 1, 1);
    CheckError("nul!", ErrorCode::UnexpectedChar, 3, 1, 4);
    CheckError("True", ErrorCode::UnexpectedChar, 0, 1, 1);
    CheckError("[1 2]", ErrorCode::UnexpectedChar, 3, 1, 4);
    CheckError("[1,]", ErrorCode::UnexpectedChar, 3, 1, 4);
    CheckError("[,1]", ErrorCode::UnexpectedChar, 1, 1, 2);
    CheckError("{\"a\" 1}", ErrorCode::UnexpectedChar, 5, 1, 6);
    CheckError("{\"a\":1,}", ErrorCode::UnexpectedChar, 7, 1, 8);
    CheckError("{a:1}", ErrorCode::UnexpectedChar, 1, 1, 2);
    CheckError("{1:1}", ErrorCode::UnexpectedChar, 1, 1, 2);
    CheckError("{\"a\":1 \"b\":2}", ErrorCode::UnexpectedChar, 7, 1, 8);
    CheckError("'a'", ErrorCode::UnexpectedChar, 0, 1, 1);
    CheckError("[1] // note", ErrorCode::UnexpectedChar, 4, 1, 5);
    CheckError("\"a\tb\"", ErrorCode::UnexpectedChar, 2, 1, 3);
}

TEST_CASE("content after the root is UnexpectedChar") {
    CheckError("1 2", ErrorCode::UnexpectedChar, 2, 1, 3);
    CheckError("{}{}", ErrorCode::UnexpectedChar, 2, 1, 3);
    CheckError("[]]", ErrorCode::UnexpectedChar, 2, 1, 3);
    CheckError("01", ErrorCode::UnexpectedChar, 1, 1, 2);
    CheckError("null\nx", ErrorCode::UnexpectedChar, 5, 2, 1);
}

TEST_CASE("a byte order mark is not skipped") {
    CheckError("\xEF\xBB\xBF{}", ErrorCode::UnexpectedChar, 0, 1, 1);
}

TEST_CASE("number, escape and UTF-8 errors carry the position from their parsers") {
    CheckError("[1.]", ErrorCode::InvalidNumber, 3, 1, 4);
    CheckError("-", ErrorCode::InvalidNumber, 1, 1, 2);
    CheckError("[ 1e999]", ErrorCode::InvalidNumber, 2, 1, 3);
    CheckError("[\"a\\x\"]", ErrorCode::InvalidEscape, 3, 1, 4);
    CheckError("{\"\\uD800\":1}", ErrorCode::InvalidEscape, 2, 1, 3);
    CheckError("[\"\xFF\"]", ErrorCode::InvalidUtf8, 2, 1, 3);
}

TEST_CASE("line and column count from 1 and follow line feeds") {
    CheckError("[\n  1,\n  ?\n]", ErrorCode::UnexpectedChar, 9, 3, 3);
    CheckError("[\r\n1,\r\n?]", ErrorCode::UnexpectedChar, 7, 3, 1);
    CheckError("\n\n\n", ErrorCode::UnexpectedEnd, 3, 4, 1);
    // Columns are counted in bytes: the three-byte character counts as three.
    CheckError("[\"\xE4\xBD\xA0\" ?]", ErrorCode::UnexpectedChar, 7, 1, 8);
}

TEST_CASE("nesting is limited to kMaxDepth containers") {
    RecordingVisitor atLimit;
    json::Reader reader;
    CHECK(reader.Parse(Nested(json::kMaxDepth), atLimit).Ok());
    CHECK(atLimit.events.size() == 2 * static_cast<size_t>(json::kMaxDepth));

    // The error is at the bracket that goes one level too deep.
    CheckError(Nested(json::kMaxDepth + 1), ErrorCode::DepthExceeded, 500, 1, 501);

    // Objects count towards the same limit.
    std::string mixed;
    for (int i = 0; i < 250; ++i) {
        mixed += "[{\"k\":";
    }
    CheckError(mixed + "[", ErrorCode::DepthExceeded, mixed.size(), 1, static_cast<int>(mixed.size()) + 1);
}

TEST_CASE("very deep input fails cleanly instead of overflowing the stack") {
    std::string deep(100000, '[');
    CheckError(deep, ErrorCode::DepthExceeded, 500, 1, 501);
}

TEST_CASE("when the visitor rejects an event, parsing stops with Aborted") {
    const std::string text = "{\"a\":[1,true,\"s\"],\"b\":{}}";
    Events all = ParseEvents(text);
    REQUIRE(all.size() == 11);

    for (int failAt = 0; failAt < static_cast<int>(all.size()); ++failAt) {
        CAPTURE(failAt);
        RecordingVisitor visitor;
        visitor.failAt = failAt;
        json::Reader reader;
        Error error = reader.Parse(text, visitor);
        CHECK(error.code == ErrorCode::Aborted);
        // The rejected event is the last one delivered.
        CHECK(static_cast<int>(visitor.events.size()) == failAt + 1);
    }
}

TEST_CASE("Aborted is positioned at the token whose event was rejected") {
    //                        0123456 789012
    const std::string text = "{\"k\":\n[12,\"s\"]}";
    struct Case {
        int failAt;
        size_t offset;
        int line;
        int column;
    };
    const Case cases[] = {
        {0, 0, 1, 1},    // EnterObject at '{'
        {1, 1, 1, 2},    // Key at its opening quote
        {2, 6, 2, 1},    // EnterArray at '['
        {3, 7, 2, 2},    // Int at its first digit
        {4, 10, 2, 5},   // String at its opening quote
        {5, 13, 2, 8},   // ExitArray at ']'
        {6, 14, 2, 9},   // ExitObject at '}'
    };
    for (const Case& expected : cases) {
        CAPTURE(expected.failAt);
        RecordingVisitor visitor;
        visitor.failAt = expected.failAt;
        json::Reader reader;
        Error error = reader.Parse(text, visitor);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == expected.offset);
        CHECK(error.line == expected.line);
        CHECK(error.column == expected.column);
    }
}

TEST_CASE("a Reader can be reused, also after a failed parse") {
    json::Reader reader;

    RecordingVisitor first;
    CHECK(reader.Parse("[\"long string to fill the scratch buffer\"]", first).Ok());

    RecordingVisitor failed;
    Error error = reader.Parse("[1,", failed);
    CHECK(error.code == ErrorCode::UnexpectedEnd);

    RecordingVisitor second;
    error = reader.Parse("\"x\"", second);
    CHECK(error.Ok());
    CHECK(error.offset == 0);
    CHECK(second.events == Events{"String(x)"});
}

TEST_CASE("the input does not have to be NUL-terminated") {
    // Only the first three characters belong to the input.
    const char buffer[] = {'[', '1', ']', '2', '3'};
    CHECK(ParseEvents(std::string_view(buffer, 3)) == Events{"EnterArray", "Int(1)", "ExitArray"});

    const char number[] = {'1', '2', '3', '4'};
    CHECK(ParseEvents(std::string_view(number, 2)) == Events{"Int(12)"});
}
