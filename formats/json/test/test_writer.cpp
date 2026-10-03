#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "datetimes.h"
#include "tinycodec/document.h"
#include "tinycodec/json/writer.h"
#include "tinycodec/sink.h"
#include "tinycodec/value.h"

using namespace tinycodec;

namespace {

json::WriterOptions Indent(int spaces) {
    json::WriterOptions options;
    options.indent = spaces;
    return options;
}

std::string WriteDouble(double value) {
    StringSink sink;
    json::Writer writer(sink);
    REQUIRE(writer.Double(value));
    return sink.Str();
}

std::string WriteString(std::string_view value) {
    StringSink sink;
    json::Writer writer(sink);
    REQUIRE(writer.String(value));
    return sink.Str();
}

// Accepts `budget` bytes in total, then fails every write.
class FailingSink : public Sink {
public:
    explicit FailingSink(size_t budget) : _budget(budget) {}

    bool Write(std::string_view data) override {
        if (data.size() > _budget) {
            return false;
        }
        _budget -= data.size();
        return true;
    }

private:
    size_t _budget;
};

// {"name":"x","list":[1,[],{}],"nested":{"k":null}}
bool DriveSample(Visitor& visitor) {
    return visitor.EnterObject()
        && visitor.Key("name") && visitor.String("x")
        && visitor.Key("list") && visitor.EnterArray()
        && visitor.Int(1)
        && visitor.EnterArray() && visitor.ExitArray()
        && visitor.EnterObject() && visitor.ExitObject()
        && visitor.ExitArray()
        && visitor.Key("nested") && visitor.EnterObject()
        && visitor.Key("k") && visitor.Null()
        && visitor.ExitObject()
        && visitor.ExitObject();
}

}  // namespace

TEST_CASE("scalars are written as JSON literals") {
    StringSink sink;
    json::Writer writer(sink);
    CHECK(writer.EnterArray());
    CHECK(writer.Null());
    CHECK(writer.Bool(true));
    CHECK(writer.Bool(false));
    CHECK(writer.Int(0));
    CHECK(writer.Int(INT64_MIN));
    CHECK(writer.Uint(UINT64_MAX));
    CHECK(writer.String("hi"));
    CHECK(writer.ExitArray());
    CHECK(sink.Str() == "[null,true,false,0,-9223372036854775808,18446744073709551615,\"hi\"]");
}

TEST_CASE("a scalar can be the root") {
    StringSink sink;
    json::Writer writer(sink);
    CHECK(writer.Int(7));
    CHECK(sink.Str() == "7");
}

TEST_CASE("compact output contains no whitespace") {
    StringSink sink;
    json::Writer writer(sink);
    CHECK(DriveSample(writer));
    CHECK(sink.Str() == "{\"name\":\"x\",\"list\":[1,[],{}],\"nested\":{\"k\":null}}");
}

TEST_CASE("indented output puts each element on its own line") {
    StringSink sink;
    json::Writer writer(sink, Indent(2));
    CHECK(DriveSample(writer));
    CHECK(sink.Str() ==
        "{\n"
        "  \"name\": \"x\",\n"
        "  \"list\": [\n"
        "    1,\n"
        "    [],\n"
        "    {}\n"
        "  ],\n"
        "  \"nested\": {\n"
        "    \"k\": null\n"
        "  }\n"
        "}");
}

TEST_CASE("the indent width is configurable and empty roots stay on one line") {
    StringSink wide;
    json::Writer wideWriter(wide, Indent(4));
    CHECK(wideWriter.EnterArray());
    CHECK(wideWriter.Int(1));
    CHECK(wideWriter.ExitArray());
    CHECK(wide.Str() == "[\n    1\n]");

    StringSink emptyArray;
    json::Writer arrayWriter(emptyArray, Indent(2));
    CHECK(arrayWriter.EnterArray());
    CHECK(arrayWriter.ExitArray());
    CHECK(emptyArray.Str() == "[]");

    StringSink emptyObject;
    json::Writer objectWriter(emptyObject, Indent(2));
    CHECK(objectWriter.EnterObject());
    CHECK(objectWriter.ExitObject());
    CHECK(emptyObject.Str() == "{}");
}

