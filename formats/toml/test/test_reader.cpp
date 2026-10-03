#include "doctest.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "recording_visitor.h"
#include "tinycodec/error.h"
#include "tinycodec/toml/reader.h"

using namespace tinycodec;

namespace {

using Events = std::vector<std::string>;

// Parses text, which must be valid, and returns the events.
Events EventsOf(std::string_view text) {
    RecordingVisitor visitor;
    toml::Reader reader;
    Error error = reader.Parse(text, visitor);
    INFO("error: ", ErrorName(error.code), " at offset ", error.offset);
    REQUIRE(error.Ok());
    return visitor.events;
}

// The events of the value in "v = <value>".
Events ValueEvents(const std::string& value) {
    Events events = EventsOf("v = " + value);
    REQUIRE(events.size() >= 4);
    REQUIRE(events[1] == "Key(v)");
    return Events(events.begin() + 2, events.end() - 1);
}

Error ErrorOf(std::string_view text) {
    RecordingVisitor visitor;
    toml::Reader reader;
    Error error = reader.Parse(text, visitor);
    // Invalid text produces no events at all.
    CHECK(visitor.events.empty());
    return error;
}

void CheckError(std::string_view text, ErrorCode code, size_t offset) {
    CAPTURE(text);
    Error error = ErrorOf(text);
    CHECK(ErrorName(error.code) == std::string(ErrorName(code)));
    CHECK(error.offset == offset);
}

}  // namespace

TEST_CASE("an empty document is an empty root table") {
    const Events EMPTY = {"EnterObject", "ExitObject"};
    CHECK(EventsOf("") == EMPTY);
    CHECK(EventsOf(" \t\n\r\n") == EMPTY);
    CHECK(EventsOf("# just a comment") == EMPTY);
    CHECK(EventsOf("\n# one\n  # two\n\n") == EMPTY);
}

TEST_CASE("key/value lines become members of the root table, in order") {
    CHECK(EventsOf("b = 1\na = \"x\"\n") == Events{
        "EnterObject", "Key(b)", "Int(1)", "Key(a)", "String(x)", "ExitObject"});
}

TEST_CASE("spaces around the parts of a line, comments and the last line break are optional") {
    CHECK(EventsOf("\t a\t=\t1 \t# comment\nb=2") == Events{
        "EnterObject", "Key(a)", "Int(1)", "Key(b)", "Int(2)", "ExitObject"});
}

TEST_CASE("lines may end with CRLF") {
    CHECK(EventsOf("a = 1\r\nb = 2 # c\r\n\r\n") == Events{
        "EnterObject", "Key(a)", "Int(1)", "Key(b)", "Int(2)", "ExitObject"});
}

TEST_CASE("each kind of scalar becomes its event") {
    CHECK(ValueEvents("\"basic\"") == Events{"String(basic)"});
    CHECK(ValueEvents("'literal'") == Events{"String(literal)"});
    CHECK(ValueEvents("\"\"\"\nmulti\nline\"\"\"") == Events{"String(multi\nline)"});
    CHECK(ValueEvents("-17") == Events{"Int(-17)"});
    CHECK(ValueEvents("0xff") == Events{"Int(255)"});
    CHECK(ValueEvents("1.5") == Events{"Double(1.500000)"});
    CHECK(ValueEvents("-inf") == Events{"Double(-inf)"});
    CHECK(ValueEvents("true") == Events{"Bool(true)"});
    CHECK(ValueEvents("false") == Events{"Bool(false)"});
    CHECK(ValueEvents("1979-05-27T07:32:00-08:00") == Events{"DateTime(1979-05-27T07:32:00-08:00)"});
    CHECK(ValueEvents("1979-05-27 07:32:00.5") == Events{"DateTime(1979-05-27T07:32:00.5)"});
    CHECK(ValueEvents("1979-05-27") == Events{"DateTime(1979-05-27)"});
    CHECK(ValueEvents("07:32:00") == Events{"DateTime(07:32:00)"});
}

TEST_CASE("a date followed by a space and something other than a digit is just a date") {
    CHECK(EventsOf("d = 1979-05-27 # comment\n") == Events{
        "EnterObject", "Key(d)", "DateTime(1979-05-27)", "ExitObject"});
}

