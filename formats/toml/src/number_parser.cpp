#include "number_parser.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <string>
#include <system_error>

namespace tinycodec {
namespace toml {

namespace {

bool IsDigit(char character, int base) {
    switch (base) {
    case 2:
        return character == '0' || character == '1';
    case 8:
        return character >= '0' && character <= '7';
    case 16:
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')
            || (character >= 'A' && character <= 'F');
    default:
        return character >= '0' && character <= '9';
    }
}

// Checks that text is one or more digits of the given base, with single
// underscores allowed only between two digits, and appends the digits
// without the underscores to *digits.
bool TakeDigits(std::string_view text, int base, std::string* digits) {
    bool afterDigit = false;
    for (char character : text) {
        if (character == '_') {
            if (!afterDigit) {
                return false;
            }
            afterDigit = false;
            continue;
        }
        if (!IsDigit(character, base)) {
            return false;
        }
        digits->push_back(character);
        afterDigit = true;
    }
    return afterDigit;
}

// Tells whether a float that is out of range for double is too large
// (true) or too close to zero (false). text is the float without
// underscores, as passed to from_chars: "-1.5e300" and the like.
//
// Written as 0.d1d2d3... x 10^exponent with d1 != 0, a number's size is
// decided by that exponent alone, and an out of range number has an
// exponent far from zero, so its sign is enough.
bool IsTooLarge(const std::string& text) {
    size_t cursor = text[0] == '-' ? 1 : 0;
    long long digitsBeforePoint = 0;
    long long zerosBeforeFirstDigit = 0;   // Leading zeros, on both sides of the point.
    bool seenNonZero = false;
    bool afterPoint = false;
    for (; cursor < text.size() && text[cursor] != 'e'; ++cursor) {
        if (text[cursor] == '.') {
            afterPoint = true;
            continue;
        }
        if (!afterPoint) {
            ++digitsBeforePoint;
        }
        if (text[cursor] != '0') {
            seenNonZero = true;
        } else if (!seenNonZero) {
            ++zerosBeforeFirstDigit;
        }
    }

    long long exponent = 0;
    if (cursor < text.size()) {
        ++cursor;  // 'e'
        bool negative = text[cursor] == '-';
        if (negative) {
            ++cursor;
        }
        const long long LIMIT = 1000000000000LL;  // Far beyond any input length.
        for (; cursor < text.size(); ++cursor) {
            if (exponent < LIMIT) {
                exponent = exponent * 10 + (text[cursor] - '0');
            }
        }
        if (negative) {
            exponent = -exponent;
        }
    }
    return digitsBeforePoint - zerosBeforeFirstDigit + exponent > 0;
}

}  // namespace

bool ParseNumber(std::string_view word, Number* out) {
    std::string_view body = word;
    char sign = 0;
    if (!body.empty() && (body[0] == '+' || body[0] == '-')) {
        sign = body[0];
        body.remove_prefix(1);
    }

    if (body == "inf" || body == "nan") {
        double value = body == "inf" ? std::numeric_limits<double>::infinity()
                                     : std::numeric_limits<double>::quiet_NaN();
        out->kind = NumberKind::Double;
        out->doubleValue = sign == '-' ? -value : value;
        return true;
    }

    // Hexadecimal, octal and binary integers: no sign, lowercase prefix.
    if (body.size() >= 2 && body[0] == '0' && (body[1] == 'x' || body[1] == 'o' || body[1] == 'b')) {
        int base = body[1] == 'x' ? 16 : body[1] == 'o' ? 8 : 2;
        std::string digits;
        if (sign != 0 || !TakeDigits(body.substr(2), base, &digits)) {
            return false;
        }
        uint64_t value = 0;
        const char* last = digits.data() + digits.size();
        std::from_chars_result result = std::from_chars(digits.data(), last, value, base);
        if (result.ec != std::errc() || value > static_cast<uint64_t>(INT64_MAX)) {
            return false;
        }
        out->kind = NumberKind::Int;
        out->intValue = static_cast<int64_t>(value);
        return true;
    }

    // Decimal: an integer part, then an optional fraction and exponent.
    // `text` collects the number in the form from_chars accepts.
    size_t integerEnd = body.find_first_of(".eE");
    std::string_view integerPart = body.substr(0, integerEnd);
    std::string_view rest = integerEnd == std::string_view::npos ? std::string_view() : body.substr(integerEnd);
    if (integerPart.size() > 1 && integerPart[0] == '0') {
        return false;   // No leading zeros.
    }
    std::string text = sign == '-' ? "-" : "";
    if (!TakeDigits(integerPart, 10, &text)) {
        return false;
    }

    if (rest.empty()) {
        int64_t value = 0;
        std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
        if (result.ec != std::errc()) {
            return false;
        }
        out->kind = NumberKind::Int;
        out->intValue = value;
        return true;
    }

    if (rest[0] == '.') {
        size_t fractionEnd = rest.find_first_of("eE");
        text += '.';
        if (!TakeDigits(rest.substr(1, fractionEnd == std::string_view::npos ? fractionEnd : fractionEnd - 1),
                        10, &text)) {
            return false;
        }
        rest = fractionEnd == std::string_view::npos ? std::string_view() : rest.substr(fractionEnd);
    }
    if (!rest.empty()) {
        // rest starts with 'e' or 'E'. The exponent may have leading zeros.
        std::string_view exponent = rest.substr(1);
        text += 'e';
        if (!exponent.empty() && (exponent[0] == '+' || exponent[0] == '-')) {
            if (exponent[0] == '-') {
                text += '-';
            }
            exponent.remove_prefix(1);
        }
        if (!TakeDigits(exponent, 10, &text)) {
            return false;
        }
    }

    double value = 0.0;
    std::errc status = std::from_chars(text.data(), text.data() + text.size(), value).ec;
    if (status == std::errc::result_out_of_range) {
        if (IsTooLarge(text)) {
            return false;
        }
        value = sign == '-' ? -0.0 : 0.0;
    } else if (status != std::errc()) {
        return false;
    }
    out->kind = NumberKind::Double;
    out->doubleValue = value;
    return true;
}

}  // namespace toml
}  // namespace tinycodec
