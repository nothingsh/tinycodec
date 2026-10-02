#include "doctest.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "recording_visitor.h"
#include "tinycodec/builder.h"
#include "tinycodec/document.h"
#include "tinycodec/value.h"

using namespace tinycodec;

TEST_CASE("a single scalar event becomes the root") {
    Document document;
    DocumentBuilder builder(document);
    CHECK(builder.Int(42));

    REQUIRE(document.Root() != nullptr);
    int64_t out = 0;
    CHECK(document.Root()->QueryInt(&out));
    CHECK(out == 42);
}

TEST_CASE("each scalar event creates a value of the matching type") {
    Document document;
    DocumentBuilder builder(document);
    CHECK(builder.EnterArray());
    CHECK(builder.Null());
    CHECK(builder.Bool(true));
    CHECK(builder.Int(-1));
    CHECK(builder.Uint(UINT64_MAX));
    CHECK(builder.Double(0.5));
    CHECK(builder.String("s"));
    CHECK(builder.ExitArray());

    const Value* root = document.Root();
    REQUIRE(root != nullptr);
    REQUIRE(root->Size() == 6);
    CHECK(root->At(0)->GetType() == Type::Null);
    CHECK(root->At(1)->GetType() == Type::Bool);
    CHECK(root->At(2)->GetType() == Type::Int);
    CHECK(root->At(3)->GetType() == Type::Uint);
    CHECK(root->At(4)->GetType() == Type::Double);
    CHECK(root->At(5)->GetType() == Type::String);
}

TEST_CASE("nested events become a nested tree") {
    Document document;
    DocumentBuilder builder(document);
    // {"list":[1,{"deep":null}],"name":"x"}
    CHECK(builder.EnterObject());
    CHECK(builder.Key("list"));
    CHECK(builder.EnterArray());
    CHECK(builder.Int(1));
    CHECK(builder.EnterObject());
    CHECK(builder.Key("deep"));
    CHECK(builder.Null());
    CHECK(builder.ExitObject());
    CHECK(builder.ExitArray());
    CHECK(builder.Key("name"));
    CHECK(builder.String("x"));
    CHECK(builder.ExitObject());

    const Value* root = document.Root();
    REQUIRE(root != nullptr);
    CHECK(root->GetType() == Type::Object);
    CHECK(root->Size() == 2);

    const Value* list = root->Find("list");
    REQUIRE(list != nullptr);
    CHECK(list->Size() == 2);
    REQUIRE(list->At(1) != nullptr);
    REQUIRE(list->At(1)->Find("deep") != nullptr);
    CHECK(list->At(1)->Find("deep")->GetType() == Type::Null);

    std::string_view name;
    REQUIRE(root->Find("name") != nullptr);
    CHECK(root->Find("name")->QueryString(&name));
    CHECK(name == "x");
}

TEST_CASE("the builder copies strings and keys out of the events") {
    Document document;
    DocumentBuilder builder(document);
    std::string buffer = "key";
    CHECK(builder.EnterObject());
    CHECK(builder.Key(buffer));
    buffer = "val";
    CHECK(builder.String(buffer));
    buffer = "###";
    CHECK(builder.ExitObject());

    const Value* member = document.Root()->FirstChild();
    REQUIRE(member != nullptr);
    CHECK(member->Key() == "key");
    std::string_view text;
    CHECK(member->QueryString(&text));
    CHECK(text == "val");
}

TEST_CASE("repeated keys are all kept, in order") {
    Document document;
    DocumentBuilder builder(document);
    CHECK(builder.EnterObject());
    CHECK(builder.Key("a"));
    CHECK(builder.Int(1));
    CHECK(builder.Key("a"));
    CHECK(builder.Int(2));
    CHECK(builder.ExitObject());

    const Value* root = document.Root();
    CHECK(root->Size() == 2);
    int64_t first = 0;
    int64_t second = 0;
    CHECK(root->FirstChild()->QueryInt(&first));
    CHECK(root->FirstChild()->Next()->QueryInt(&second));
    CHECK(first == 1);
    CHECK(second == 2);
    CHECK(root->Find("a") == root->FirstChild());
}

TEST_CASE("events after the root is complete are rejected") {
    Document document;

    SUBCASE("after a scalar root") {
        DocumentBuilder builder(document);
        CHECK(builder.Null());
        CHECK_FALSE(builder.Null());
        CHECK_FALSE(builder.EnterArray());
        CHECK(document.Root()->GetType() == Type::Null);
    }
    SUBCASE("after a container root") {
        DocumentBuilder builder(document);
        CHECK(builder.EnterArray());
        CHECK(builder.ExitArray());
        CHECK_FALSE(builder.Int(1));
        CHECK_FALSE(builder.ExitArray());
        CHECK(document.Root()->Size() == 0);
    }
}

TEST_CASE("malformed event sequences are rejected") {
    Document document;
    DocumentBuilder builder(document);

    SUBCASE("Key outside an object") {
        CHECK_FALSE(builder.Key("k"));
        CHECK(builder.EnterArray());
        CHECK_FALSE(builder.Key("k"));
    }
    SUBCASE("a value in an object without a Key") {
        CHECK(builder.EnterObject());
        CHECK_FALSE(builder.Int(1));
    }
    SUBCASE("two Keys in a row") {
        CHECK(builder.EnterObject());
        CHECK(builder.Key("a"));
        CHECK_FALSE(builder.Key("b"));
    }
    SUBCASE("Exit of the wrong kind") {
        CHECK(builder.EnterObject());
        CHECK_FALSE(builder.ExitArray());
    }
    SUBCASE("ExitObject while a Key is waiting for its value") {
        CHECK(builder.EnterObject());
        CHECK(builder.Key("a"));
        CHECK_FALSE(builder.ExitObject());
    }
}

TEST_CASE("building from Accept reproduces the tree") {
    Document source;
    Value* list = source.NewArray();
    list->Append(source.NewInt(1));
    list->Append(source.NewUint(UINT64_MAX));
    list->Append(source.NewDouble(2.5));
    Value* root = source.NewObject();
    root->Set("list", list);
    root->Set("", source.NewString(""));
    source.SetRoot(root);

    Document copy;
    DocumentBuilder builder(copy);
    CHECK(source.Root()->Accept(builder));

    REQUIRE(copy.Root() != nullptr);
    CHECK(copy.Root()->Equals(*source.Root()));
}

TEST_CASE("the root is installed only when the root value is complete") {
    Document document;
    Value* previous = document.NewString("previous");
    REQUIRE(document.SetRoot(previous));

    DocumentBuilder builder(document);
    CHECK(builder.EnterArray());
    CHECK(document.Root() == previous);
    CHECK(builder.Int(1));
    CHECK(builder.EnterObject());
    CHECK(builder.ExitObject());
    CHECK(document.Root() == previous);   // Still building: the old root stays.
    CHECK(builder.ExitArray());

    REQUIRE(document.Root() != previous);
    CHECK(document.Root()->GetType() == Type::Array);
    CHECK(document.Root()->Size() == 2);
}

TEST_CASE("a build that is never finished leaves the document as it was") {
    Document empty;
    DocumentBuilder first(empty);
    CHECK(first.EnterObject());
    CHECK(first.Key("a"));
    CHECK(empty.Root() == nullptr);

    Document filled;
    Value* previous = filled.NewInt(7);
    REQUIRE(filled.SetRoot(previous));
    DocumentBuilder second(filled);
    CHECK(second.EnterArray());
    CHECK(second.Int(1));
    CHECK_FALSE(second.ExitObject());     // The producer gave up with an error.
    CHECK(filled.Root() == previous);
}
