#include "tinycodec/msgpack/msgpack.h"

#include "tinycodec/builder.h"
#include "tinycodec/sink.h"

namespace tinycodec {
namespace msgpack {

Error Parse(std::string_view data, Document& document) {
    document.Clear();
    DocumentBuilder builder(document);
    Reader reader;
    Error error = reader.Parse(data, builder);
    if (!error.Ok()) {
        document.Clear();
    }
    return error;
}

std::string Encode(const Value& value) {
    StringSink sink;
    Writer writer(sink);
    if (!value.Accept(writer)) {
        return std::string();
    }
    return sink.Str();
}

}  // namespace msgpack
}  // namespace tinycodec
