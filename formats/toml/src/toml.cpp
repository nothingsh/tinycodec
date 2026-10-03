#include "tinycodec/toml/toml.h"

#include "tinycodec/builder.h"
#include "tinycodec/sink.h"

namespace tinycodec {
namespace toml {

Error Parse(std::string_view text, Document& document) {
    document.Clear();
    DocumentBuilder builder(document);
    Reader reader;
    Error error = reader.Parse(text, builder);
    if (!error.Ok()) {
        document.Clear();
    }
    return error;
}

bool Stringify(const Value& value, std::string* out) {
    StringSink sink;
    Writer writer(sink);
    if (!value.Accept(writer)) {
        return false;
    }
    *out = sink.Str();
    return true;
}

}  // namespace toml
}  // namespace tinycodec
