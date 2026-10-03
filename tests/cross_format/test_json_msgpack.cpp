// Converts between JSON and MessagePack, using the y_ files of JSONTestSuite
// as input. See formats/json/test/data/JSONTestSuite/README.md.

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

using namespace tinycodec;

namespace {

struct TestFile {
    std::string name;
    std::string content;
};

// Loads every y_ file of the suite, sorted by name.
std::vector<TestFile> LoadAcceptedFiles() {
    std::vector<TestFile> files;
    const std::filesystem::path directory = std::filesystem::path(TINYCODEC_JSON_TEST_DATA_DIR) / "test_parsing";
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
        std::string name = entry.path().filename().string();
        if (name.compare(0, 2, "y_") != 0) {
            continue;
        }
        std::ifstream stream(entry.path(), std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        files.push_back({name, content});
    }
    std::sort(files.begin(), files.end(), [](const TestFile& a, const TestFile& b) { return a.name < b.name; });
    return files;
}

}  // namespace

TEST_CASE("the JSON test data is present") {
    CHECK(LoadAcceptedFiles().size() == 95);
}

TEST_CASE("every y_ file survives JSON to MessagePack to JSON unchanged") {
    for (const TestFile& file : LoadAcceptedFiles()) {
        CAPTURE(file.name);
        Document a;
        REQUIRE(json::Parse(file.content, a).Ok());

        std::string encoded = msgpack::Encode(*a.Root());
        REQUIRE_FALSE(encoded.empty());
        Document b;
        REQUIRE(msgpack::Parse(encoded, b).Ok());

        std::string text = json::Stringify(*b.Root());
        REQUIRE_FALSE(text.empty());
        Document c;
        REQUIRE(json::Parse(text, c).Ok());

        CHECK(a.Root()->Equals(*b.Root()));
        CHECK(b.Root()->Equals(*c.Root()));
        CHECK(a.Root()->Equals(*c.Root()));
    }
}

TEST_CASE("json::Reader to msgpack::Writer gives the same bytes as Encode") {
    for (const TestFile& file : LoadAcceptedFiles()) {
        CAPTURE(file.name);
        Document document;
        REQUIRE(json::Parse(file.content, document).Ok());

        StringSink direct;
        msgpack::Writer writer(direct);
        json::Reader reader;
        REQUIRE(reader.Parse(file.content, writer).Ok());

        CHECK(ToHex(direct.Str()) == ToHex(msgpack::Encode(*document.Root())));
    }
}

TEST_CASE("msgpack::Reader to json::Writer gives the same text as Stringify") {
    for (const TestFile& file : LoadAcceptedFiles()) {
        CAPTURE(file.name);
        Document a;
        REQUIRE(json::Parse(file.content, a).Ok());
        std::string encoded = msgpack::Encode(*a.Root());
        Document b;
        REQUIRE(msgpack::Parse(encoded, b).Ok());

        StringSink direct;
        json::Writer writer(direct);
        msgpack::Reader reader;
        REQUIRE(reader.Parse(encoded, writer).Ok());

        CHECK(direct.Str() == json::Stringify(*b.Root()));
    }
}

TEST_CASE("MessagePack that JSON cannot express is Aborted at the value") {
    SUBCASE("Bytes") {
        StringSink sink;
        json::Writer writer(sink);
        msgpack::Reader reader;
        // [1, bin 00]
        Error error = reader.Parse(FromHex("92" "01" "c40100"), writer);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == 2);
    }
    SUBCASE("NaN") {
        StringSink sink;
        json::Writer writer(sink);
        msgpack::Reader reader;
        // {"a": NaN}
        Error error = reader.Parse(FromHex("81" "a161" "cb7ff8000000000000"), writer);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == 3);
    }
    SUBCASE("infinity, as float32") {
        StringSink sink;
        json::Writer writer(sink);
        msgpack::Reader reader;
        Error error = reader.Parse(FromHex("ca7f800000"), writer);
        CHECK(error.code == ErrorCode::Aborted);
        CHECK(error.offset == 0);
    }
}