TEST_CASE("strings decode their escapes and may span lines") {
    CHECK(ValueEvents("\"a\\tb\\u00e9\"") == Events{"String(a\tb\xC3\xA9)"});
    CHECK(ValueEvents("'C:\\path'") == Events{"String(C:\\path)"});
    CHECK(EventsOf("s = \"\"\"\nline one\nline two \\\n  continued\"\"\"\nn = 1") == Events{
        "EnterObject", "Key(s)", "String(line one\nline two continued)", "Key(n)", "Int(1)", "ExitObject"});
}

TEST_CASE("keys may be bare, quoted, empty when quoted, and look like other values") {
    CHECK(EventsOf("bare-key_1 = 1\n\"quoted key\" = 2\n'literal \"key\"' = 3\n\"\" = 4\n1234 = 5\ntrue = 6") == Events{
        "EnterObject",
        "Key(bare-key_1)", "Int(1)",
        "Key(quoted key)", "Int(2)",
        "Key(literal \"key\")", "Int(3)",
        "Key()", "Int(4)",
        "Key(1234)", "Int(5)",
        "Key(true)", "Int(6)",
        "ExitObject"});
    CHECK(EventsOf("\"\\u0041\" = 1") == Events{"EnterObject", "Key(A)", "Int(1)", "ExitObject"});
}

TEST_CASE("dotted keys create nested tables in the order their keys first appear") {
    CHECK(EventsOf("a.b = 1\nc = 2\na . \"x.y\" = 3\na.d.e = 4") == Events{
        "EnterObject",
        "Key(a)", "EnterObject",
            "Key(b)", "Int(1)",
            "Key(x.y)", "Int(3)",
            "Key(d)", "EnterObject", "Key(e)", "Int(4)", "ExitObject",
            "ExitObject",
        "Key(c)", "Int(2)",
        "ExitObject"});
}

TEST_CASE("arrays may nest, mix types, span lines and end with a comma") {
    CHECK(ValueEvents("[]") == Events{"EnterArray", "ExitArray"});
    CHECK(ValueEvents("[ 1, 'a', [true], [] ]") == Events{
        "EnterArray", "Int(1)", "String(a)", "EnterArray", "Bool(true)", "ExitArray",
        "EnterArray", "ExitArray", "ExitArray"});
    CHECK(ValueEvents("[\n  1, # one\n\n  2,\r\n  # nothing\n]") == Events{
        "EnterArray", "Int(1)", "Int(2)", "ExitArray"});
    CHECK(ValueEvents("[1,2,]") == Events{"EnterArray", "Int(1)", "Int(2)", "ExitArray"});
}

TEST_CASE("inline tables may nest and use dotted keys") {
    CHECK(ValueEvents("{}") == Events{"EnterObject", "ExitObject"});
    CHECK(ValueEvents("{ a = 1, b.c = 'x', b.d = {e = true} }") == Events{
        "EnterObject",
        "Key(a)", "Int(1)",
        "Key(b)", "EnterObject",
            "Key(c)", "String(x)",
            "Key(d)", "EnterObject", "Key(e)", "Bool(true)", "ExitObject",
            "ExitObject",
        "ExitObject"});
    CHECK(ValueEvents("[{a = 1}, {}]") == Events{
        "EnterArray", "EnterObject", "Key(a)", "Int(1)", "ExitObject", "EnterObject", "ExitObject", "ExitArray"});
}

TEST_CASE("the text must be UTF-8 throughout, comments included") {
    CheckError("a = 1 # \xFF\n", ErrorCode::InvalidUtf8, 8);
    CheckError("a = \"\xC0\x80\"", ErrorCode::InvalidUtf8, 5);
    // The encoding is checked first, even when a syntax error comes before.
    CheckError("= 1\n# \xFF", ErrorCode::InvalidUtf8, 6);
}

TEST_CASE("a byte order mark is UnexpectedChar") {
    CheckError("\xEF\xBB\xBF" "a = 1", ErrorCode::UnexpectedChar, 0);
}

TEST_CASE("control characters are rejected in comments and between tokens") {
    CheckError("a = 1 # tab\tis fine, this is not:\x01\n", ErrorCode::UnexpectedChar, 33);
    CheckError("# \x7F", ErrorCode::UnexpectedChar, 2);
    CheckError("a = 1\rb = 2", ErrorCode::UnexpectedChar, 5);
    CheckError("# comment\r", ErrorCode::UnexpectedChar, 9);
    CheckError("a = [1,\r2]", ErrorCode::UnexpectedChar, 7);
}

