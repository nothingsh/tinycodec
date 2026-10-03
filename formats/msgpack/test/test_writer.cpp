#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>

#include "hex.h"
#include "tinycodec/document.h"
#include "tinycodec/msgpack/writer.h"
#include "tinycodec/sink.h"
#include "tinycodec/value.h"

using namespace tinycodec;

namespace {

// Remembers every Write call. Fails all of them when `fail` is set.
class RecordingSink : public Sink {
public:
    std::string data;
    int calls = 0;
    bool fail = false;

    bool Write(std::string_view bytes) override {
        ++calls;
        if (fail) {
            return false;
        }
        data.append(bytes.data(), bytes.size());
        return true;
    }
};

// Drives a fresh Writer with `events`, which must succeed, and returns the
// output in hex.
std::string Written(const std::function<bool(Visitor&)>& events) {
    StringSink sink;
    msgpack::Writer writer(sink);
    REQUIRE(events(writer));
    return ToHex(sink.Str());
}

std::string WrittenInt(int64_t value) {
    return Written([value](Visitor& visitor) { return visitor.Int(value); });
}

std::string WrittenUint(uint64_t value) {
    return Written([value](Visitor& visitor) { return visitor.Uint(value); });
}

std::string WrittenDouble(double value) {
    return Written([value](Visitor& visitor) { return visitor.Double(value); });
}

// Writes a String (or Bytes) of `length` bytes and returns the output as
// raw bytes.
std::string WrittenText(size_t length, bool asBytes) {
    std::string value(length, 'x');
    StringSink sink;
    msgpack::Writer writer(sink);
    REQUIRE((asBytes ? writer.Bytes(value) : writer.String(value)));
    return sink.Str();
}

// Writes an array, or an object, of `count` nulls and returns the output as
// raw bytes.
std::string WrittenContainer(size_t count, bool isObject) {
    StringSink sink;
    msgpack::Writer writer(sink);
    bool ok = isObject ? writer.EnterObject() : writer.EnterArray();
    for (size_t i = 0; i < count; ++i) {
        ok = ok && (!isObject || writer.Key("")) && writer.Null();
    }
    ok = ok && (isObject ? writer.ExitObject() : writer.ExitArray());
    REQUIRE(ok);
    return sink.Str();
}

// Checks that `output` starts with the header given in hex and is followed
// by `contentSize` more bytes.
void CheckHeader(const std::string& output, const std::string& header, size_t contentSize) {
    std::string expected = FromHex(header);
    CHECK(ToHex(output.substr(0, expected.size())) == ToHex(expected));
    CHECK(output.size() == expected.size() + contentSize);
}

}  // namespace

TEST_CASE("nil and booleans") {
    CHECK(Written([](Visitor& v) { return v.Null(); }) == "c0");
    CHECK(Written([](Visitor& v) { return v.Bool(false); }) == "c2");
    CHECK(Written([](Visitor& v) { return v.Bool(true); }) == "c3");
}

TEST_CASE("non-negative integers use the shortest unsigned form") {
    CHECK(WrittenInt(0) == "00");
    CHECK(WrittenInt(127) == "7f");
    CHECK(WrittenInt(128) == "cc80");
    CHECK(WrittenInt(255) == "ccff");
    CHECK(WrittenInt(256) == "cd0100");
    CHECK(WrittenInt(65535) == "cdffff");
    CHECK(WrittenInt(65536) == "ce00010000");
    CHECK(WrittenInt(4294967295) == "ceffffffff");
    CHECK(WrittenInt(4294967296) == "cf0000000100000000");
    CHECK(WrittenInt(INT64_MAX) == "cf7fffffffffffffff");
    CHECK(WrittenUint(static_cast<uint64_t>(INT64_MAX) + 1) == "cf8000000000000000");
    CHECK(WrittenUint(UINT64_MAX) == "cfffffffffffffffff");
}

TEST_CASE("a non-negative integer is written the same way as Int and as Uint") {
    for (uint64_t value : {0ULL, 127ULL, 128ULL, 65536ULL, 4294967296ULL, 9223372036854775807ULL}) {
        CAPTURE(value);
        CHECK(WrittenInt(static_cast<int64_t>(value)) == WrittenUint(value));
    }
}

TEST_CASE("negative integers use the shortest signed form") {
    CHECK(WrittenInt(-1) == "ff");
    CHECK(WrittenInt(-32) == "e0");
    CHECK(WrittenInt(-33) == "d0df");
    CHECK(WrittenInt(-128) == "d080");
    CHECK(WrittenInt(-129) == "d1ff7f");
    CHECK(WrittenInt(-32768) == "d18000");
    CHECK(WrittenInt(-32769) == "d2ffff7fff");
    CHECK(WrittenInt(INT32_MIN) == "d280000000");
    CHECK(WrittenInt(static_cast<int64_t>(INT32_MIN) - 1) == "d3ffffffff7fffffff");
    CHECK(WrittenInt(INT64_MIN) == "d38000000000000000");
}

