#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "datetimes.h"
#include "tinycodec/builder.h"
#include "tinycodec/sink.h"
#include "tinycodec/toml/toml.h"
#include "unordered_equal.h"

using namespace tinycodec;

namespace {

// A tree with every type TOML can express.
Value* BuildSample(Document& document) {
    Value* root = document.NewObject();
    root->Set("string", document.NewString("tiny \"codec\"\n"));
    root->Set("int", document.NewInt(INT64_MIN));
    root->Set("double", document.NewDouble(0.1));
    root->Set("nan", document.NewDouble(std::numeric_limits<double>::quiet_NaN()));
    root->Set("bool", document.NewBool(true));
    root->Set("date", document.NewDateTime(MakeDate(1979, 5, 27)));
    root->Set("moment", document.NewDateTime(MakeOffsetDateTime(1979, 5, 27, 0, 32, 0, 999999000, -420)));
    Value* list = document.NewArray();
    list->Append(document.NewInt(1));
    list->Append(document.NewDateTime(MakeTime(7, 32, 0)));
    Value* point = document.NewObject();
    point->Set("x", document.NewInt(1));
    list->Append(point);
    root->Set("list", list);
    Value* table = document.NewObject();
    table->Set("key with space", document.NewString(""));
    Value* items = document.NewArray();
    items->Append(document.NewObject());
    Value* item = document.NewObject();
    item->Set("local", document.NewDateTime(MakeLocalDateTime(2000, 2, 29, 23, 59, 60)));
    items->Append(item);
    table->Set("items", items);
    root->Set("table", table);
    return root;
}

}  // namespace

TEST_CASE("Parse builds a tree that can be queried") {
    Document document;
    Error error = toml::Parse("name = 'tiny'\n[server]\nports = [80, 443]\n", document);
    REQUIRE(error.Ok());
    std::string_view name;
    CHECK(document.Root()->Find("name")->QueryString(&name));
    CHECK(name == "tiny");
    int64_t port = 0;
    CHECK(document.Root()->Find("server")->Find("ports")->At(1)->QueryInt(&port));
    CHECK(port == 443);
}

TEST_CASE("Parse replaces the previous content of the document") {
    Document document;
    REQUIRE(toml::Parse("a = 1", document).Ok());
    REQUIRE(toml::Parse("b = 2", document).Ok());
    CHECK(document.Root()->Find("a") == nullptr);
    CHECK(document.Root()->Find("b") != nullptr);
}

TEST_CASE("a failed Parse reports the error and leaves the document empty") {
    Document document;
    REQUIRE(toml::Parse("a = 1", document).Ok());
    Error error = toml::Parse("a = 1\na = 2", document);
    CHECK(error.code == ErrorCode::DuplicateKey);
    CHECK(error.line == 2);
    CHECK(document.Root() == nullptr);
}

TEST_CASE("Stringify writes the empty table as an empty string, and succeeds") {
    Document document;
    std::string out = "unchanged";
    CHECK(toml::Stringify(*document.NewObject(), &out));
    CHECK(out.empty());
}

TEST_CASE("Stringify fails and leaves the output alone for what TOML cannot express") {
    Document document;
    std::string out = "unchanged";
    CHECK_FALSE(toml::Stringify(*document.NewInt(1), &out));
    CHECK_FALSE(toml::Stringify(*document.NewArray(), &out));
    Value* object = document.NewObject();
    object->Set("a", document.NewNull());
    CHECK_FALSE(toml::Stringify(*object, &out));
    object->Set("a", document.NewBytes("x"));
    CHECK_FALSE(toml::Stringify(*object, &out));
    object->Set("a", document.NewUint(UINT64_MAX));
    CHECK_FALSE(toml::Stringify(*object, &out));
    object->Set("a", document.NewDateTime(MakeDate(2023, 2, 30)));
    CHECK_FALSE(toml::Stringify(*object, &out));
    CHECK(out == "unchanged");
}

TEST_CASE("Stringify and Parse round-trip every type TOML can express") {
    Document document;
    Value* sample = BuildSample(document);
    std::string text;
    REQUIRE(toml::Stringify(*sample, &text));
    Document copy;
    Error error = toml::Parse(text, copy);
    INFO(text);
    REQUIRE(error.Ok());
    CHECK(EqualsIgnoringOrder(*sample, *copy.Root()));

    std::string again;
    REQUIRE(toml::Stringify(*copy.Root(), &again));
    CHECK(again == text);
}

TEST_CASE("the round trip keeps content, not member order") {
    Document document;
    REQUIRE(toml::Parse("a.b = 1\nc = 2", document).Ok());
    std::string text;
    REQUIRE(toml::Stringify(*document.Root(), &text));
    CHECK(text == "c = 2\n\n[a]\nb = 1\n");
    Document copy;
    REQUIRE(toml::Parse(text, copy).Ok());
    CHECK_FALSE(document.Root()->Equals(*copy.Root()));
    CHECK(EqualsIgnoringOrder(*document.Root(), *copy.Root()));
}

