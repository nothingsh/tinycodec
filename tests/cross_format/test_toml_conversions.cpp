// Converts between TOML and the other formats, using the valid documents of
// toml-test and the y_ files of JSONTestSuite as input.

#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "hex.h"
#include "tinycodec/json/json.h"
#include "tinycodec/msgpack/msgpack.h"
#include "tinycodec/sink.h"
#include "tinycodec/toml/toml.h"
#include "unordered_equal.h"

using namespace tinycodec;

namespace {

struct TestFile {
    std::string name;
    std::string content;
};

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

// The valid TOML documents of toml-test, in list order.
std::vector<TestFile> LoadTomlFiles() {
    const std::filesystem::path directory = TINYCODEC_TOML_TEST_DATA_DIR;
    std::vector<TestFile> files;
    std::string list = ReadFile(directory / "files-toml-1.0.0");
    size_t start = 0;
    while (start < list.size()) {
        size_t end = std::min(list.find('\n', start), list.size());
        std::string path = list.substr(start, end - start);
        if (path.compare(0, 6, "valid/") == 0 && path.size() > 5 && path.compare(path.size() - 5, 5, ".toml") == 0) {
            files.push_back({path, ReadFile(directory / path)});
        }
        start = end + 1;
    }
    return files;
}

// The y_ files of JSONTestSuite, sorted by name.
std::vector<TestFile> LoadJsonFiles() {
    std::vector<TestFile> files;
    const std::filesystem::path directory = std::filesystem::path(TINYCODEC_JSON_TEST_DATA_DIR) / "test_parsing";
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
        std::string name = entry.path().filename().string();
        if (name.compare(0, 2, "y_") == 0) {
            files.push_back({name, ReadFile(entry.path())});
        }
    }
    std::sort(files.begin(), files.end(), [](const TestFile& a, const TestFile& b) { return a.name < b.name; });
    return files;
}

// Whether value, or anything inside it, matches the test.
template <typename Test>
bool Contains(const Value& value, Test test) {
    if (test(value)) {
        return true;
    }
    for (const Value* child = value.FirstChild(); child != nullptr; child = child->Next()) {
        if (Contains(*child, test)) {
            return true;
        }
    }
    return false;
}

bool HasDateTime(const Value& value) {
    return Contains(value, [](const Value& v) { return v.GetType() == Type::DateTime; });
}

bool HasNonFinite(const Value& value) {
    return Contains(value, [](const Value& v) {
        double number = 0.0;
        return v.GetType() == Type::Double && v.QueryDouble(&number) && !std::isfinite(number);
    });
}

// Whether JSON value can be written as TOML: an object root, no null, no
// integer above INT64_MAX and no repeated key.
bool FitsToml(const Value& value) {
    if (value.GetType() != Type::Object) {
        return false;
    }
    return !Contains(value, [](const Value& v) {
        if (v.GetType() == Type::Null || v.GetType() == Type::Uint) {
            return true;
        }
        for (const Value* a = v.GetType() == Type::Object ? v.FirstChild() : nullptr; a != nullptr; a = a->Next()) {
            for (const Value* b = a->Next(); b != nullptr; b = b->Next()) {
                if (a->Key() == b->Key()) {
                    return true;
                }
            }
        }
        return false;
    });
}

}  // namespace

TEST_CASE("TOML without datetimes survives MessagePack and back") {
    int converted = 0;
    for (const TestFile& file : LoadTomlFiles()) {
        CAPTURE(file.name);
        Document a;
        REQUIRE(toml::Parse(file.content, a).Ok());
        if (HasDateTime(*a.Root())) {
            CHECK(msgpack::Encode(*a.Root()).empty());
            continue;
        }

        std::string encoded = msgpack::Encode(*a.Root());
        REQUIRE_FALSE(encoded.empty());
        Document b;
        REQUIRE(msgpack::Parse(encoded, b).Ok());
        std::string text;
        REQUIRE(toml::Stringify(*b.Root(), &text));
        Document c;
        REQUIRE(toml::Parse(text, c).Ok());

        CHECK(EqualsIgnoringOrder(*a.Root(), *b.Root()));
        CHECK(EqualsIgnoringOrder(*a.Root(), *c.Root()));

        // The direct path gives the same bytes as going through a Document.
        StringSink direct;
        msgpack::Writer writer(direct);
        toml::Reader reader;
        REQUIRE(reader.Parse(file.content, writer).Ok());
        CHECK(ToHex(direct.Str()) == ToHex(encoded));
        ++converted;
    }
    CHECK(converted == 189);
}

