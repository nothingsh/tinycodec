#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>

#include "datetimes.h"
#include "tinycodec/sink.h"
#include "tinycodec/toml/writer.h"

using namespace tinycodec;

namespace {

// Remembers every Write call. Fails all of them when `fail` is set.
class RecordingSink : public Sink {
public:
    std::string data;
    int calls = 0;
    bool fail = false;

    bool Write(std::string_view text) override {
        ++calls;
        if (fail) {
            return false;
        }
        data.append(text.data(), text.size());
        return true;
    }
};

// Drives a fresh Writer with `events`, which must succeed, and returns the
// output.
std::string Written(const std::function<bool(Visitor&)>& events) {
    StringSink sink;
    toml::Writer writer(sink);
    REQUIRE(events(writer));
    return sink.Str();
}

// Writes {"v": <value>} and returns the text after "v = ", without the
// final line break.
std::string WrittenValue(const std::function<bool(Visitor&)>& value) {
    std::string text = Written([&](Visitor& v) {
        return v.EnterObject() && v.Key("v") && value(v) && v.ExitObject();
    });
    REQUIRE(text.size() >= 5);
    REQUIRE(text.compare(0, 4, "v = ") == 0);
    REQUIRE(text.back() == '\n');
    return text.substr(4, text.size() - 5);
}

std::string WrittenDouble(double value) {
    return WrittenValue([value](Visitor& v) { return v.Double(value); });
}

std::string WrittenString(std::string_view value) {
    return WrittenValue([value](Visitor& v) { return v.String(value); });
}

std::string WrittenDateTime(const DateTime& value) {
    return WrittenValue([value](Visitor& v) { return v.DateTime(value); });
}

// Writes {<key>: 1} and returns the text before " = 1\n".
std::string WrittenKey(std::string_view key) {
    std::string text = Written([key](Visitor& v) {
        return v.EnterObject() && v.Key(key) && v.Int(1) && v.ExitObject();
    });
    std::string suffix = " = 1\n";
    REQUIRE(text.size() > suffix.size());
    REQUIRE(text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0);
    return text.substr(0, text.size() - suffix.size());
}

}  // namespace

TEST_CASE("an empty root table writes nothing") {
    CHECK(Written([](Visitor& v) { return v.EnterObject() && v.ExitObject(); }) == "");
}

TEST_CASE("each scalar is written on a line of its own") {
    CHECK(Written([](Visitor& v) {
        return v.EnterObject()
            && v.Key("s") && v.String("x")
            && v.Key("i") && v.Int(-42)
            && v.Key("f") && v.Double(1.5)
            && v.Key("b") && v.Bool(true)
            && v.Key("c") && v.Bool(false)
            && v.Key("d") && v.DateTime(MakeDate(1979, 5, 27))
            && v.ExitObject();
    }) == "s = \"x\"\n"
          "i = -42\n"
          "f = 1.5\n"
          "b = true\n"
          "c = false\n"
          "d = 1979-05-27\n");
}

TEST_CASE("integers cover the whole int64 range") {
    CHECK(WrittenValue([](Visitor& v) { return v.Int(0); }) == "0");
    CHECK(WrittenValue([](Visitor& v) { return v.Int(INT64_MAX); }) == "9223372036854775807");
    CHECK(WrittenValue([](Visitor& v) { return v.Int(INT64_MIN); }) == "-9223372036854775808");
}

TEST_CASE("doubles are written in the shortest form that reads back as a float") {
    CHECK(WrittenDouble(0.5) == "0.5");
    CHECK(WrittenDouble(1.0) == "1.0");
    CHECK(WrittenDouble(-0.0) == "-0.0");
    CHECK(WrittenDouble(1e300) == "1e+300");
    CHECK(WrittenDouble(5e-324) == "5e-324");
    CHECK(WrittenDouble(std::numeric_limits<double>::quiet_NaN()) == "nan");
    CHECK(WrittenDouble(-std::numeric_limits<double>::quiet_NaN()) == "nan");
    CHECK(WrittenDouble(std::numeric_limits<double>::infinity()) == "inf");
    CHECK(WrittenDouble(-std::numeric_limits<double>::infinity()) == "-inf");
}

