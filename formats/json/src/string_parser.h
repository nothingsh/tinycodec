#ifndef TINYCODEC_JSON_STRING_PARSER_H
#define TINYCODEC_JSON_STRING_PARSER_H

#include <string>

#include "tinycodec/error.h"

namespace tinycodec {
namespace json {

// Decodes the body of a JSON string. begin points just past the opening
// quote; end is the end of the input. *out is cleared and then receives the
// decoded UTF-8 bytes.
//
// On success returns ErrorCode::Ok and sets *stop just past the closing
// quote. On failure returns the error and sets *stop to its position:
//   UnexpectedEnd   the input ended inside the string; *stop is end.
//   UnexpectedChar  an unescaped control character; *stop is that character.
//   InvalidEscape   a bad escape or unpaired surrogate; *stop is its backslash.
//   InvalidUtf8     a malformed byte sequence; *stop is its first byte.
ErrorCode ParseString(const char* begin, const char* end, std::string* out, const char** stop);

}  // namespace json
}  // namespace tinycodec

#endif
