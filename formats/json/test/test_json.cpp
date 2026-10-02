#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "tinycodec/json/json.h"

using namespace tinycodec;

TEST_CASE("Parse builds a tree that can be queried") {
    Document document;
    Error error = json::Parse("{\"name\":\"tiny\",\"tags\":[\"a\",\"b\"],\"size\":3,\"ratio\":0.5}", document);
    REQUIRE(error.Ok());

    const Value* root = document.Root();
    REQUIRE(root != nullptr);
    CHECK(root->GetType() == Type::Object);

    std::string_view name;
    REQUIRE(root->Find("name") != nullptr);
    CHECK(root->Find("name")->QueryString(&name));
    CHECK(name == "tiny");

    const Value* tags = root->Find("tags");
    REQUIRE(tags != nullptr);
    CHECK(tags->Size() == 2);
    std::string_view second;
    CHECK(tags->At(1)->QueryString(&second));
    CHECK(second == "b");

    int64_t size = 0;
    CHECK(root->Find("size")->QueryInt(&size));
    CHECK(size == 3);

    double ratio = 0.0;
    CHECK(root->Find("ratio")->QueryDouble(&ratio));
    CHECK(ratio == 0.5);
}

TEST_CASE("Parse replaces the previous content of the document") {
    Document document;
    REQUIRE(json::Parse("[1,2,3]", document).Ok());
    REQUIRE(json::Parse("\"second\"", document).Ok());

    std::string_view text;
    CHECK(document.Root()->QueryString(&text));
    CHECK(text == "second");
}

TEST_CASE("a failed Parse reports the error and leaves the document empty") {
    Document document;
    REQUIRE(json::Parse("[1]", document).Ok());

    Error error = json::Parse("[1, 2,\n  oops]", document);
    CHECK(error.code == ErrorCode::UnexpectedChar);
    CHECK(error.line == 2);
    CHECK(error.column == 3);
    CHECK(document.Root() == nullptr);

    // The document is still usable afterwards.
    REQUIRE(json::Parse("true", document).Ok());
    CHECK(document.Root()->GetType() == Type::Bool);
}

TEST_CASE("Parse keeps repeated keys and Find returns the first") {
    Document document;
    REQUIRE(json::Parse("{\"a\":1,\"a\":2}", document).Ok());
    CHECK(document.Root()->Size() == 2);
    int64_t value = 0;
    CHECK(document.Root()->Find("a")->QueryInt(&value));
    CHECK(value == 1);
}

TEST_CASE("strings longer than an arena block and NUL bytes survive parsing") {
    std::string big(10000, 'x');
    Document document;
    REQUIRE(json::Parse("[\"" + big + "\",\"a\\u0000b\"]", document).Ok());

    std::string_view first;
    CHECK(document.Root()->At(0)->QueryString(&first));
    CHECK(first == std::string_view(big));

    std::string_view second;
    CHECK(document.Root()->At(1)->QueryString(&second));
    CHECK(second == std::string_view("a\0b", 3));
    CHECK(json::Stringify(*document.Root()->At(1)) == "\"a\\u0000b\"");
}

TEST_CASE("Stringify writes compact text by default and indented text on request") {
    Document document;
    Value* root = document.NewObject();
    root->Set("id", document.NewInt(7));
    Value* list = document.NewArray();
    list->Append(document.NewBool(true));
    list->Append(document.NewNull());
    root->Set("list", list);

    CHECK(json::Stringify(*root) == "{\"id\":7,\"list\":[true,null]}");

    json::WriterOptions options;
    options.indent = 2;
    CHECK(json::Stringify(*root, options) ==
        "{\n"
        "  \"id\": 7,\n"
        "  \"list\": [\n"
        "    true,\n"
        "    null\n"
        "  ]\n"
        "}");
}

TEST_CASE("Stringify works on any subtree") {
    Document document;
    REQUIRE(json::Parse("{\"inner\":[1,2]}", document).Ok());
    CHECK(json::Stringify(*document.Root()->Find("inner")) == "[1,2]");
}

TEST_CASE("Stringify returns an empty string for NaN and infinity") {
    Document document;
    CHECK(json::Stringify(*document.NewDouble(std::nan(""))).empty());

    Value* list = document.NewArray();
    list->Append(document.NewInt(1));
    list->Append(document.NewDouble(std::numeric_limits<double>::infinity()));
    CHECK(json::Stringify(*list).empty());
}

TEST_CASE("numbers keep their type and value through Stringify and Parse") {
    const char* texts[] = {
        "0", "-1", "9223372036854775807", "-9223372036854775808",
        "9223372036854775808", "18446744073709551615",
        "0.1", "-0.0", "1.0", "1e+100", "5e-324", "1.7976931348623157e+308",
    };
    for (const char* text : texts) {
        CAPTURE(text);
        Document document;
        REQUIRE(json::Parse(text, document).Ok());
        CHECK(json::Stringify(*document.Root()) == text);
    }
}

TEST_CASE("minus zero written as an integer comes back as a Double") {
    Document document;
    REQUIRE(json::Parse("-0", document).Ok());
    CHECK(document.Root()->GetType() == Type::Double);
    CHECK(json::Stringify(*document.Root()) == "-0.0");
}
