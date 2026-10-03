#include "doctest.h"

#include <cstdint>
#include <string>

#include "hex.h"
#include "tinycodec/builder.h"
#include "tinycodec/msgpack/msgpack.h"
#include "tinycodec/sink.h"

using namespace tinycodec;

namespace {

// A tree with every type:
// {"null": nil, "bool": true, "int": -5, "uint": UINT64_MAX, "double": 0.25,
//  "string": "s", "bytes": bin 00 ff, "array": [1, []], "object": {"": {}}}
Value* BuildSample(Document& document) {
    Value* array = document.NewArray();
    array->Append(document.NewInt(1));
    array->Append(document.NewArray());
    Value* object = document.NewObject();
    object->Set("", document.NewObject());

    Value* root = document.NewObject();
    root->Set("null", document.NewNull());
    root->Set("bool", document.NewBool(true));
    root->Set("int", document.NewInt(-5));
    root->Set("uint", document.NewUint(UINT64_MAX));
    root->Set("double", document.NewDouble(0.25));
    root->Set("string", document.NewString("s"));
    root->Set("bytes", document.NewBytes(std::string("\0\xFF", 2)));
    root->Set("array", array);
    root->Set("object", object);
    return root;
}

}  // namespace

TEST_CASE("Parse builds a tree that can be queried") {
    Document document;
    Error error = msgpack::Parse(FromHex("82" "a161" "92" "01" "c40100" "a162" "cb3fd0000000000000"), document);
    REQUIRE(error.Ok());

    const Value* root = document.Root();
    REQUIRE(root != nullptr);
    REQUIRE(root->Find("a") != nullptr);
    CHECK(root->Find("a")->Size() == 2);
    std::string_view bytes;
    CHECK(root->Find("a")->At(1)->QueryBytes(&bytes));
    CHECK(ToHex(bytes) == "00");
    double number = 0.0;
    CHECK(root->Find("b")->QueryDouble(&number));
    CHECK(number == 0.25);
}

TEST_CASE("Parse replaces the previous content of the document") {
    Document document;
    REQUIRE(msgpack::Parse(FromHex("01"), document).Ok());
    REQUIRE(msgpack::Parse(FromHex("a178"), document).Ok());
    std::string_view text;
    CHECK(document.Root()->QueryString(&text));
    CHECK(text == "x");
}

TEST_CASE("a failed Parse reports the error and leaves the document empty") {
    Document document;
    REQUIRE(msgpack::Parse(FromHex("01"), document).Ok());

    Error error = msgpack::Parse(FromHex("92" "01"), document);
    CHECK(error.code == ErrorCode::UnexpectedEnd);
    CHECK(error.offset == 2);
    CHECK(document.Root() == nullptr);

    error = msgpack::Parse(FromHex("d6ff00000000"), document);
    CHECK(error.code == ErrorCode::Unsupported);
    CHECK(document.Root() == nullptr);
}

TEST_CASE("Encode and Parse round-trip every type") {
    Document source;
    Value* sample = BuildSample(source);
    std::string encoded = msgpack::Encode(*sample);
    REQUIRE_FALSE(encoded.empty());

    Document copy;
    REQUIRE(msgpack::Parse(encoded, copy).Ok());
    CHECK(copy.Root()->Equals(*sample));
    CHECK(msgpack::Encode(*copy.Root()) == encoded);
}

TEST_CASE("Encode works on any subtree") {
    Document document;
    Value* sample = BuildSample(document);
    CHECK(ToHex(msgpack::Encode(*sample->Find("array"))) == "920190");
    CHECK(ToHex(msgpack::Encode(*sample->Find("bytes"))) == "c40200ff");
}

TEST_CASE("Reader to Writer gives the same bytes as going through a Document") {
    Document document;
    std::string inputs[] = {
        msgpack::Encode(*BuildSample(document)),
        // The same values in longer encodings than necessary.
        FromHex("dc0003" "d000" "cd0001" "d9026162"),
        FromHex("df00000001" "da000161" "c6000000020102"),
        FromHex("ca3fc00000"),
    };
    for (const std::string& input : inputs) {
        CAPTURE(ToHex(input));
        msgpack::Reader reader;

        StringSink direct;
        msgpack::Writer directWriter(direct);
        REQUIRE(reader.Parse(input, directWriter).Ok());

        Document parsed;
        DocumentBuilder builder(parsed);
        REQUIRE(reader.Parse(input, builder).Ok());
        StringSink viaDocument;
        msgpack::Writer documentWriter(viaDocument);
        REQUIRE(parsed.Root()->Accept(documentWriter));

        CHECK(ToHex(direct.Str()) == ToHex(viaDocument.Str()));
    }
}

TEST_CASE("re-encoding gives the shortest form") {
    Document document;
    REQUIRE(msgpack::Parse(FromHex("dc0003" "d000" "cd0001" "d9026162"), document).Ok());
    CHECK(ToHex(msgpack::Encode(*document.Root())) == "93" "00" "01" "a26162");
    REQUIRE(msgpack::Parse(FromHex("ca3fc00000"), document).Ok());
    CHECK(ToHex(msgpack::Encode(*document.Root())) == "cb3ff8000000000000");
}