TEST_CASE("a key/value line needs a key, an equals sign and a value") {
    CheckError("= 1", ErrorCode::UnexpectedChar, 0);
    CheckError("a", ErrorCode::UnexpectedEnd, 1);
    CheckError("a 1", ErrorCode::UnexpectedChar, 2);
    CheckError("a =", ErrorCode::UnexpectedEnd, 3);
    CheckError("a = \n", ErrorCode::UnexpectedChar, 4);
    CheckError("a = # no value", ErrorCode::UnexpectedChar, 4);
    CheckError("a. = 1", ErrorCode::UnexpectedChar, 3);
    CheckError(".a = 1", ErrorCode::UnexpectedChar, 0);
    CheckError("a..b = 1", ErrorCode::UnexpectedChar, 2);
    CheckError("a b = 1", ErrorCode::UnexpectedChar, 2);
    CheckError("a$ = 1", ErrorCode::UnexpectedChar, 1);
}

TEST_CASE("a key cannot be a multi-line string") {
    CheckError("\"\"\"a\"\"\" = 1", ErrorCode::UnexpectedChar, 0);
    CheckError("'''a''' = 1", ErrorCode::UnexpectedChar, 0);
}

TEST_CASE("only one key/value pair fits on a line") {
    CheckError("a = 1 b = 2", ErrorCode::UnexpectedChar, 6);
    CheckError("a = 'x'y", ErrorCode::UnexpectedChar, 7);
    CheckError("a = true1", ErrorCode::UnexpectedChar, 8);
}

TEST_CASE("a value must be one of the known kinds") {
    CheckError("a = x", ErrorCode::UnexpectedChar, 4);
    CheckError("a = no", ErrorCode::UnexpectedChar, 4);
    CheckError("a = tru", ErrorCode::UnexpectedEnd, 7);
    CheckError("a = trUe", ErrorCode::UnexpectedChar, 6);
    CheckError("a = .5", ErrorCode::UnexpectedChar, 4);
}

TEST_CASE("errors inside a string are reported where they are") {
    CheckError("a = \"abc", ErrorCode::UnexpectedEnd, 8);
    CheckError("a = \"a\\qb\"", ErrorCode::InvalidEscape, 6);
    CheckError("a = \"a\nb\"", ErrorCode::UnexpectedChar, 6);
    CheckError("\"a\\x\" = 1", ErrorCode::InvalidEscape, 2);
}

TEST_CASE("a malformed number is InvalidNumber at its start") {
    CheckError("a = 01", ErrorCode::InvalidNumber, 4);
    CheckError("a = 1__0", ErrorCode::InvalidNumber, 4);
    CheckError("a = 9223372036854775808", ErrorCode::InvalidNumber, 4);
    CheckError("a = [1, 2x]", ErrorCode::InvalidNumber, 8);
    CheckError("a = 1e400", ErrorCode::InvalidNumber, 4);
    CheckError("a = infinity", ErrorCode::InvalidNumber, 4);
}

TEST_CASE("a malformed datetime is UnexpectedChar where it goes wrong") {
    CheckError("a = 1979-05-27T07:32", ErrorCode::UnexpectedEnd, 20);
    CheckError("a = 1979-05-27T07:32\n", ErrorCode::UnexpectedChar, 20);
    CheckError("a = 1979-05-27T07:32:00+7", ErrorCode::UnexpectedEnd, 25);
    CheckError("a = 1979-5-27", ErrorCode::UnexpectedChar, 10);
    CheckError("a = 07:32:00Z", ErrorCode::UnexpectedChar, 12);
    CheckError("a = 1979-05-27 7:32:00", ErrorCode::UnexpectedChar, 16);
}

TEST_CASE("a datetime with a field out of range is InvalidDateTime at its start") {
    CheckError("a = 2023-02-29", ErrorCode::InvalidDateTime, 4);
    CheckError("a = [07:32:00, 25:00:00]", ErrorCode::InvalidDateTime, 15);
    CheckError("a = 1979-05-27T00:00:00+24:00", ErrorCode::InvalidDateTime, 4);
}