TEST_CASE("doubles are always float64, including NaN, infinity and minus zero") {
    CHECK(WrittenDouble(1.5) == "cb3ff8000000000000");
    CHECK(WrittenDouble(0.0) == "cb0000000000000000");
    CHECK(WrittenDouble(-0.0) == "cb8000000000000000");
    CHECK(WrittenDouble(0.5) == "cb3fe0000000000000");   // Fits float32, still float64.
    CHECK(WrittenDouble(std::numeric_limits<double>::infinity()) == "cb7ff0000000000000");
    CHECK(WrittenDouble(-std::numeric_limits<double>::infinity()) == "cbfff0000000000000");
    CHECK(WrittenDouble(std::numeric_limits<double>::quiet_NaN()) == "cb7ff8000000000000");
}

TEST_CASE("strings use the shortest str form and are copied unchanged") {
    CHECK(Written([](Visitor& v) { return v.String(""); }) == "a0");
    CHECK(Written([](Visitor& v) { return v.String("abc"); }) == "a3616263");
    CHECK(Written([](Visitor& v) { return v.String(std::string("\0\xC3\xA9", 3)); }) == "a300c3a9");

    CheckHeader(WrittenText(31, false), "bf", 31);
    CheckHeader(WrittenText(32, false), "d9 20", 32);
    CheckHeader(WrittenText(255, false), "d9 ff", 255);
    CheckHeader(WrittenText(256, false), "da 01 00", 256);
    CheckHeader(WrittenText(65535, false), "da ff ff", 65535);
    CheckHeader(WrittenText(65536, false), "db 00 01 00 00", 65536);
}

TEST_CASE("strings are not checked for UTF-8") {
    CHECK(Written([](Visitor& v) { return v.String("\xFF"); }) == "a1ff");
}

TEST_CASE("Bytes use the shortest bin form") {
    CHECK(Written([](Visitor& v) { return v.Bytes(""); }) == "c400");
    CHECK(Written([](Visitor& v) { return v.Bytes(std::string("\0\xFF", 2)); }) == "c40200ff");

    CheckHeader(WrittenText(255, true), "c4 ff", 255);
    CheckHeader(WrittenText(256, true), "c5 01 00", 256);
    CheckHeader(WrittenText(65535, true), "c5 ff ff", 65535);
    CheckHeader(WrittenText(65536, true), "c6 00 01 00 00", 65536);
}

TEST_CASE("arrays and maps use the shortest form for their count") {
    CHECK(Written([](Visitor& v) { return v.EnterArray() && v.ExitArray(); }) == "90");
    CHECK(Written([](Visitor& v) { return v.EnterObject() && v.ExitObject(); }) == "80");

    CheckHeader(WrittenContainer(15, false), "9f", 15);
    CheckHeader(WrittenContainer(16, false), "dc 00 10", 16);
    CheckHeader(WrittenContainer(65535, false), "dc ff ff", 65535);
    CheckHeader(WrittenContainer(65536, false), "dd 00 01 00 00", 65536);

    // Each member is an empty key (a0) and a null (c0).
    CheckHeader(WrittenContainer(15, true), "8f", 30);
    CheckHeader(WrittenContainer(16, true), "de 00 10", 32);
    CheckHeader(WrittenContainer(65535, true), "de ff ff", 131070);
    CheckHeader(WrittenContainer(65536, true), "df 00 01 00 00", 131072);
}

TEST_CASE("a map counts its members, not its keys and values") {
    CHECK(Written([](Visitor& v) {
        return v.EnterObject()
            && v.Key("a") && v.Int(1)
            && v.Key("b") && v.EnterArray() && v.ExitArray()
            && v.ExitObject();
    }) == "82" "a161" "01" "a162" "90");
}

TEST_CASE("each header goes in front of its own container") {
    // [1, {"k": [true]}, []]
    CHECK(Written([](Visitor& v) {
        return v.EnterArray()
            && v.Int(1)
            && v.EnterObject() && v.Key("k") && v.EnterArray() && v.Bool(true) && v.ExitArray() && v.ExitObject()
            && v.EnterArray() && v.ExitArray()
            && v.ExitArray();
    }) == "93" "01" "81" "a16b" "91" "c3" "90");
}

