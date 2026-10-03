#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "hex.h"
#include "recording_visitor.h"
#include "tinycodec/builder.h"
#include "tinycodec/document.h"
#include "tinycodec/msgpack/reader.h"
#include "tinycodec/value.h"

using namespace tinycodec;

namespace {

using Events = std::vector<std::string>;

// Parses the bytes given in hex and returns the events it produced.
Events EventsOf(const std::string& hex) {
    CAPTURE(hex);
    RecordingVisitor visitor;
    msgpack::Reader reader;
    Error error = reader.Parse(FromHex(hex), visitor);
    CHECK(error.Ok());
    return visitor.events;
}

// Checks that the bytes given in hex fail with code at offset.
void CheckFails(const std::string& hex, ErrorCode code, size_t offset) {
    CAPTURE(hex);
    RecordingVisitor visitor;
    msgpack::Reader reader;
    Error error = reader.Parse(FromHex(hex), visitor);
    CHECK(error.code == code);
    CHECK(error.offset == offset);
    CHECK(error.line == 0);
    CHECK(error.column == 0);
}

// Parses the bytes given in hex, which must hold a single float, and
// returns its value.
double DoubleOf(const std::string& hex) {
    CAPTURE(hex);
    Document document;
    DocumentBuilder builder(document);
    msgpack::Reader reader;
    REQUIRE(reader.Parse(FromHex(hex), builder).Ok());
    REQUIRE(document.Root()->GetType() == Type::Double);
    double value = 0.0;
    document.Root()->QueryDouble(&value);
    return value;
}

// One value of every kind:
// {"a": [nil, false, 300, -200, 1.5, "é", bin 01 02], "b": {}}
const char SAMPLE[] =
    "82"
    "a161" "97" "c0" "c2" "cd012c" "d1ff38" "cb3ff8000000000000" "a2c3a9" "c4020102"
    "a162" "80";

}  // namespace

TEST_CASE("nil, booleans and every integer form") {
    CHECK(EventsOf("c0") == Events{"Null"});
    CHECK(EventsOf("c2") == Events{"Bool(false)"});
    CHECK(EventsOf("c3") == Events{"Bool(true)"});

    CHECK(EventsOf("00") == Events{"Int(0)"});
    CHECK(EventsOf("7f") == Events{"Int(127)"});
    CHECK(EventsOf("ccff") == Events{"Int(255)"});
    CHECK(EventsOf("cdffff") == Events{"Int(65535)"});
    CHECK(EventsOf("ceffffffff") == Events{"Int(4294967295)"});
    CHECK(EventsOf("cf0000000100000000") == Events{"Int(4294967296)"});

    CHECK(EventsOf("ff") == Events{"Int(-1)"});
    CHECK(EventsOf("e0") == Events{"Int(-32)"});
    CHECK(EventsOf("d080") == Events{"Int(-128)"});
    CHECK(EventsOf("d07f") == Events{"Int(127)"});
    CHECK(EventsOf("d18000") == Events{"Int(-32768)"});
    CHECK(EventsOf("d280000000") == Events{"Int(-2147483648)"});
    CHECK(EventsOf("d3ffffffffffffffff") == Events{"Int(-1)"});
}

TEST_CASE("integers become Int or Uint by value, not by encoding") {
    CHECK(EventsOf("cf7fffffffffffffff") == Events{"Int(9223372036854775807)"});
    CHECK(EventsOf("cf8000000000000000") == Events{"Uint(9223372036854775808)"});
    CHECK(EventsOf("cfffffffffffffffff") == Events{"Uint(18446744073709551615)"});
    CHECK(EventsOf("d38000000000000000") == Events{"Int(-9223372036854775808)"});
    CHECK(EventsOf("d37fffffffffffffff") == Events{"Int(9223372036854775807)"});
    CHECK(EventsOf("d30000000000000001") == Events{"Int(1)"});
}

TEST_CASE("float32 is widened exactly and float64 is read as is") {
    CHECK(DoubleOf("ca3fc00000") == 1.5);
    CHECK(DoubleOf("ca3dcccccd") == static_cast<double>(0.1f));   // Not 0.1.
    CHECK(DoubleOf("cb3fb999999999999a") == 0.1);
    CHECK(DoubleOf("cb8000000000000000") == 0.0);
    CHECK(std::signbit(DoubleOf("cb8000000000000000")));
}

