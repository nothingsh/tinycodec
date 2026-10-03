// Tables and arrays of tables: headers, and the rules for defining a table
// only once (spec 4.2.5).

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

Events EventsOf(std::string_view text) {
    RecordingVisitor visitor;
    toml::Reader reader;
    Error error = reader.Parse(text, visitor);
    INFO("error: ", ErrorName(error.code), " at offset ", error.offset);
    REQUIRE(error.Ok());
    return visitor.events;
}

void CheckError(std::string_view text, ErrorCode code, size_t offset) {
    CAPTURE(text);
    RecordingVisitor visitor;
    toml::Reader reader;
    Error error = reader.Parse(text, visitor);
    CHECK(ErrorName(error.code) == std::string(ErrorName(code)));
    CHECK(error.offset == offset);
    CHECK(visitor.events.empty());
}

}  // namespace

TEST_CASE("a header starts a table, and the lines after it go into that table") {
    CHECK(EventsOf("top = 0\n[a]\nx = 1\n\n[b]\ny = 2\n") == Events{
        "EnterObject",
        "Key(top)", "Int(0)",
        "Key(a)", "EnterObject", "Key(x)", "Int(1)", "ExitObject",
        "Key(b)", "EnterObject", "Key(y)", "Int(2)", "ExitObject",
        "ExitObject"});
}

TEST_CASE("a header may be empty, have spaces and quoted segments, and a comment after it") {
    CHECK(EventsOf("[ a . \"b c\" . 'd' ] # comment\n[e]") == Events{
        "EnterObject",
        "Key(a)", "EnterObject",
            "Key(b c)", "EnterObject",
                "Key(d)", "EnterObject", "ExitObject",
                "ExitObject",
            "ExitObject",
        "Key(e)", "EnterObject", "ExitObject",
        "ExitObject"});
}

TEST_CASE("a header path creates the tables on its way, which may be defined later") {
    CHECK(EventsOf("[x.y.z]\na = 1\n[x]\nb = 2") == Events{
        "EnterObject",
        "Key(x)", "EnterObject",
            "Key(y)", "EnterObject",
                "Key(z)", "EnterObject", "Key(a)", "Int(1)", "ExitObject",
                "ExitObject",
            "Key(b)", "Int(2)",
            "ExitObject",
        "ExitObject"});
}

TEST_CASE("a table may be written in pieces; members keep the order they first appeared in") {
    CHECK(EventsOf("[a.b]\nx = 1\n[c]\n[a.d]\ny = 2") == Events{
        "EnterObject",
        "Key(a)", "EnterObject",
            "Key(b)", "EnterObject", "Key(x)", "Int(1)", "ExitObject",
            "Key(d)", "EnterObject", "Key(y)", "Int(2)", "ExitObject",
            "ExitObject",
        "Key(c)", "EnterObject", "ExitObject",
        "ExitObject"});
}

TEST_CASE("a header may go into a table made by dotted keys") {
    CHECK(EventsOf("[fruit]\napple.color = 'red'\n[fruit.apple.texture]\nsmooth = true") == Events{
        "EnterObject",
        "Key(fruit)", "EnterObject",
            "Key(apple)", "EnterObject",
                "Key(color)", "String(red)",
                "Key(texture)", "EnterObject", "Key(smooth)", "Bool(true)", "ExitObject",
                "ExitObject",
            "ExitObject",
        "ExitObject"});
}

TEST_CASE("each [[header]] adds a table to an array of tables") {
    CHECK(EventsOf("[[item]]\nx = 1\n[[item]]\n[[item]]\nx = 3") == Events{
        "EnterObject",
        "Key(item)", "EnterArray",
            "EnterObject", "Key(x)", "Int(1)", "ExitObject",
            "EnterObject", "ExitObject",
            "EnterObject", "Key(x)", "Int(3)", "ExitObject",
            "ExitArray",
        "ExitObject"});
}