TEST_CASE("arrays need commas between elements and a closing bracket") {
    CheckError("a = [1 2]", ErrorCode::UnexpectedChar, 7);
    CheckError("a = [,]", ErrorCode::UnexpectedChar, 5);
    CheckError("a = [1,,2]", ErrorCode::UnexpectedChar, 7);
    CheckError("a = [1", ErrorCode::UnexpectedEnd, 6);
    CheckError("a = [1,", ErrorCode::UnexpectedEnd, 7);
    CheckError("a = [", ErrorCode::UnexpectedEnd, 5);
}

TEST_CASE("inline tables stay on one line and have no trailing comma") {
    CheckError("a = {b = 1,}", ErrorCode::UnexpectedChar, 11);
    CheckError("a = {b = 1\n}", ErrorCode::UnexpectedChar, 10);
    CheckError("a = {\nb = 1}", ErrorCode::UnexpectedChar, 5);
    CheckError("a = {b = 1 c = 2}", ErrorCode::UnexpectedChar, 11);
    CheckError("a = {b = 1", ErrorCode::UnexpectedEnd, 10);
    CheckError("a = {,}", ErrorCode::UnexpectedChar, 5);
}

TEST_CASE("a key cannot be defined twice") {
    CheckError("a = 1\na = 2", ErrorCode::DuplicateKey, 6);
    CheckError("a = 1\n\"a\" = 2", ErrorCode::DuplicateKey, 6);
    CheckError("a = 1\n'a' = 2", ErrorCode::DuplicateKey, 6);
    CheckError("a = {b = 1, b = 2}", ErrorCode::DuplicateKey, 12);
    // The key is checked before its value is read.
    CheckError("a = 1\na = !", ErrorCode::DuplicateKey, 6);
}

TEST_CASE("a dotted key cannot go through a value or an inline table, nor redefine a table") {
    // Through a scalar.
    CheckError("a = 1\na.b = 2", ErrorCode::DuplicateKey, 6);
    // Through an array.
    CheckError("a = []\na.b = 2", ErrorCode::DuplicateKey, 7);
    // Into an inline table, from outside or inside.
    CheckError("a = {}\na.b = 2", ErrorCode::DuplicateKey, 7);
    CheckError("a = {b = {}, b.c = 1}", ErrorCode::DuplicateKey, 13);
    CheckError("a = {k1 = 1, k1.name = 'joe'}", ErrorCode::DuplicateKey, 13);
    // A table made by dotted keys is not a value to overwrite.
    CheckError("a.b = 1\na = 2", ErrorCode::DuplicateKey, 8);
    CheckError("a.b.c = 1\na.b = 2", ErrorCode::DuplicateKey, 12);
}

TEST_CASE("line and column count from 1, and a CRLF is one line break") {
    Error error = ErrorOf("a = 1\r\n\r\nb = 2\nc = @");
    CHECK(error.code == ErrorCode::UnexpectedChar);
    CHECK(error.offset == 19);
    CHECK(error.line == 4);
    CHECK(error.column == 5);

    error = ErrorOf("s = '''\nab\n'''x");
    CHECK(error.line == 3);
    CHECK(error.column == 4);
}

TEST_CASE("nesting is limited to kMaxDepth levels, counting the root table") {
    const int NESTED = toml::kMaxDepth - 1;   // Containers inside the root table.

    std::string arrays = "a = " + std::string(NESTED, '[') + std::string(NESTED, ']');
    CHECK(EventsOf(arrays).size() == 2 + 1 + 2 * NESTED);
    arrays = "a = " + std::string(NESTED + 1, '[') + std::string(NESTED + 1, ']');
    CheckError(arrays, ErrorCode::DepthExceeded, 4 + NESTED);

    std::string inlines;
    for (int i = 0; i < NESTED; ++i) {
        inlines += "{a=";
    }
    std::string tables = "a = " + inlines + "1" + std::string(NESTED, '}');
    CHECK(EventsOf(tables).size() == 4 + 3 * NESTED);
    tables = "a = " + inlines + "{}" + std::string(NESTED, '}');
    CheckError(tables, ErrorCode::DepthExceeded, 4 + 3 * NESTED);

    // A dotted key with n segments makes n - 1 tables.
    std::string dotted;
    for (int i = 0; i < NESTED; ++i) {
        dotted += "a.";
    }
    CHECK(EventsOf(dotted + "b = 1").size() == 4 + 3 * NESTED);
    CheckError(dotted + "b.c = 1", ErrorCode::DepthExceeded, 2 * NESTED);
}

