#ifndef TINYCODEC_TOML_STRING_PARSER_H
#define TINYCODEC_TOML_STRING_PARSER_H

#include <string>
#include <string_view>

#include "tinycodec/error.h"

namespace tinycodec {
namespace toml {

// Parses the TOML string whose opening delimiter (", """, ' or ''') starts
// at begin; end is the end of the input. The input is assumed to be valid
// UTF-8 already.
//
// On success returns ErrorCode::Ok, sets *stop just past the closing
// delimiter and *multiline to whether it was a """ or ''' string, and sets
// *out to the content: a view of the input when nothing had to be decoded,
// otherwise a view of *scratch, which then holds the decoded text.
//
// On failure returns the error and sets *stop to its position:
//   UnexpectedEnd   the input ended inside the string; *stop is end.
//   UnexpectedChar  a control character other than tab, a line break in a
//                   single-line string, a CR not followed by LF, or a sixth
//                   quote in a row in a multi-line string; *stop is that
//                   character.
//   InvalidEscape   an unknown escape, a \u or \U with too few hex digits
//                   or a code point that is not a Unicode scalar value, or
//                   a backslash followed by spaces that do not end the
//                   line; *stop is the backslash.
ErrorCode ParseString(const char* begin, const char* end, std::string* scratch,
                      std::string_view* out, bool* multiline, const char** stop);

}  // namespace toml
}  // namespace tinycodec

#endif
