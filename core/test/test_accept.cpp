#include "doctest.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "datetimes.h"
#include "recording_visitor.h"
#include "tinycodec/document.h"
#include "tinycodec/value.h"
#include "tinycodec/visitor.h"

using namespace tinycodec;

namespace {

using Events = std::vector<std::string>;

// {"name":"x","list":[1,null,true],"empty":{}}
Value* BuildSample(Document& document) {
    Value* list = document.NewArray();
    list->Append(document.NewInt(1));
    list->Append(document.NewNull());
    list->Append(document.NewBool(true));

    Value* root = document.NewObject();
    root->Set("name", document.NewString("x"));
    root->Set("list", list);
    root->Set("empty", document.NewObject());
    return root;
}

}  // namespace

TEST_CASE("Accept replays scalars as single events") {
    Document document;
    RecordingVisitor visitor;
    CHECK(document.NewNull()->Accept(visitor));
    CHECK(document.NewBool(false)->Accept(visitor));
    CHECK(document.NewInt(-5)->Accept(visitor));
    CHECK(document.NewDouble(1.5)->Accept(visitor));
    CHECK(document.NewString("hi")->Accept(visitor));
    CHECK(visitor.events == Events{"Null", "Bool(false)", "Int(-5)", "Double(1.500000)", "String(hi)"});
}

TEST_CASE("Accept sends Uint only for values above INT64_MAX") {
    Document document;
    RecordingVisitor visitor;
    CHECK(document.NewUint(5)->Accept(visitor));
    CHECK(document.NewUint(static_cast<uint64_t>(INT64_MAX))->Accept(visitor));
    CHECK(document.NewUint(static_cast<uint64_t>(INT64_MAX) + 1)->Accept(visitor));
    CHECK(visitor.events == Events{"Int(5)", "Int(9223372036854775807)", "Uint(9223372036854775808)"});
}

TEST_CASE("Accept replays Bytes as a Bytes event") {
    Document document;
    RecordingVisitor visitor;
    CHECK(document.NewBytes(std::string("\x00\xFF", 2))->Accept(visitor));
    CHECK(document.NewBytes("")->Accept(visitor));
    CHECK(visitor.events == Events{"Bytes(00ff)", "Bytes()"});
}

TEST_CASE("a Visitor that does not override Bytes or DateTime rejects them") {
    // Implements only the pure virtual events, each of which accepts.
    class BasicVisitor : public Visitor {
    public:
        bool Null() override { return true; }
        bool Bool(bool) override { return true; }
        bool Int(int64_t) override { return true; }
        bool Uint(uint64_t) override { return true; }
        bool Double(double) override { return true; }
        bool String(std::string_view) override { return true; }
        bool EnterObject() override { return true; }
        bool Key(std::string_view) override { return true; }
        bool ExitObject() override { return true; }
        bool EnterArray() override { return true; }
        bool ExitArray() override { return true; }
    };

    BasicVisitor visitor;
    CHECK_FALSE(visitor.Bytes("x"));
    CHECK_FALSE(visitor.DateTime(MakeDate(2023, 1, 1)));

    Document document;
    Value* list = document.NewArray();
    list->Append(document.NewInt(1));
    list->Append(document.NewBytes("x"));
    CHECK_FALSE(list->Accept(visitor));

    Value* dates = document.NewArray();
    dates->Append(document.NewInt(1));
    dates->Append(document.NewDateTime(MakeDate(2023, 1, 1)));
    CHECK_FALSE(dates->Accept(visitor));
}

TEST_CASE("Accept replays a DateTime as a DateTime event") {
    Document document;
    RecordingVisitor visitor;
    CHECK(document.NewDateTime(MakeOffsetDateTime(1979, 5, 27, 0, 32, 0, 999000000, -420))->Accept(visitor));
    CHECK(document.NewDateTime(MakeDate(1979, 5, 27))->Accept(visitor));
    CHECK(document.NewDateTime(MakeTime(7, 32, 0))->Accept(visitor));
    CHECK(document.NewDateTime(MakeLocalDateTime(1979, 5, 27, 7, 32, 0, 500000000))->Accept(visitor));
    CHECK(visitor.events == Events{
        "DateTime(1979-05-27T00:32:00.999-07:00)",
        "DateTime(1979-05-27)",
        "DateTime(07:32:00)",
        "DateTime(1979-05-27T07:32:00.5)"});
}

TEST_CASE("Accept replays containers in document order") {
    Document document;
    RecordingVisitor visitor;
    CHECK(BuildSample(document)->Accept(visitor));
    CHECK(visitor.events == Events{
        "EnterObject",
        "Key(name)", "String(x)",
        "Key(list)", "EnterArray", "Int(1)", "Null", "Bool(true)", "ExitArray",
        "Key(empty)", "EnterObject", "ExitObject",
        "ExitObject"});
}

TEST_CASE("Accept stops at the first event that returns false") {
    Document document;
    Value* sample = BuildSample(document);

    RecordingVisitor complete;
    sample->Accept(complete);
    int total = static_cast<int>(complete.events.size());

    for (int failAt = 0; failAt < total; ++failAt) {
        RecordingVisitor visitor;
        visitor.failAt = failAt;
        CHECK_FALSE(sample->Accept(visitor));
        // The failing event is the last one delivered.
        CHECK(static_cast<int>(visitor.events.size()) == failAt + 1);
    }
}