TEST_CASE("a rejected event stops the replay with Aborted at its source") {
    // a.b = [1, {c = 2}]
    //  offsets: a=0, b=2, [=6, 1=7, {=10, c=11, 2=15
    const std::string text = "a.b = [1, {c = 2}]";
    struct Case {
        int failAt;
        const char* event;
        size_t offset;
    };
    const Case CASES[] = {
        {0, "EnterObject", 0},    // The root table.
        {1, "Key(a)", 0},
        {2, "EnterObject", 0},    // The table the dotted key made.
        {3, "Key(b)", 2},
        {4, "EnterArray", 6},
        {5, "Int(1)", 7},
        {6, "EnterObject", 10},   // The inline table.
        {7, "Key(c)", 11},
        {8, "Int(2)", 15},
        {9, "ExitObject", 10},
        {10, "ExitArray", 6},
        {11, "ExitObject", 0},
        {12, "ExitObject", 0},
    };
    for (const Case& test : CASES) {
        CAPTURE(test.failAt);
        RecordingVisitor visitor;
        visitor.failAt = test.failAt;
        toml::Reader reader;
        Error error = reader.Parse(text, visitor);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == test.offset);
        CHECK(error.line == 1);
        CHECK(error.column == static_cast<int>(test.offset) + 1);
        // Nothing after the rejected event.
        REQUIRE(visitor.events.size() == static_cast<size_t>(test.failAt) + 1);
        CHECK(visitor.events.back() == test.event);
    }
}

TEST_CASE("Aborted gives the line and column of the value") {
    RecordingVisitor visitor;
    visitor.failAt = 4;   // EnterObject, Key(a), Int(1), Key(b), String(x)
    toml::Reader reader;
    Error error = reader.Parse("a = 1\nb = 'x'", visitor);
    CHECK(error.code == ErrorCode::Aborted);
    CHECK(error.offset == 10);
    CHECK(error.line == 2);
    CHECK(error.column == 5);
}

TEST_CASE("the input may be part of a larger buffer and is not read past its end") {
    const std::string buffer = "a = [1, 2]\nb = 3";
    // Stops in the middle of the array: the rest of the buffer is not seen.
    CheckError(std::string_view(buffer.data(), 8), ErrorCode::UnexpectedEnd, 8);
    // Stops right after "b = ": no value.
    CheckError(std::string_view(buffer.data(), 15), ErrorCode::UnexpectedEnd, 15);
    const std::string quoted = "a = \"xy\"";
    CheckError(std::string_view(quoted.data(), 7), ErrorCode::UnexpectedEnd, 7);
}

TEST_CASE("a Reader can be used again after a failure") {
    toml::Reader reader;
    RecordingVisitor failed;
    CHECK(reader.Parse("a = 1\na = 2", failed).code == ErrorCode::DuplicateKey);

    RecordingVisitor visitor;
    CHECK(reader.Parse("a = 1", visitor).Ok());
    CHECK(visitor.events == Events{"EnterObject", "Key(a)", "Int(1)", "ExitObject"});
}

TEST_CASE("keys are compared after decoding, so differently quoted keys can clash") {
    CheckError("a = 1\n\"\\u0061\" = 2", ErrorCode::DuplicateKey, 6);
    CheckError("\"a b\" = 1\n'a b' = 2", ErrorCode::DuplicateKey, 10);
    CheckError("x.\"\\u0062\" = 1\nx.b = 2", ErrorCode::DuplicateKey, 17);
    // Different spellings of the same text are the same key, but "1" and
    // "01" are not the same number.
    CHECK(EventsOf("1 = 'a'\n01 = 'b'") == Events{
        "EnterObject", "Key(1)", "String(a)", "Key(01)", "String(b)", "ExitObject"});
}

TEST_CASE("decoded strings and keys longer than an arena block survive") {
    std::string longText(10000, 'x');
    std::string escaped;
    for (size_t i = 0; i < longText.size(); ++i) {
        escaped += "\\u0078";   // 'x'
    }
    CHECK(EventsOf("\"" + escaped + "\" = \"" + escaped + "\"\nk = 'short\\'") == Events{
        "EnterObject", "Key(" + longText + ")", "String(" + longText + ")", "Key(k)", "String(short\\)",
        "ExitObject"});
}
