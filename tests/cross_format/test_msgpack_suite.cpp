// Runs the MessagePack reader and writer over msgpack-test-suite. See
// formats/msgpack/test/data/msgpack-test-suite/README.md. The suite is a
// JSON file, read with json::Parse.

#include "doctest.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "hex.h"
#include "tinycodec/json/json.h"
#include "tinycodec/msgpack/msgpack.h"

using namespace tinycodec;

namespace {

struct Case {
    std::string group;      // e.g. "20.number-positive.yaml"
    const Value* fields;    // The case object: the expected value and "msgpack".
};

// Loads the suite into document and returns every case, in file order.
std::vector<Case> LoadCases(Document& document) {
    std::ifstream stream(std::string(TINYCODEC_MSGPACK_TEST_DATA_DIR) + "/msgpack-test-suite.json", std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    REQUIRE(json::Parse(content, document).Ok());

    std::vector<Case> cases;
    for (const Value* group = document.Root()->FirstChild(); group != nullptr; group = group->Next()) {
        for (const Value* fields = group->FirstChild(); fields != nullptr; fields = fields->Next()) {
            cases.push_back({std::string(group->Key()), fields});
        }
    }
    return cases;
}

// The encodings listed for a case, as raw bytes.
std::vector<std::string> Encodings(const Case& testCase) {
    std::vector<std::string> encodings;
    const Value* list = testCase.fields->Find("msgpack");
    for (const Value* item = list->FirstChild(); item != nullptr; item = item->Next()) {
        std::string_view hex;
        item->QueryString(&hex);
        encodings.push_back(FromHex(hex));
    }
    return encodings;
}

// Cases whose value this library cannot represent.
bool IsUnsupported(const Case& testCase) {
    return testCase.fields->Find("timestamp") != nullptr || testCase.fields->Find("ext") != nullptr;
}

// Checks a decoded value against what the case expects.
void CheckValue(const Case& testCase, const Value& decoded) {
    const Value* fields = testCase.fields;

    if (const Value* binary = fields->Find("binary")) {
        std::string_view hex;
        std::string_view bytes;
        REQUIRE(binary->QueryString(&hex));
        REQUIRE(decoded.QueryBytes(&bytes));
        CHECK(ToHex(bytes) == ToHex(FromHex(hex)));
        return;
    }

    const Value* bignum = fields->Find("bignum");
    if (bignum != nullptr && (decoded.GetType() == Type::Int || decoded.GetType() == Type::Uint)) {
        std::string_view expected;
        REQUIRE(bignum->QueryString(&expected));
        int64_t asInt = 0;
        uint64_t asUint = 0;
        std::string text;
        if (decoded.QueryInt(&asInt)) {
            text = std::to_string(asInt);
        } else if (decoded.QueryUint(&asUint)) {
            text = std::to_string(asUint);
        }
        CHECK(text == expected);
        return;
    }

    // The suite treats numbers the JSON way, listing integer and float
    // encodings in the same case, so compare them as doubles.
    if (const Value* number = fields->Find("number")) {
        double expected = 0.0;
        double actual = 0.0;
        REQUIRE(number->QueryDouble(&expected));
        REQUIRE(decoded.QueryDouble(&actual));
        CHECK(actual == expected);
        return;
    }

    for (const char* name : {"nil", "bool", "string", "array", "map"}) {
        if (const Value* expected = fields->Find(name)) {
            CHECK(decoded.Equals(*expected));
            return;
        }
    }
    FAIL("a case with no expected value");
}

}  // namespace

TEST_CASE("the test data is present") {
    Document document;
    std::vector<Case> cases = LoadCases(document);
    size_t encodings = 0;
    size_t unsupportedCases = 0;
    size_t unsupportedEncodings = 0;
    for (const Case& testCase : cases) {
        size_t count = Encodings(testCase).size();
        encodings += count;
        if (IsUnsupported(testCase)) {
            ++unsupportedCases;
            unsupportedEncodings += count;
        }
    }
    CHECK(cases.size() == 85);
    CHECK(encodings == 233);
    CHECK(unsupportedCases == 26);
    CHECK(unsupportedEncodings == 30);
}

TEST_CASE("timestamp and ext encodings are Unsupported") {
    Document document;
    for (const Case& testCase : LoadCases(document)) {
        if (!IsUnsupported(testCase)) {
            continue;
        }
        for (const std::string& encoding : Encodings(testCase)) {
            CAPTURE(testCase.group);
            CAPTURE(ToHex(encoding));
            Document decoded;
            Error error = msgpack::Parse(encoding, decoded);
            CHECK(error.code == ErrorCode::Unsupported);
            CHECK(error.offset == 0);
        }
    }
}

TEST_CASE("every other encoding decodes to the expected value") {
    Document document;
    for (const Case& testCase : LoadCases(document)) {
        if (IsUnsupported(testCase)) {
            continue;
        }
        for (const std::string& encoding : Encodings(testCase)) {
            CAPTURE(testCase.group);
            CAPTURE(ToHex(encoding));
            Document decoded;
            REQUIRE(msgpack::Parse(encoding, decoded).Ok());
            CheckValue(testCase, *decoded.Root());
        }
    }
}

TEST_CASE("encoding a decoded value gives one of the case's encodings") {
    Document document;
    for (const Case& testCase : LoadCases(document)) {
        if (IsUnsupported(testCase)) {
            continue;
        }
        std::vector<std::string> encodings = Encodings(testCase);
        for (const std::string& encoding : encodings) {
            CAPTURE(testCase.group);
            CAPTURE(ToHex(encoding));
            Document decoded;
            REQUIRE(msgpack::Parse(encoding, decoded).Ok());
            std::string encoded = msgpack::Encode(*decoded.Root());
            CAPTURE(ToHex(encoded));
            bool listed = false;
            for (const std::string& candidate : encodings) {
                listed = listed || candidate == encoded;
            }
            CHECK(listed);
        }
    }
}