TEST_CASE("a negative indent means compact") {
    StringSink sink;
    json::Writer writer(sink, Indent(-3));
    CHECK(writer.EnterArray());
    CHECK(writer.Int(1));
    CHECK(writer.Int(2));
    CHECK(writer.ExitArray());
    CHECK(sink.Str() == "[1,2]");
}

TEST_CASE("strings are escaped") {
    CHECK(WriteString("") == "\"\"");
    CHECK(WriteString("plain") == "\"plain\"");
    CHECK(WriteString("say \"hi\"") == "\"say \\\"hi\\\"\"");
    CHECK(WriteString("back\\slash") == "\"back\\\\slash\"");
    CHECK(WriteString("\b\f\n\r\t") == "\"\\b\\f\\n\\r\\t\"");
    CHECK(WriteString(std::string_view("\x01\x1f", 2)) == "\"\\u0001\\u001f\"");
    CHECK(WriteString(std::string_view("a\0b", 3)) == "\"a\\u0000b\"");
    CHECK(WriteString("a/b") == "\"a/b\"");
    CHECK(WriteString("\x7f") == "\"\x7f\"");
}

TEST_CASE("non-ASCII bytes are passed through unchanged") {
    CHECK(WriteString("\xE4\xBD\xA0\xE5\xA5\xBD") == "\"\xE4\xBD\xA0\xE5\xA5\xBD\"");
    CHECK(WriteString("\xF0\x9F\x98\x80") == "\"\xF0\x9F\x98\x80\"");
}

TEST_CASE("keys are escaped like strings") {
    StringSink sink;
    json::Writer writer(sink);
    CHECK(writer.EnterObject());
    CHECK(writer.Key("a\"b\n"));
    CHECK(writer.Null());
    CHECK(writer.Key(""));
    CHECK(writer.Null());
    CHECK(writer.ExitObject());
    CHECK(sink.Str() == "{\"a\\\"b\\n\":null,\"\":null}");
}

TEST_CASE("doubles use the shortest text that reads back identically") {
    CHECK(WriteDouble(0.1) == "0.1");
    CHECK(WriteDouble(-2.5) == "-2.5");
    CHECK(WriteDouble(1e100) == "1e+100");
    CHECK(WriteDouble(1.5e-7) == "1.5e-07");
    CHECK(WriteDouble(std::numeric_limits<double>::max()) == "1.7976931348623157e+308");
    CHECK(WriteDouble(std::numeric_limits<double>::denorm_min()) == "5e-324");
}

TEST_CASE("a double without a fraction still looks like a double") {
    CHECK(WriteDouble(1.0) == "1.0");
    CHECK(WriteDouble(0.0) == "0.0");
    CHECK(WriteDouble(-0.0) == "-0.0");
    CHECK(WriteDouble(123456.0) == "123456.0");
}

TEST_CASE("NaN and infinity are rejected and nothing is written") {
    StringSink sink;
    json::Writer writer(sink);
    CHECK(writer.EnterArray());
    CHECK(writer.Int(1));
    CHECK_FALSE(writer.Double(std::nan("")));
    CHECK_FALSE(writer.Double(std::numeric_limits<double>::infinity()));
    CHECK_FALSE(writer.Double(-std::numeric_limits<double>::infinity()));
    CHECK(sink.Str() == "[1");
}

TEST_CASE("Bytes are rejected and nothing is written") {
    StringSink sink;
    json::Writer writer(sink);
    CHECK_FALSE(writer.Bytes("x"));
    CHECK(sink.Str().empty());

    CHECK(writer.EnterArray());
    CHECK(writer.Int(1));
    CHECK_FALSE(writer.Bytes(""));
    CHECK(sink.Str() == "[1");
}

