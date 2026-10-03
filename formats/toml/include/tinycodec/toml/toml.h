#ifndef TINYCODEC_TOML_TOML_H
#define TINYCODEC_TOML_TOML_H

#include <string>
#include <string_view>

#include "tinycodec/document.h"
#include "tinycodec/error.h"
#include "tinycodec/toml/reader.h"
#include "tinycodec/toml/writer.h"
#include "tinycodec/value.h"

namespace tinycodec {
namespace toml {

// Parses text into document, replacing whatever it held. On failure the
// document is left empty: document.Root() is null.
Error Parse(std::string_view text, Document& document);

// Writes value as TOML text into *out and returns true. Returns false,
// leaving *out alone, when value is not an object or holds something TOML
// cannot express: Null, Bytes, an integer above INT64_MAX, an invalid
// DateTime, a repeated key, or nesting deeper than kMaxDepth. (An empty
// string is valid TOML, the empty table, so it cannot signal failure.)
bool Stringify(const Value& value, std::string* out);

}  // namespace toml
}  // namespace tinycodec

#endif
