#include <cstdio>
#include <string>

#include "tinycodec/json/json.h"
#include "tinycodec/msgpack/msgpack.h"
#include "tinycodec/toml/toml.h"

int main() {
    tinycodec::Document document;
    tinycodec::Error error = tinycodec::json::Parse("{\"answer\": [42]}", document);
    if (!error.Ok()) {
        std::fprintf(stderr, "parse failed: %s\n", tinycodec::ErrorName(error.code));
        return 1;
    }

    std::string text = tinycodec::json::Stringify(*document.Root());
    if (text != "{\"answer\":[42]}") {
        std::fprintf(stderr, "unexpected output: %s\n", text.c_str());
        return 1;
    }

    // {"answer":[42]} as MessagePack.
    std::string encoded = tinycodec::msgpack::Encode(*document.Root());
    if (encoded != "\x81\xA6" "answer" "\x91\x2A") {
        std::fprintf(stderr, "unexpected MessagePack output\n");
        return 1;
    }
    tinycodec::Document decoded;
    error = tinycodec::msgpack::Parse(encoded, decoded);
    if (!error.Ok() || !decoded.Root()->Equals(*document.Root())) {
        std::fprintf(stderr, "MessagePack round trip failed\n");
        return 1;
    }

    std::string toml;
    if (!tinycodec::toml::Stringify(*document.Root(), &toml) || toml != "answer = [42]\n") {
        std::fprintf(stderr, "unexpected TOML output: %s\n", toml.c_str());
        return 1;
    }
    error = tinycodec::toml::Parse(toml, decoded);
    if (!error.Ok() || !decoded.Root()->Equals(*document.Root())) {
        std::fprintf(stderr, "TOML round trip failed\n");
        return 1;
    }
    return 0;
}