TEST_CASE("NaN and infinity are read like any other float") {
    CHECK(std::isnan(DoubleOf("ca7fc00000")));
    CHECK(std::isnan(DoubleOf("cb7ff8000000000000")));
    CHECK(DoubleOf("ca7f800000") == HUGE_VAL);
    CHECK(DoubleOf("cbfff0000000000000") == -HUGE_VAL);
}

TEST_CASE("every str form, with the content checked as UTF-8") {
    CHECK(EventsOf("a0") == Events{"String()"});
    CHECK(EventsOf("a3616263") == Events{"String(abc)"});
    CHECK(EventsOf("d903616263") == Events{"String(abc)"});
    CHECK(EventsOf("da0003616263") == Events{"String(abc)"});
    CHECK(EventsOf("db00000003616263") == Events{"String(abc)"});
    CHECK(EventsOf("a2c3a9") == Events{"String(\xC3\xA9)"});
    CHECK(EventsOf("a3610062") == Events{std::string("String(a\0b)", 11)});
}

TEST_CASE("every bin form becomes Bytes") {
    CHECK(EventsOf("c400") == Events{"Bytes()"});
    CHECK(EventsOf("c40200ff") == Events{"Bytes(00ff)"});
    CHECK(EventsOf("c5000200ff") == Events{"Bytes(00ff)"});
    CHECK(EventsOf("c60000000200ff") == Events{"Bytes(00ff)"});
    CHECK(EventsOf("c401c3") == Events{"Bytes(c3)"});   // Not UTF-8, and that is fine.
}

TEST_CASE("every array and map form") {
    CHECK(EventsOf("90") == Events{"EnterArray", "ExitArray"});
    CHECK(EventsOf("920102") == Events{"EnterArray", "Int(1)", "Int(2)", "ExitArray"});
    CHECK(EventsOf("dc00020102") == Events{"EnterArray", "Int(1)", "Int(2)", "ExitArray"});
    CHECK(EventsOf("dd000000020102") == Events{"EnterArray", "Int(1)", "Int(2)", "ExitArray"});

    CHECK(EventsOf("80") == Events{"EnterObject", "ExitObject"});
    CHECK(EventsOf("81a16101") == Events{"EnterObject", "Key(a)", "Int(1)", "ExitObject"});
    CHECK(EventsOf("de0001a16101") == Events{"EnterObject", "Key(a)", "Int(1)", "ExitObject"});
    CHECK(EventsOf("df00000001a16101") == Events{"EnterObject", "Key(a)", "Int(1)", "ExitObject"});
    CHECK(EventsOf("81d90161" "01") == Events{"EnterObject", "Key(a)", "Int(1)", "ExitObject"});
}

TEST_CASE("nested containers and repeated keys are reported as they appear") {
    CHECK(EventsOf(SAMPLE) == Events{
        "EnterObject",
        "Key(a)", "EnterArray", "Null", "Bool(false)", "Int(300)", "Int(-200)", "Double(1.500000)",
        "String(\xC3\xA9)", "Bytes(0102)", "ExitArray",
        "Key(b)", "EnterObject", "ExitObject",
        "ExitObject"});
    CHECK(EventsOf("82a16101a16102") == Events{"EnterObject", "Key(a)", "Int(1)", "Key(a)", "Int(2)", "ExitObject"});
}

TEST_CASE("empty input is UnexpectedEnd at 0") {
    CheckFails("", ErrorCode::UnexpectedEnd, 0);
}

TEST_CASE("input that ends inside a value is UnexpectedEnd at the end") {
    CheckFails("cc", ErrorCode::UnexpectedEnd, 1);            // Number missing.
    CheckFails("cd01", ErrorCode::UnexpectedEnd, 2);          // Number cut short.
    CheckFails("cb3ff8", ErrorCode::UnexpectedEnd, 3);
    CheckFails("da00", ErrorCode::UnexpectedEnd, 2);          // Length cut short.
    CheckFails("a36162", ErrorCode::UnexpectedEnd, 3);        // Content cut short.
    CheckFails("c40501", ErrorCode::UnexpectedEnd, 3);
    CheckFails("9201", ErrorCode::UnexpectedEnd, 2);          // Element missing.
    CheckFails("81", ErrorCode::UnexpectedEnd, 1);            // Key missing.
    CheckFails("81a161", ErrorCode::UnexpectedEnd, 3);        // Value missing.
}

