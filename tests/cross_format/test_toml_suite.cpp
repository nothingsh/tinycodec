// Runs the TOML reader and writer over toml-test. See
// formats/toml/test/data/toml-test/README.md. The expected values are JSON
// files, read with json::Parse.

#include "doctest.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "tinycodec/json/json.h"
#include "tinycodec/toml/toml.h"
#include "unordered_equal.h"

using namespace tinycodec;

namespace {

std::string ReadFile(const std::string& path) {
    std::ifstream stream(std::string(TINYCODEC_TOML_TEST_DATA_DIR) + "/" + path, std::ios::binary);
    REQUIRE(stream.good());
    return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

// The paths in files-toml-1.0.0 that start with prefix and end in ".toml".
std::vector<std::string> ListFiles(const std::string& prefix) {
    std::vector<std::string> files;
    std::string list = ReadFile("files-toml-1.0.0");
    size_t start = 0;
    while (start < list.size()) {
        size_t end = list.find('\n', start);
        if (end == std::string::npos) {
            end = list.size();
        }
        std::string path = list.substr(start, end - start);
        if (path.compare(0, prefix.size(), prefix) == 0 && path.size() > 5
                && path.compare(path.size() - 5, 5, ".toml") == 0) {
            files.push_back(path);
        }
        start = end + 1;
    }
    return files;
}

std::string Text(const Value& value) {
    std::string_view text;
    value.QueryString(&text);
    return std::string(text);
}

// An expected scalar: {"type": T, "value": V} with both strings.
bool IsTagged(const Value& expected) {
    const Value* type = expected.Find("type");
    const Value* value = expected.Find("value");
    return expected.GetType() == Type::Object && expected.Size() == 2 && type != nullptr && value != nullptr
        && type->GetType() == Type::String && value->GetType() == Type::String;
}

// Returns an empty string when actual matches the toml-test description
// expected, otherwise what is wrong and where.
std::string Mismatch(const Value& actual, const Value& expected, const std::string& path) {
    if (IsTagged(expected)) {
        std::string type = Text(*expected.Find("type"));
        std::string text = Text(*expected.Find("value"));
        std::string where = path + " (" + type + " " + text + ")";

        if (type == "string") {
            std::string_view value;
            return actual.QueryString(&value) && value == text ? "" : "string differs at " + where;
        }
        if (type == "integer") {
            int64_t want = 0;
            std::from_chars(text.data(), text.data() + text.size(), want);
            int64_t value = 0;
            return actual.GetType() == Type::Int && actual.QueryInt(&value) && value == want
                ? "" : "integer differs at " + where;
        }
        if (type == "float") {
            double value = 0.0;
            if (actual.GetType() != Type::Double || !actual.QueryDouble(&value)) {
                return "not a float at " + where;
            }
            std::string_view body = text;
            bool negative = !body.empty() && body[0] == '-';
            if (!body.empty() && (body[0] == '+' || body[0] == '-')) {
                body.remove_prefix(1);
            }
            if (body == "nan") {
                return std::isnan(value) ? "" : "float differs at " + where;
            }
            double want = 0.0;
            if (body == "inf") {
                want = std::numeric_limits<double>::infinity();
            } else {
                std::from_chars(body.data(), body.data() + body.size(), want);
            }
            if (negative) {
                want = -want;
            }
            return value == want && std::signbit(value) == std::signbit(want) ? "" : "float differs at " + where;
        }
        if (type == "bool") {
            bool value = false;
            return actual.QueryBool(&value) && (value ? "true" : "false") == text ? "" : "bool differs at " + where;
        }

        // A datetime: let the reader parse the expected text too.
        DateTime::Kind kind = type == "datetime"       ? DateTime::Kind::OffsetDateTime
                            : type == "datetime-local" ? DateTime::Kind::LocalDateTime
                            : type == "date-local"     ? DateTime::Kind::LocalDate
                                                       : DateTime::Kind::LocalTime;
        Document document;
        DateTime want;
        if (!toml::Parse("v = " + text, document).Ok() || !document.Root()->Find("v")->QueryDateTime(&want)
                || want.kind != kind) {
            return "cannot read the expected datetime at " + where;
        }
        DateTime value;
        return actual.QueryDateTime(&value) && value == want ? "" : "datetime differs at " + where;
    }

    if (expected.GetType() == Type::Array) {
        if (actual.GetType() != Type::Array || actual.Size() != expected.Size()) {
            return "array differs at " + path;
        }
        const Value* element = actual.FirstChild();
        size_t index = 0;
        for (const Value* want = expected.FirstChild(); want != nullptr; want = want->Next()) {
            std::string problem = Mismatch(*element, *want, path + "[" + std::to_string(index++) + "]");
            if (!problem.empty()) {
                return problem;
            }
            element = element->Next();
        }
        return "";
    }

    if (actual.GetType() != Type::Object || actual.Size() != expected.Size()) {
        return "table differs at " + path;
    }
    for (const Value* want = expected.FirstChild(); want != nullptr; want = want->Next()) {
        const Value* member = actual.Find(want->Key());
        std::string memberPath = path + "." + std::string(want->Key());
        if (member == nullptr) {
            return "missing " + memberPath;
        }
        std::string problem = Mismatch(*member, *want, memberPath);
        if (!problem.empty()) {
            return problem;
        }
    }
    return "";
}

}  // namespace

TEST_CASE("the toml-test data is present") {
    CHECK(ListFiles("valid/").size() == 208);
    CHECK(ListFiles("invalid/").size() == 501);
}

TEST_CASE("every valid document parses to what its JSON file describes") {
    for (const std::string& path : ListFiles("valid/")) {
        CAPTURE(path);
        Document document;
        Error error = toml::Parse(ReadFile(path), document);
        INFO("error: ", ErrorName(error.code), " at line ", error.line, ", column ", error.column);
        REQUIRE(error.Ok());

        Document expected;
        REQUIRE(json::Parse(ReadFile(path.substr(0, path.size() - 5) + ".json"), expected).Ok());
        CHECK(Mismatch(*document.Root(), *expected.Root(), "root") == "");
    }
}

TEST_CASE("every valid document survives Stringify and Parse") {
    for (const std::string& path : ListFiles("valid/")) {
        CAPTURE(path);
        Document document;
        REQUIRE(toml::Parse(ReadFile(path), document).Ok());

        std::string text;
        REQUIRE(toml::Stringify(*document.Root(), &text));
        CAPTURE(text);
        Document copy;
        Error error = toml::Parse(text, copy);
        INFO("error: ", ErrorName(error.code), " at line ", error.line, ", column ", error.column);
        REQUIRE(error.Ok());
        CHECK(EqualsIgnoringOrder(*document.Root(), *copy.Root()));
    }
}

TEST_CASE("every invalid document is rejected") {
    for (const std::string& path : ListFiles("invalid/")) {
        CAPTURE(path);
        Document document;
        Error error = toml::Parse(ReadFile(path), document);
        CHECK_FALSE(error.Ok());
        CHECK(error.code != ErrorCode::Aborted);
    }
}