TEST_CASE("a header path goes into the last table of an array of tables") {
    CHECK(EventsOf("[[a]]\n[a.b]\nx = 1\n[[a]]\n[[a.c]]\ny = 2\n[[a.c]]\n") == Events{
        "EnterObject",
        "Key(a)", "EnterArray",
            "EnterObject",
                "Key(b)", "EnterObject", "Key(x)", "Int(1)", "ExitObject",
                "ExitObject",
            "EnterObject",
                "Key(c)", "EnterArray",
                    "EnterObject", "Key(y)", "Int(2)", "ExitObject",
                    "EnterObject", "ExitObject",
                    "ExitArray",
                "ExitObject",
            "ExitArray",
        "ExitObject"});
}

TEST_CASE("headers need a key and their closing brackets") {
    CheckError("[]", ErrorCode::UnexpectedChar, 1);
    CheckError("[[]]", ErrorCode::UnexpectedChar, 2);
    CheckError("[a", ErrorCode::UnexpectedEnd, 2);
    CheckError("[a\n", ErrorCode::UnexpectedChar, 2);
    CheckError("[[a]", ErrorCode::UnexpectedEnd, 4);
    CheckError("[[a]\n", ErrorCode::UnexpectedChar, 4);
    CheckError("[[a] ]", ErrorCode::UnexpectedChar, 4);
    CheckError("[ [a]]", ErrorCode::UnexpectedChar, 2);
    CheckError("[a]]", ErrorCode::UnexpectedChar, 3);
    CheckError("[a.]", ErrorCode::UnexpectedChar, 3);
    CheckError("[a b]", ErrorCode::UnexpectedChar, 3);
    CheckError("[\"\"\"a\"\"\"]", ErrorCode::UnexpectedChar, 1);
}

TEST_CASE("nothing but a comment may follow a header on its line") {
    CheckError("[a] x = 1", ErrorCode::UnexpectedChar, 4);
    CheckError("[[a]] [b]", ErrorCode::UnexpectedChar, 6);
}

// Each row creates a node `x` inside table `p` in one of the ways a node
// can come to be; each column then tries one way of using the name p.x
// again. The expected results are the table in spec 4.2.5.
TEST_CASE("the rules for reusing a name, case by case") {
    struct Setup {
        const char* name;
        const char* text;
        bool inSectionP;   // The text ends inside the [p] section.
    };
    const Setup SETUPS[] = {
        {"HeaderTable", "[p.x]", false},
        {"ImplicitTable", "[p.x.y]", false},
        {"DottedTable", "[p]\nx.y = 1", true},
        {"TableArray", "[[p.x]]", false},
        {"InlineTable", "[p]\nx = {}", true},
        {"StaticArray", "[p]\nx = []", true},
        {"scalar", "[p]\nx = 1", true},
    };

    struct Usage {
        const char* name;
        const char* text;
        bool isKeyValue;    // Needs to be in the [p] section.
        size_t keyOffset;   // Where `x` is in the text.
    };
    const Usage USAGES[] = {
        {"key/value last", "x = 2", true, 0},
        {"key/value middle", "x.z = 2", true, 0},
        {"header last", "[p.x]", false, 3},
        {"header middle", "[p.x.z]", false, 3},
        {"array of tables last", "[[p.x]]", false, 4},
    };

    // true: allowed; false: DuplicateKey at the x of the usage.
    const bool ALLOWED[7][5] = {
        // KV last, KV middle, header last, header middle, [[ ]] last
        {false, false, false, true, false},    // HeaderTable
        {false, true, true, true, false},      // ImplicitTable
        {false, true, false, true, false},     // DottedTable
        {false, false, false, true, true},     // TableArray
        {false, false, false, false, false},   // InlineTable
        {false, false, false, false, false},   // StaticArray
        {false, false, false, false, false},   // scalar
    };

    for (size_t row = 0; row < 7; ++row) {
        for (size_t column = 0; column < 5; ++column) {
            const Setup& setup = SETUPS[row];
            const Usage& usage = USAGES[column];
            std::string text = std::string(setup.text) + "\n";
            if (usage.isKeyValue && !setup.inSectionP) {
                text += "[p]\n";
            }
            size_t usageStart = text.size();
            text += usage.text;

            CAPTURE(setup.name);
            CAPTURE(usage.name);
            CAPTURE(text);
            RecordingVisitor visitor;
            toml::Reader reader;
            Error error = reader.Parse(text, visitor);
            if (ALLOWED[row][column]) {
                CHECK(error.Ok());
            } else {
                CHECK(error.code == ErrorCode::DuplicateKey);
                CHECK(error.offset == usageStart + usage.keyOffset);
            }
        }
    }
}