TEST_CASE("an inner container with a wider header does not disturb the outer one") {
    // [[16 nulls], 2]: the inner header is three bytes long.
    std::string written = Written([](Visitor& v) {
        bool ok = v.EnterArray() && v.EnterArray();
        for (int i = 0; i < 16; ++i) {
            ok = ok && v.Null();
        }
        return ok && v.ExitArray() && v.Int(2) && v.ExitArray();
    });
    std::string expected = "92" "dc0010";
    for (int i = 0; i < 16; ++i) {
        expected += "c0";
    }
    expected += "02";
    CHECK(written == expected);
}

TEST_CASE("the output reaches the sink in one Write, when the root is complete") {
    SUBCASE("a scalar root") {
        RecordingSink sink;
        msgpack::Writer writer(sink);
        CHECK(writer.Int(1));
        CHECK(sink.calls == 1);
        CHECK(ToHex(sink.data) == "01");
    }
    SUBCASE("a container root") {
        RecordingSink sink;
        msgpack::Writer writer(sink);
        CHECK(writer.EnterArray());
        CHECK(writer.String("a"));
        CHECK(writer.EnterObject());
        CHECK(writer.ExitObject());
        CHECK(sink.calls == 0);
        CHECK(writer.ExitArray());
        CHECK(sink.calls == 1);
        CHECK(ToHex(sink.data) == "92a16180");
    }
}

TEST_CASE("events after the root is complete are rejected") {
    SUBCASE("after a scalar root") {
        RecordingSink sink;
        msgpack::Writer writer(sink);
        CHECK(writer.Null());
        CHECK_FALSE(writer.Null());
        CHECK_FALSE(writer.EnterArray());
        CHECK_FALSE(writer.Bytes(""));
        CHECK(sink.calls == 1);
        CHECK(ToHex(sink.data) == "c0");
    }
    SUBCASE("after a container root") {
        RecordingSink sink;
        msgpack::Writer writer(sink);
        CHECK(writer.EnterArray());
        CHECK(writer.ExitArray());
        CHECK_FALSE(writer.Int(1));
        CHECK_FALSE(writer.ExitArray());
        CHECK(sink.calls == 1);
        CHECK(ToHex(sink.data) == "90");
    }
}

TEST_CASE("events that do not fit the sequence are rejected and nothing reaches the sink") {
    RecordingSink sink;
    msgpack::Writer writer(sink);

    SUBCASE("Key outside an object") {
        CHECK_FALSE(writer.Key("k"));
        CHECK(writer.EnterArray());
        CHECK_FALSE(writer.Key("k"));
    }
    SUBCASE("a value in an object without a Key") {
        CHECK(writer.EnterObject());
        CHECK_FALSE(writer.Int(1));
        CHECK_FALSE(writer.String("s"));
        CHECK_FALSE(writer.Bytes("b"));
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
        CHECK(writer.ExitObject());   // The object is still open and can be closed.
        sink.data.clear();
        sink.calls = 0;
        CHECK_FALSE(writer.ExitObject());
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

    CHECK(sink.calls == 0);
    CHECK(sink.data.empty());
}

TEST_CASE("a failing sink makes the event that completes the root fail") {
    SUBCASE("a scalar root") {
        RecordingSink sink;
        sink.fail = true;
        msgpack::Writer writer(sink);
        CHECK_FALSE(writer.Double(1.0));
    }
    SUBCASE("a container root") {
        RecordingSink sink;
        sink.fail = true;
        msgpack::Writer writer(sink);
        CHECK(writer.EnterObject());
        CHECK(writer.Key("a"));
        CHECK(writer.Null());
        CHECK_FALSE(writer.ExitObject());
        CHECK(sink.calls == 1);
    }
}

TEST_CASE("Value::Accept can drive the writer") {
    Document document;
    Value* list = document.NewArray();
    list->Append(document.NewInt(-1));
    list->Append(document.NewDouble(2.0));
    list->Append(document.NewUint(UINT64_MAX));
    list->Append(document.NewBytes("\x01"));
    Value* root = document.NewObject();
    root->Set("list", list);
    root->Set("ok", document.NewBool(true));

    StringSink sink;
    msgpack::Writer writer(sink);
    CHECK(root->Accept(writer));
    CHECK(ToHex(sink.Str()) ==
          "82"
          "a46c697374" "94" "ff" "cb4000000000000000" "cfffffffffffffffff" "c40101"
          "a26f6b" "c3");
}

TEST_CASE("500 levels of nesting are written correctly") {
    StringSink sink;
    msgpack::Writer writer(sink);
    for (int i = 0; i < 500; ++i) {
        REQUIRE(writer.EnterArray());
    }
    for (int i = 0; i < 500; ++i) {
        REQUIRE(writer.ExitArray());
    }
    std::string expected(499, static_cast<char>(0x91));
    expected += static_cast<char>(0x90);
    CHECK(sink.Str() == expected);
}