TEST_CASE("strings are basic strings with escapes") {
    CHECK(WrittenString("") == "\"\"");
    CHECK(WrittenString("plain text") == "\"plain text\"");
    CHECK(WrittenString("\"\\") == "\"\\\"\\\\\"");
    CHECK(WrittenString("\b\t\n\f\r") == "\"\\b\\t\\n\\f\\r\"");
    CHECK(WrittenString(std::string("\0\x01\x1F\x7F", 4)) == "\"\\u0000\\u0001\\u001F\\u007F\"");
    CHECK(WrittenString("\xE4\xBD\xA0 'single'") == "\"\xE4\xBD\xA0 'single'\"");
}

TEST_CASE("keys are bare when they can be, quoted otherwise") {
    CHECK(WrittenKey("name") == "name");
    CHECK(WrittenKey("A-z_0-9") == "A-z_0-9");
    CHECK(WrittenKey("1234") == "1234");
    CHECK(WrittenKey("") == "\"\"");
    CHECK(WrittenKey("a.b") == "\"a.b\"");
    CHECK(WrittenKey("with space") == "\"with space\"");
    CHECK(WrittenKey("quote\"") == "\"quote\\\"\"");
    CHECK(WrittenKey("\xC3\xA9") == "\"\xC3\xA9\"");
}

TEST_CASE("datetimes are written in RFC 3339 form") {
    CHECK(WrittenDateTime(MakeOffsetDateTime(1979, 5, 27, 7, 32, 0, 0, 0)) == "1979-05-27T07:32:00Z");
    CHECK(WrittenDateTime(MakeOffsetDateTime(1979, 5, 27, 0, 32, 0, 999999000, -420))
          == "1979-05-27T00:32:00.999999-07:00");
    CHECK(WrittenDateTime(MakeOffsetDateTime(1, 1, 1, 0, 0, 0, 1, 1439)) == "0001-01-01T00:00:00.000000001+23:59");
    CHECK(WrittenDateTime(MakeLocalDateTime(1979, 5, 27, 7, 32, 0)) == "1979-05-27T07:32:00");
    CHECK(WrittenDateTime(MakeLocalDateTime(1979, 5, 27, 7, 32, 0, 500000000)) == "1979-05-27T07:32:00.5");
    CHECK(WrittenDateTime(MakeDate(2000, 2, 29)) == "2000-02-29");
    CHECK(WrittenDateTime(MakeTime(23, 59, 60)) == "23:59:60");
    CHECK(WrittenDateTime(MakeTime(0, 32, 0, 120000000)) == "00:32:00.12");
}

TEST_CASE("arrays are written on one line, nested and mixed") {
    CHECK(WrittenValue([](Visitor& v) { return v.EnterArray() && v.ExitArray(); }) == "[]");
    CHECK(WrittenValue([](Visitor& v) {
        return v.EnterArray() && v.Int(1) && v.String("x") && v.EnterArray() && v.Int(2) && v.ExitArray()
            && v.EnterArray() && v.ExitArray() && v.ExitArray();
    }) == "[1, \"x\", [2], []]");
}

TEST_CASE("objects inside arrays are inline tables") {
    CHECK(WrittenValue([](Visitor& v) {
        return v.EnterArray() && v.Int(1)
            && v.EnterObject() && v.Key("a") && v.Int(1)
                && v.Key("b") && v.EnterObject() && v.Key("c") && v.Int(2) && v.ExitObject()
                && v.Key("d") && v.String("x") && v.ExitObject()
            && v.EnterObject() && v.ExitObject()
            && v.ExitArray();
    }) == "[1, {a = 1, b = {c = 2}, d = \"x\"}, {}]");
}

TEST_CASE("an object member becomes a table section after the plain members") {
    CHECK(Written([](Visitor& v) {
        return v.EnterObject()
            && v.Key("server") && v.EnterObject() && v.Key("host") && v.String("x") && v.ExitObject()
            && v.Key("name") && v.String("app")
            && v.ExitObject();
    }) == "name = \"app\"\n"
          "\n"
          "[server]\n"
          "host = \"x\"\n");
}