TEST_CASE("every proper prefix of a valid document is UnexpectedEnd at its end") {
    std::string sample = FromHex(SAMPLE);
    for (size_t length = 0; length < sample.size(); ++length) {
        CAPTURE(length);
        RecordingVisitor visitor;
        msgpack::Reader reader;
        Error error = reader.Parse(sample.substr(0, length), visitor);
        CHECK(error.code == ErrorCode::UnexpectedEnd);
        CHECK(error.offset == length);
    }
}

TEST_CASE("a count far larger than the input is UnexpectedEnd") {
    CheckFails("ddffffffff" "c0c0", ErrorCode::UnexpectedEnd, 7);
    CheckFails("dfffffffff" "a161c0", ErrorCode::UnexpectedEnd, 8);
    CheckFails("dbffffffff" "61", ErrorCode::UnexpectedEnd, 6);
    CheckFails("c6ffffffff" "61", ErrorCode::UnexpectedEnd, 6);
}

TEST_CASE("the unused type byte c1 is UnexpectedChar") {
    CheckFails("c1", ErrorCode::UnexpectedChar, 0);
    CheckFails("9201c1", ErrorCode::UnexpectedChar, 2);
    CheckFails("81c1c0", ErrorCode::UnexpectedChar, 1);   // In place of a key.
}

TEST_CASE("bytes after the root are UnexpectedChar at the first of them") {
    CheckFails("c0c0", ErrorCode::UnexpectedChar, 1);
    CheckFails("9000", ErrorCode::UnexpectedChar, 1);
    CheckFails("a161" "ff", ErrorCode::UnexpectedChar, 2);
}

TEST_CASE("a str that is not UTF-8 is InvalidUtf8 at the first bad byte") {
    CheckFails("a2" "61ff", ErrorCode::InvalidUtf8, 2);
    CheckFails("a3" "61eda080", ErrorCode::InvalidUtf8, 2);       // A surrogate.
    CheckFails("d902" "c328", ErrorCode::InvalidUtf8, 2);
    CheckFails("a2" "61c3", ErrorCode::InvalidUtf8, 2);           // Sequence cut by the str's end.
    CheckFails("81" "a1ff" "01", ErrorCode::InvalidUtf8, 2);      // In a key.
}

TEST_CASE("a str is checked for completeness before its content is checked") {
    CheckFails("a3ff", ErrorCode::UnexpectedEnd, 2);
}

TEST_CASE("a map key that is not a str is Unsupported at its type byte") {
    CheckFails("8101" "02", ErrorCode::Unsupported, 1);
    CheckFails("82a16101" "c0" "02", ErrorCode::Unsupported, 4);
    CheckFails("81" "c40161" "01", ErrorCode::Unsupported, 1);   // bin is not a str either.
    CheckFails("81" "90" "01", ErrorCode::Unsupported, 1);
    CheckFails("81" "cd", ErrorCode::Unsupported, 1);           // Reported before the cut-off number.
}

TEST_CASE("ext values, including timestamps, are Unsupported at their type byte") {
    const char* exts[] = {
        "d40110", "d5022021", "d60330313233", "d7044041424344454647",
        "d805505152535455565758595a5b5c5d5e5f",
        "c70006", "c8000006", "c90000000006",
        "d6ff5a4af6a5",   // A timestamp.
    };
    for (const char* ext : exts) {
        CheckFails(ext, ErrorCode::Unsupported, 0);
    }
    CheckFails("9201" "d401", ErrorCode::Unsupported, 2);   // Reported before the cut-off content.
    CheckFails("c7", ErrorCode::Unsupported, 0);
}