TEST_CASE("TOML without datetimes, NaN or infinity survives JSON and back") {
    int converted = 0;
    for (const TestFile& file : LoadTomlFiles()) {
        CAPTURE(file.name);
        Document a;
        REQUIRE(toml::Parse(file.content, a).Ok());
        if (HasDateTime(*a.Root()) || HasNonFinite(*a.Root())) {
            CHECK(json::Stringify(*a.Root()).empty());
            continue;
        }

        std::string jsonText = json::Stringify(*a.Root());
        REQUIRE_FALSE(jsonText.empty());
        Document b;
        REQUIRE(json::Parse(jsonText, b).Ok());
        std::string tomlText;
        REQUIRE(toml::Stringify(*b.Root(), &tomlText));
        Document c;
        REQUIRE(toml::Parse(tomlText, c).Ok());

        CHECK(EqualsIgnoringOrder(*a.Root(), *b.Root()));
        CHECK(EqualsIgnoringOrder(*a.Root(), *c.Root()));

        StringSink direct;
        json::Writer writer(direct);
        toml::Reader reader;
        REQUIRE(reader.Parse(file.content, writer).Ok());
        CHECK(direct.Str() == jsonText);
        ++converted;
    }
    CHECK(converted == 186);
}

TEST_CASE("JSON that fits TOML survives TOML and back") {
    int converted = 0;
    for (const TestFile& file : LoadJsonFiles()) {
        CAPTURE(file.name);
        Document a;
        REQUIRE(json::Parse(file.content, a).Ok());
        if (!FitsToml(*a.Root())) {
            std::string unused;
            CHECK_FALSE(toml::Stringify(*a.Root(), &unused));
            continue;
        }

        std::string tomlText;
        REQUIRE(toml::Stringify(*a.Root(), &tomlText));
        CAPTURE(tomlText);
        Document b;
        REQUIRE(toml::Parse(tomlText, b).Ok());
        std::string jsonText = json::Stringify(*b.Root());
        REQUIRE_FALSE(jsonText.empty());
        Document c;
        REQUIRE(json::Parse(jsonText, c).Ok());

        CHECK(EqualsIgnoringOrder(*a.Root(), *b.Root()));
        CHECK(EqualsIgnoringOrder(*a.Root(), *c.Root()));

        StringSink direct;
        toml::Writer writer(direct);
        json::Reader reader;
        REQUIRE(reader.Parse(file.content, writer).Ok());
        CHECK(direct.Str() == tomlText);
        ++converted;
    }
    CHECK(converted == 10);
}

TEST_CASE("what TOML cannot express is Aborted at the value") {
    SUBCASE("null") {
        StringSink sink;
        toml::Writer writer(sink);
        json::Reader reader;
        Error error = reader.Parse("{\"a\": 1, \"b\": null}", writer);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == 14);
        CHECK(sink.Str().empty());
    }
    SUBCASE("a root that is not an object") {
        StringSink sink;
        toml::Writer writer(sink);
        json::Reader reader;
        Error error = reader.Parse("[1]", writer);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == 0);
    }
    SUBCASE("an integer above INT64_MAX") {
        StringSink sink;
        toml::Writer writer(sink);
        json::Reader reader;
        Error error = reader.Parse("{\"a\": 18446744073709551615}", writer);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == 6);
    }
    SUBCASE("Bytes") {
        StringSink sink;
        toml::Writer writer(sink);
        msgpack::Reader reader;
        // {"a": bin 00}
        Error error = reader.Parse(FromHex("81" "a161" "c40100"), writer);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == 3);
    }
    SUBCASE("a repeated key, found when the root is complete") {
        StringSink sink;
        toml::Writer writer(sink);
        json::Reader reader;
        Error error = reader.Parse("{\"a\": 1, \"a\": 2}", writer);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == 15);
        CHECK(sink.Str().empty());
    }
}

TEST_CASE("a TOML datetime is Aborted at the value by JSON and MessagePack") {
    const std::string text = "a = 1\nwhen = 1979-05-27T07:32:00Z";
    SUBCASE("JSON") {
        StringSink sink;
        json::Writer writer(sink);
        toml::Reader reader;
        Error error = reader.Parse(text, writer);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == 13);
        CHECK(error.line == 2);
        CHECK(error.column == 8);
    }
    SUBCASE("MessagePack") {
        StringSink sink;
        msgpack::Writer writer(sink);
        toml::Reader reader;
        Error error = reader.Parse(text, writer);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == 13);
        CHECK(sink.Str().empty());
    }
}
