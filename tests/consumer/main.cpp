#include <cstdio>
#include <string>

#include "tinycodec/json/json.h"

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
    return 0;
}
