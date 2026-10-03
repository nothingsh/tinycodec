#ifndef TINYCODEC_TOML_DATETIME_PARSER_H
#define TINYCODEC_TOML_DATETIME_PARSER_H

#include <cstddef>
#include <string_view>

#include "tinycodec/datetime.h"
#include "tinycodec/error.h"

namespace tinycodec {
namespace toml {

// Tells whether word, a token that starts a value, is meant as a date or
// time rather than a number: it holds a ':' or starts with four digits and
// a '-'.
bool IsDateTimeWord(std::string_view word);

// Parses word, the whole text of a TOML offset datetime, local datetime,
// local date or local time. A space between date and time is part of word.
//
// On success returns ErrorCode::Ok and fills *out. On failure returns:
//   UnexpectedChar   the character at *errorOffset (counted from the start
//                    of word) does not fit; *errorOffset is word.size()
//                    when word ends too early.
//   InvalidDateTime  the form is right but a field is out of range, such
//                    as February 30 or an offset of +24:00; *errorOffset
//                    is 0.
// Digits of the fraction beyond nanoseconds are dropped, not rounded.
ErrorCode ParseDateTime(std::string_view word, DateTime* out, size_t* errorOffset);

}  // namespace toml
}  // namespace tinycodec

#endif
