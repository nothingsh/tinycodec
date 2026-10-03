#ifndef TINYCODEC_JSON_JSON_H
#define TINYCODEC_JSON_JSON_H

#include <string>
#include <string_view>

#include "tinycodec/document.h"
#include "tinycodec/error.h"
#include "tinycodec/json/reader.h"
#include "tinycodec/json/writer.h"
#include "tinycodec/value.h"

namespace tinycodec {
namespace json {

// Parses text into document, replacing whatever it held. On failure the
// document is left empty: document.Root() is null.
Error Parse(std::string_view text, Document& document);

// Returns value as JSON text. Returns an empty string, which is never valid
// JSON, when the tree contains a NaN or an infinite Double, or Bytes.
std::string Stringify(const Value& value, WriterOptions options = {});

}  // namespace json
}  // namespace tinycodec

#endif