TEST_CASE("a dotted key may go into a table a header path made, which stays definable") {
    CHECK(EventsOf("[a.b.c]\n[a]\nb.d = 1\n[a.b]\ne = 2") == Events{
        "EnterObject",
        "Key(a)", "EnterObject",
            "Key(b)", "EnterObject",
                "Key(c)", "EnterObject", "ExitObject",
                "Key(d)", "Int(1)",
                "Key(e)", "Int(2)",
                "ExitObject",
            "ExitObject",
        "ExitObject"});
}

TEST_CASE("cases from toml-test that a table rule must catch") {
    // A header table cannot be extended by dotted keys from a parent section.
    CheckError("[a.b.c]\nz = 9\n[a]\nb.c.t = 1", ErrorCode::DuplicateKey, 20);
    // An array of tables cannot be extended by dotted keys either.
    CheckError("[[a.b]]\n[a]\nb.y = 2", ErrorCode::DuplicateKey, 12);
    // [dependencies] defined twice, once after a subtable.
    CheckError("[dependencies.foo]\nversion = '0.16'\n[dependencies]\nlibc = '0.2'\n[dependencies]\n",
               ErrorCode::DuplicateKey, 65);
    // A static array is not an array of tables.
    CheckError("a = [{b = 1}]\n[[a]]", ErrorCode::DuplicateKey, 16);
    // An array of tables is not a table.
    CheckError("[[a]]\n[a]", ErrorCode::DuplicateKey, 7);
    // A header cannot reach into an inline table.
    CheckError("a = {b = {}}\n[a.b.c]", ErrorCode::DuplicateKey, 14);
}

TEST_CASE("a header cannot name a value") {
    CheckError("a = 1\n[a]", ErrorCode::DuplicateKey, 7);
}

TEST_CASE("headers count towards the nesting limit") {
    // A header with n segments makes n tables below the root.
    const int SEGMENTS = toml::kMaxDepth - 1;
    std::string path = "a";
    for (int i = 1; i < SEGMENTS; ++i) {
        path += ".a";
    }
    CHECK(EventsOf("[" + path + "]").size() == 2 + 3 * static_cast<size_t>(SEGMENTS));
    CheckError("[" + path + ".b]", ErrorCode::DepthExceeded, 1 + path.size() + 1);
    // An array of tables and its element are two levels.
    std::string shorter = path.substr(2);   // SEGMENTS - 1 segments.
    CHECK(EventsOf("[[" + shorter + "]]").size() == 3 * static_cast<size_t>(SEGMENTS) + 1);
    CheckError("[[" + path + "]]", ErrorCode::DepthExceeded, 0);
}

TEST_CASE("a rejected event in a table is Aborted at the source of the table") {
    // [p.q]  -> p at 1 (implicit), q at 3
    // [[r]]  -> r at 8, element at 6
    // x = 1  -> x at 12, 1 at 16
    const std::string text = "[p.q]\n[[r]]\nx = 1";
    struct Case {
        int failAt;
        const char* event;
        size_t offset;
    };
    const Case CASES[] = {
        {1, "Key(p)", 1},
        {2, "EnterObject", 1},    // p, made by the header path.
        {3, "Key(q)", 3},
        {4, "EnterObject", 3},    // q, made by the header.
        {5, "ExitObject", 3},
        {6, "ExitObject", 1},
        {7, "Key(r)", 8},
        {8, "EnterArray", 8},     // The array of tables.
        {9, "EnterObject", 6},    // Its element: the "[[".
        {10, "Key(x)", 12},
        {11, "Int(1)", 16},
        {12, "ExitObject", 6},
        {13, "ExitArray", 8},
        {14, "ExitObject", 0},    // The root.
    };
    for (const Case& test : CASES) {
        CAPTURE(test.failAt);
        RecordingVisitor visitor;
        visitor.failAt = test.failAt;
        toml::Reader reader;
        Error error = reader.Parse(text, visitor);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == test.offset);
        REQUIRE(visitor.events.size() == static_cast<size_t>(test.failAt) + 1);
        CHECK(visitor.events.back() == test.event);
    }
}