TEST_CASE("events after the root is complete are rejected") {
    SUBCASE("after a scalar root") {
        StringSink sink;
        json::Writer writer(sink);
        CHECK(writer.Null());
        CHECK_FALSE(writer.Null());
        CHECK_FALSE(writer.EnterArray());
        CHECK(sink.Str() == "null");
    }
    SUBCASE("after a container root") {
        StringSink sink;
        json::Writer writer(sink);
        CHECK(writer.EnterArray());
        CHECK(writer.ExitArray());
        CHECK_FALSE(writer.Int(1));
        CHECK_FALSE(writer.ExitArray());
        CHECK(sink.Str() == "[]");
    }
}

TEST_CASE("events that do not fit the sequence are rejected") {
    StringSink sink;
    json::Writer writer(sink);

    SUBCASE("Key outside an object") {
        CHECK_FALSE(writer.Key("k"));
        CHECK(writer.EnterArray());
        CHECK_FALSE(writer.Key("k"));
    }
    SUBCASE("a value in an object without a Key") {
        CHECK(writer.EnterObject());
        CHECK_FALSE(writer.Int(1));
        CHECK_FALSE(writer.EnterArray());
    }
    SUBCASE("two Keys in a row") {
        CHECK(writer.EnterObject());
        CHECK(writer.Key("a"));
        CHECK_FALSE(writer.Key("b"));
    }
    SUBCASE("Exit of the wrong kind") {
        CHECK(writer.EnterObject());
        CHECK_FALSE(writer.ExitArray());
    }
    SUBCASE("ExitObject while a Key is waiting for its value") {
        CHECK(writer.EnterObject());
        CHECK(writer.Key("a"));
        CHECK_FALSE(writer.ExitObject());
    }
    SUBCASE("Exit with nothing open") {
        CHECK_FALSE(writer.ExitArray());
        CHECK_FALSE(writer.ExitObject());
    }
}

TEST_CASE("a failing sink makes the event fail, wherever the failure happens") {
    StringSink complete;
    json::Writer reference(complete, Indent(2));
    REQUIRE(DriveSample(reference));
    size_t total = complete.Str().size();

    for (size_t budget = 0; budget < total; ++budget) {
        FailingSink sink(budget);
        json::Writer writer(sink, Indent(2));
        CHECK_FALSE(DriveSample(writer));
    }

    FailingSink enough(total);
    json::Writer writer(enough, Indent(2));
    CHECK(DriveSample(writer));
}

TEST_CASE("Value::Accept can drive the writer") {
    Document document;
    Value* list = document.NewArray();
    list->Append(document.NewInt(1));
    list->Append(document.NewDouble(2.0));
    list->Append(document.NewUint(UINT64_MAX));
    Value* root = document.NewObject();
    root->Set("list", list);
    root->Set("ok", document.NewBool(true));

    StringSink sink;
    json::Writer writer(sink);
    CHECK(root->Accept(writer));
    CHECK(sink.Str() == "{\"list\":[1,2.0,18446744073709551615],\"ok\":true}");
}

TEST_CASE("500 levels of nesting are written correctly") {
    StringSink sink;
    json::Writer writer(sink);
    for (int i = 0; i < 500; ++i) {
        REQUIRE(writer.EnterArray());
    }
    for (int i = 0; i < 500; ++i) {
        REQUIRE(writer.ExitArray());
    }
    CHECK(sink.Str() == std::string(500, '[') + std::string(500, ']'));
}

TEST_CASE("a DateTime is rejected and nothing is written") {
    StringSink sink;
    json::Writer writer(sink);
    CHECK_FALSE(writer.DateTime(MakeDate(2023, 1, 1)));
    CHECK(sink.Str().empty());

    CHECK(writer.EnterArray());
    CHECK(writer.Int(1));
    CHECK_FALSE(writer.DateTime(MakeTime(1, 2, 3)));
    CHECK(sink.Str() == "[1");
}
