#ifndef TINYCODEC_JSON_NUMBER_PARSER_H
#define TINYCODEC_JSON_NUMBER_PARSER_H

#include <cstdint>

namespace tinycodec {
namespace json {

enum class NumberKind { Int, Uint, Double };

// The result of parsing a number. Only the field named by `kind` is set.
struct Number {
    NumberKind kind = NumberKind::Int;
    int64_t intValue = 0;
    uint64_t uintValue = 0;
    double doubleValue = 0.0;
};

// Parses the JSON number that starts at begin; end is the end of the input.
//
// On success returns true, fills *out and sets *stop just past the number.
// On failure returns false and sets *stop to where the problem is: the
// offending character, or begin when the number overflows to infinity.
//
// An integer that fits int64_t is an Int; a larger one that fits uint64_t
// is a Uint; anything else is a Double. "-0" is the Double -0.0.
bool ParseNumber(const char* begin, const char* end, Number* out, const char** stop);

}  // namespace json
}  // namespace tinycodec

#endif
