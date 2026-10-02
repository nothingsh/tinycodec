// Runs the parser over JSONTestSuite. See data/JSONTestSuite/README.md.

#include "doctest.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "tinycodec/builder.h"
#include "tinycodec/json/json.h"
#include "tinycodec/sink.h"

using namespace tinycodec;

namespace {

struct TestFile {
    std::string name;
    std::string content;
};

// Loads every file in the suite whose name starts with prefix, sorted by name.
std::vector<TestFile> LoadFiles(const std::string& prefix) {
    std::vector<TestFile> files;
    const std::filesystem::path directory = std::filesystem::path(TINYCODEC_JSON_TEST_DATA_DIR) / "test_parsing";
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
        std::string name = entry.path().filename().string();
        if (name.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        std::ifstream stream(entry.path(), std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        files.push_back({name, content});
    }
    std::sort(files.begin(), files.end(), [](const TestFile& a, const TestFile& b) { return a.name < b.name; });
    return files;
}

// The implementation-defined files this parser accepts; it rejects the rest.
//  - Numbers too close to zero become 0.
//  - Integers beyond 64 bits become doubles.
//  - Exactly kMaxDepth levels of nesting are allowed.
const std::set<std::string> ACCEPTED_IMPLEMENTATION_DEFINED = {
    "i_number_double_huge_neg_exp.json",
    "i_number_real_underflow.json",
    "i_number_too_big_neg_int.json",
    "i_number_too_big_pos_int.json",
    "i_number_very_big_negative_int.json",
    "i_structure_500_nested_arrays.json",
};

}  // namespace

TEST_CASE("the test data is present") {
    CHECK(LoadFiles("y_").size() == 95);
    CHECK(LoadFiles("n_").size() == 188);
    CHECK(LoadFiles("i_").size() == 35);
}

TEST_CASE("every y_ file is accepted") {
    for (const TestFile& file : LoadFiles("y_")) {
        CAPTURE(file.name);
        Document document;
        Error error = json::Parse(file.content, document);
        CHECK(error.Ok());
        CHECK(document.Root() != nullptr);
    }
}

TEST_CASE("every n_ file is rejected") {
    for (const TestFile& file : LoadFiles("n_")) {
        CAPTURE(file.name);
        Document document;
        Error error = json::Parse(file.content, document);
        CHECK_FALSE(error.Ok());
        CHECK(document.Root() == nullptr);
    }
}

TEST_CASE("i_ files are handled as this implementation has decided") {
    for (const TestFile& file : LoadFiles("i_")) {
        CAPTURE(file.name);
        Document document;
        Error error = json::Parse(file.content, document);
        bool shouldAccept = ACCEPTED_IMPLEMENTATION_DEFINED.count(file.name) != 0;
        CHECK(error.Ok() == shouldAccept);
    }
}

TEST_CASE("every y_ file survives Parse, Stringify, Parse unchanged") {
    for (const TestFile& file : LoadFiles("y_")) {
        CAPTURE(file.name);
        Document first;
        REQUIRE(json::Parse(file.content, first).Ok());

        std::string text = json::Stringify(*first.Root());
        REQUIRE_FALSE(text.empty());

        Document second;
        REQUIRE(json::Parse(text, second).Ok());
        CHECK(second.Root()->Equals(*first.Root()));
        // Writing the reparsed tree gives the same text again.
        CHECK(json::Stringify(*second.Root()) == text);
    }
}

TEST_CASE("Reader to Writer gives the same bytes as going through a Document") {
    for (int indent : {0, 2}) {
        json::WriterOptions options;
        options.indent = indent;
        for (const TestFile& file : LoadFiles("y_")) {
            CAPTURE(file.name);
            CAPTURE(indent);

            StringSink direct;
            json::Writer directWriter(direct, options);
            json::Reader reader;
            REQUIRE(reader.Parse(file.content, directWriter).Ok());

            Document document;
            DocumentBuilder builder(document);
            REQUIRE(reader.Parse(file.content, builder).Ok());
            StringSink viaDocument;
            json::Writer documentWriter(viaDocument, options);
            REQUIRE(document.Root()->Accept(documentWriter));

            CHECK(direct.Str() == viaDocument.Str());
        }
    }
}