TEST_CASE("nested tables give dotted headers, and sections are separated by a blank line") {
    CHECK(Written([](Visitor& v) {
        return v.EnterObject()
            && v.Key("a") && v.EnterObject()
                && v.Key("x") && v.Int(1)
                && v.Key("b c") && v.EnterObject() && v.Key("y") && v.Int(2) && v.ExitObject()
                && v.ExitObject()
            && v.Key("d") && v.EnterObject() && v.Key("z") && v.Int(3) && v.ExitObject()
            && v.ExitObject();
    }) == "[a]\n"
          "x = 1\n"
          "\n"
          "[a.\"b c\"]\n"
          "y = 2\n"
          "\n"
          "[d]\n"
          "z = 3\n");
}

TEST_CASE("a table with only subtables gets no header of its own") {
    CHECK(Written([](Visitor& v) {
        return v.EnterObject()
            && v.Key("a") && v.EnterObject()
                && v.Key("b") && v.EnterObject() && v.Key("x") && v.Int(1) && v.ExitObject()
                && v.ExitObject()
            && v.ExitObject();
    }) == "[a.b]\n"
          "x = 1\n");
}

TEST_CASE("an empty table still gets its header") {
    CHECK(Written([](Visitor& v) {
        return v.EnterObject()
            && v.Key("a") && v.EnterObject()
                && v.Key("empty") && v.EnterObject() && v.ExitObject()
                && v.ExitObject()
            && v.ExitObject();
    }) == "[a.empty]\n");
}

TEST_CASE("an array of objects becomes an array of tables") {
    CHECK(Written([](Visitor& v) {
        return v.EnterObject()
            && v.Key("item") && v.EnterArray()
                && v.EnterObject() && v.Key("x") && v.Int(1) && v.ExitObject()
                && v.EnterObject() && v.ExitObject()
                && v.EnterObject()
                    && v.Key("sub") && v.EnterObject() && v.Key("y") && v.Int(2) && v.ExitObject()
                    && v.ExitObject()
                && v.ExitArray()
            && v.ExitObject();
    }) == "[[item]]\n"
          "x = 1\n"
          "\n"
          "[[item]]\n"
          "\n"
          "[[item]]\n"
          "\n"
          "[item.sub]\n"
          "y = 2\n");
}

TEST_CASE("an array that is empty or not all objects stays a plain member") {
    CHECK(Written([](Visitor& v) {
        return v.EnterObject()
            && v.Key("t") && v.EnterObject() && v.ExitObject()
            && v.Key("empty") && v.EnterArray() && v.ExitArray()
            && v.Key("mixed") && v.EnterArray() && v.EnterObject() && v.ExitObject() && v.Int(1) && v.ExitArray()
            && v.ExitObject();
    }) == "empty = []\n"
          "mixed = [{}, 1]\n"
          "\n"
          "[t]\n");
}

TEST_CASE("the root must be an object") {
    StringSink sink;
    toml::Writer writer(sink);
    CHECK_FALSE(writer.Int(1));
    CHECK_FALSE(writer.String("x"));
    CHECK_FALSE(writer.EnterArray());
    CHECK_FALSE(writer.ExitObject());
    CHECK(writer.EnterObject());
    CHECK(writer.ExitObject());
    CHECK(sink.Str() == "");
}

TEST_CASE("values TOML cannot express are rejected without changing the state") {
    RecordingSink sink;
    toml::Writer writer(sink);
    CHECK(writer.EnterObject());
    CHECK(writer.Key("a"));
    CHECK_FALSE(writer.Null());
    CHECK_FALSE(writer.Bytes("x"));
    CHECK_FALSE(writer.Uint(static_cast<uint64_t>(INT64_MAX) + 1));
    CHECK_FALSE(writer.DateTime(MakeDate(2023, 2, 30)));
    // The key is still waiting for its value.
    CHECK(writer.Int(1));
    CHECK(writer.ExitObject());
    CHECK(sink.data == "a = 1\n");
}