TEST_CASE("nesting is limited to kMaxDepth containers") {
    std::string ok(static_cast<size_t>(msgpack::kMaxDepth) - 1, static_cast<char>(0x91));
    ok += static_cast<char>(0x90);
    RecordingVisitor visitor;
    msgpack::Reader reader;
    CHECK(reader.Parse(ok, visitor).Ok());

    std::string tooDeep(static_cast<size_t>(msgpack::kMaxDepth), static_cast<char>(0x91));
    tooDeep += static_cast<char>(0x90);
    Error error = reader.Parse(tooDeep, visitor);
    CHECK(error.code == ErrorCode::DepthExceeded);
    CHECK(error.offset == static_cast<size_t>(msgpack::kMaxDepth));

    std::string maps;
    for (int i = 0; i < msgpack::kMaxDepth; ++i) {
        maps += FromHex("81a0");
    }
    maps += FromHex("80");
    error = reader.Parse(maps, visitor);
    CHECK(error.code == ErrorCode::DepthExceeded);
    CHECK(error.offset == maps.size() - 1);
}

TEST_CASE("very deep input fails cleanly instead of overflowing the stack") {
    std::string deep(1000000, static_cast<char>(0x91));
    RecordingVisitor visitor;
    msgpack::Reader reader;
    CHECK(reader.Parse(deep, visitor).code == ErrorCode::DepthExceeded);
}

TEST_CASE("when the visitor rejects an event, parsing stops with Aborted") {
    std::string sample = FromHex(SAMPLE);
    RecordingVisitor complete;
    msgpack::Reader reader;
    REQUIRE(reader.Parse(sample, complete).Ok());
    int total = static_cast<int>(complete.events.size());

    for (int failAt = 0; failAt < total; ++failAt) {
        CAPTURE(failAt);
        RecordingVisitor visitor;
        visitor.failAt = failAt;
        CHECK(reader.Parse(sample, visitor).code == ErrorCode::Aborted);
        // The rejected event is the last one delivered.
        CHECK(static_cast<int>(visitor.events.size()) == failAt + 1);
    }
}

TEST_CASE("Aborted is positioned at the type byte, or just past a closed container") {
    // [1, {"k": "v"}]
    std::string data = FromHex("92" "01" "81" "a16b" "a176");
    auto abortedAt = [&data](int failAt) {
        RecordingVisitor visitor;
        visitor.failAt = failAt;
        msgpack::Reader reader;
        Error error = reader.Parse(data, visitor);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.line == 0);
        CHECK(error.column == 0);
        return error.offset;
    };
    CHECK(abortedAt(0) == 0);   // EnterArray
    CHECK(abortedAt(1) == 1);   // Int(1)
    CHECK(abortedAt(2) == 2);   // EnterObject
    CHECK(abortedAt(3) == 3);   // Key(k)
    CHECK(abortedAt(4) == 5);   // String(v)
    CHECK(abortedAt(5) == 7);   // ExitObject: the end of the map.
    CHECK(abortedAt(6) == 7);   // ExitArray: the end of the input.
}

TEST_CASE("Aborted for a Bytes event is positioned at the bin's type byte") {
    RecordingVisitor visitor;
    visitor.failAt = 1;
    msgpack::Reader reader;
    Error error = reader.Parse(FromHex("91" "c40100"), visitor);
    CHECK(error.code == ErrorCode::Aborted);
    CHECK(error.offset == 1);
}

TEST_CASE("a Reader can be reused, also after a failed parse") {
    msgpack::Reader reader;

    RecordingVisitor first;
    CHECK(reader.Parse(FromHex("9201"), first).code == ErrorCode::UnexpectedEnd);

    RecordingVisitor second;
    Error error = reader.Parse(FromHex("9101"), second);
    CHECK(error.Ok());
    CHECK(error.offset == 0);
    CHECK(second.events == Events{"EnterArray", "Int(1)", "ExitArray"});
}

TEST_CASE("the input may be part of a larger buffer and is not read past its end") {
    std::string buffer = FromHex("a3616263" "c0");
    RecordingVisitor visitor;
    msgpack::Reader reader;

    Error error = reader.Parse(std::string_view(buffer.data(), 3), visitor);
    CHECK(error.code == ErrorCode::UnexpectedEnd);
    CHECK(error.offset == 3);

    CHECK(reader.Parse(std::string_view(buffer.data(), 4), visitor).Ok());
}
