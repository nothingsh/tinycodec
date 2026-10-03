#ifndef TINYCODEC_TOML_NUMBER_PARSER_H
#define TINYCODEC_TOML_NUMBER_PARSER_H

#include <cstdint>
#include <string_view>

namespace tinycodec {
namespace toml {

enum class NumberKind { Int, Double };

// The result of parsing a number. Only the field named by `kind` is set.
struct Number {
    NumberKind kind = NumberKind::Int;
    int64_t intValue = 0;
    double doubleValue = 0.0;
};

// Parses word, the whole text of a TOML integer or float, such as "1_000",
// "0xff", "-3.5e+2" or "inf". Returns true and fills *out on success.
// Returns false when word is not a valid integer or float, when an integer
// is outside int64_t, or when a float overflows to infinity. A float too
// close to zero to represent becomes zero, with its sign.
bool ParseNumber(std::string_view word, Number* out);

}  // namespace toml
}  // namespace tinycodec

#endif