TEST_CASE("events that do not fit the sequence are rejected and nothing reaches the sink") {
    RecordingSink sink;
    toml::Writer writer(sink);
    CHECK(writer.EnterObject());
    CHECK_FALSE(writer.Int(1));          // No key.
    CHECK_FALSE(writer.ExitArray());     // Not in an array.
    CHECK(writer.Key("a"));
    CHECK_FALSE(writer.Key("b"));        // Two keys in a row.
    CHECK_FALSE(writer.ExitObject());    // The key has no value.
    CHECK(writer.EnterArray());
    CHECK_FALSE(writer.Key("c"));        // A key in an array.
    CHECK(writer.ExitArray());
    CHECK(sink.calls == 0);
    CHECK(writer.ExitObject());
    CHECK(sink.data == "a = []\n");
}

TEST_CASE("events after the root is complete are rejected") {
    RecordingSink sink;
    toml::Writer writer(sink);
    CHECK(writer.EnterObject());
    CHECK(writer.ExitObject());
    CHECK_FALSE(writer.EnterObject());
    CHECK_FALSE(writer.Key("a"));
    CHECK(sink.calls == 1);
}

TEST_CASE("nesting is limited to kMaxDepth levels") {
    // The root table with `levels - 1` arrays nested inside it.
    auto nest = [](Visitor& v, int levels) {
        bool ok = v.EnterObject() && v.Key("a");
        for (int i = 1; ok && i < levels; ++i) {
            ok = v.EnterArray();
        }
        return ok;
    };

    StringSink sink;
    toml::Writer writer(sink);
    CHECK(nest(writer, toml::kMaxDepth));
    CHECK_FALSE(writer.EnterArray());
    CHECK_FALSE(writer.EnterObject());
    // The rejected events changed nothing: the arrays still close.
    bool ok = true;
    for (int i = 1; ok && i < toml::kMaxDepth; ++i) {
        ok = writer.ExitArray();
    }
    CHECK(ok);
    CHECK(writer.ExitObject());
    CHECK(sink.Str().size() == 4 + 2 * (toml::kMaxDepth - 1) + 1);
}

TEST_CASE("repeated keys make the event that completes the root fail") {
    SUBCASE("in a table") {
        RecordingSink sink;
        toml::Writer writer(sink);
        CHECK(writer.EnterObject());
        CHECK(writer.Key("a"));
        CHECK(writer.Int(1));
        CHECK(writer.Key("a"));
        CHECK(writer.Int(2));
        CHECK_FALSE(writer.ExitObject());
        CHECK(sink.calls == 0);
    }
    SUBCASE("in an inline table") {
        RecordingSink sink;
        toml::Writer writer(sink);
        CHECK(writer.EnterObject());
        CHECK(writer.Key("list"));
        CHECK(writer.EnterArray());
        CHECK(writer.Int(0));
        CHECK(writer.EnterObject());
        CHECK(writer.Key("b"));
        CHECK(writer.Int(1));
        CHECK(writer.Key("b"));
        CHECK(writer.Int(2));
        CHECK(writer.ExitObject());
        CHECK(writer.ExitArray());
        CHECK_FALSE(writer.ExitObject());
        CHECK(sink.calls == 0);
    }
}

TEST_CASE("the output reaches the sink in one Write, when the root is complete") {
    RecordingSink sink;
    toml::Writer writer(sink);
    CHECK(writer.EnterObject());
    CHECK(writer.Key("a"));
    CHECK(writer.EnterObject());
    CHECK(writer.Key("b"));
    CHECK(writer.Int(1));
    CHECK(writer.ExitObject());
    CHECK(sink.calls == 0);
    CHECK(writer.ExitObject());
    CHECK(sink.calls == 1);
    CHECK(sink.data == "[a]\nb = 1\n");
}

TEST_CASE("a failing sink makes the event that completes the root fail") {
    RecordingSink sink;
    sink.fail = true;
    toml::Writer writer(sink);
    CHECK(writer.EnterObject());
    CHECK(writer.Key("a"));
    CHECK(writer.Int(1));
    CHECK_FALSE(writer.ExitObject());
    CHECK(sink.calls == 1);
}