TEST_CASE("keys and strings with any character survive Stringify and Parse") {
    std::string every;
    for (int byte = 0; byte < 0x80; ++byte) {
        every += static_cast<char>(byte);
    }
    every += "\xC3\xA9\xE4\xBD\xA0\xF0\x9F\x98\x80";
    const char* keys[] = {"", " ", ".", "a.b", "\"", "'", "\\", "#", "=", "[x]", "true", "1234", "inf", "-",
                          "\n", "\x7F", "\xC3\xA9"};

    Document document;
    Value* root = document.NewObject();
    Value* table = document.NewObject();
    for (const char* key : keys) {
        root->Set(key, document.NewString(every));
        // The same keys in a header path, and in an array of tables.
        Value* inner = document.NewObject();
        inner->Set(key, document.NewString(key));
        table->Set(key, inner);
    }
    root->Set("table", table);
    Value* items = document.NewArray();
    Value* item = document.NewObject();
    item->Set("", document.NewString(every));
    items->Append(item);
    table->Set("\x01 items", items);

    std::string text;
    REQUIRE(toml::Stringify(*root, &text));
    Document copy;
    Error error = toml::Parse(text, copy);
    INFO(text);
    INFO("error: ", ErrorName(error.code), " at line ", error.line, ", column ", error.column);
    REQUIRE(error.Ok());
    CHECK(EqualsIgnoringOrder(*root, *copy.Root()));
}

TEST_CASE("nested tables and arrays of tables in every combination survive Stringify and Parse") {
    const char* texts[] = {
        // An array of tables inside the elements of another.
        "[[a]]\nx = 1\n[[a.b]]\ny = 2\n[[a.b]]\n[[a]]\n[[a.b]]\nz = 3\n",
        // A table that holds only arrays of tables, so gets no header.
        "[[a.b]]\nx = 1\n[[a.c]]\n",
        // Arrays of arrays of tables, and tables inside inline arrays.
        "a = [[{x = 1}], [{y = [{z = 2}]}]]\n",
        // An element that holds only a subtable.
        "[[a]]\n[a.b]\nx = 1\n[[a]]\n[a.b.c]\n",
        // Empty tables at each level.
        "[a]\n[a.b]\n[c]\nd = {}\ne = [{}]\n",
        // Empty and quoted keys in header paths.
        "[\"\".\"a.b\".'c d']\n\"\" = 1\n",
    };
    for (const char* text : texts) {
        CAPTURE(text);
        Document document;
        REQUIRE(toml::Parse(text, document).Ok());
        std::string written;
        REQUIRE(toml::Stringify(*document.Root(), &written));
        CAPTURE(written);
        Document copy;
        REQUIRE(toml::Parse(written, copy).Ok());
        CHECK(EqualsIgnoringOrder(*document.Root(), *copy.Root()));
    }
}

TEST_CASE("the deepest document the Writer accepts can be read back") {
    // kMaxDepth levels of tables.
    Document tables;
    Value* root = tables.NewObject();
    Value* parent = root;
    for (int level = 2; level <= toml::kMaxDepth; ++level) {
        Value* child = tables.NewObject();
        parent->Set("a", child);
        parent = child;
    }
    parent->Set("x", tables.NewInt(1));
    std::string text;
    REQUIRE(toml::Stringify(*root, &text));
    Document copy;
    REQUIRE(toml::Parse(text, copy).Ok());
    CHECK(copy.Root()->Equals(*root));

    // Arrays of tables down to kMaxDepth: each is two levels, with its element.
    Document arrays;
    root = arrays.NewObject();
    parent = root;
    for (int level = 3; level <= toml::kMaxDepth; level += 2) {
        Value* list = arrays.NewArray();
        Value* element = arrays.NewObject();
        list->Append(element);
        parent->Set("a", list);
        parent = element;
    }
    REQUIRE(toml::Stringify(*root, &text));
    REQUIRE(toml::Parse(text, copy).Ok());
    CHECK(copy.Root()->Equals(*root));
}

TEST_CASE("Reader to Writer gives the same text as going through a Document") {
    const char* text =
        "title = 'x'\n"
        "[owner]\nname = \"Tom\"\ndob = 1979-05-27T07:32:00-08:00\n"
        "[database]\nports = [8000, 8001, 8002]\ndata = [['delta', 'phi'], [3.14]]\n"
        "temp_targets = {cpu = 79.5, case = 72.0}\n"
        "[[products]]\nname = 'Hammer'\nsku = 738594937\n"
        "[[products]]\n"
        "[[products]]\nname = 'Nail'\ncolor = 'gray'\n";

    Document document;
    REQUIRE(toml::Parse(text, document).Ok());
    std::string viaDocument;
    REQUIRE(toml::Stringify(*document.Root(), &viaDocument));

    StringSink direct;
    toml::Writer writer(direct);
    toml::Reader reader;
    REQUIRE(reader.Parse(text, writer).Ok());
    CHECK(direct.Str() == viaDocument);
}
