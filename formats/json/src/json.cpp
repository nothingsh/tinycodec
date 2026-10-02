#include "tinycodec/json/json.h"

#include "tinycodec/builder.h"
#include "tinycodec/sink.h"

namespace tinycodec {
namespace json {

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

std::string Stringify(const Value& value, WriterOptions options) {
    StringSink sink;
    Writer writer(sink, options);
    if (!value.Accept(writer)) {
        return std::string();
    }
    return sink.Str();
}

}  // namespace json
}  // namespace tinycodec
