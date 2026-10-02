#include "number_parser.h"

#include <charconv>
#include <system_error>

namespace tinycodec {
namespace json {

namespace {

bool IsDigit(char character) {
    return character >= '0' && character <= '9';
}

const char* SkipDigits(const char* cursor, const char* end) {
    while (cursor != end && IsDigit(*cursor)) {
        ++cursor;
    }
    return cursor;
}

// Tells whether a number that is out of range for double is too large
// (true) or too close to zero (false). [begin, end) must be a valid JSON
// number.
//
// Write the number as 0.d1d2d3... x 10^exponent with d1 != 0. Its size is
// then decided by that exponent alone, and a number that is out of range
// has an exponent far from zero, so the sign of the exponent is enough.
bool IsTooLarge(const char* begin, const char* end) {
    const char* cursor = begin;
    if (*cursor == '-') {
        ++cursor;
    }

    long long digitsBeforePoint = 0;
    long long zerosBeforeFirstDigit = 0;   // Leading zeros, on both sides of the point.
    bool seenNonZero = false;
    bool afterPoint = false;
    for (; cursor != end && *cursor != 'e' && *cursor != 'E'; ++cursor) {
        if (*cursor == '.') {
            afterPoint = true;
            continue;
        }
        if (!afterPoint) {
            ++digitsBeforePoint;
        }
        if (*cursor != '0') {
            seenNonZero = true;
        } else if (!seenNonZero) {
            ++zerosBeforeFirstDigit;
        }
    }

    long long exponent = 0;
    if (cursor != end) {
        ++cursor;  // 'e' or 'E'
        bool negative = false;
        if (*cursor == '+' || *cursor == '-') {
            negative = *cursor == '-';
            ++cursor;
        }
        const long long LIMIT = 1000000000000LL;  // Far beyond any input length.
        for (; cursor != end; ++cursor) {
            if (exponent < LIMIT) {
                exponent = exponent * 10 + (*cursor - '0');
            }
        }
        if (negative) {
            exponent = -exponent;
        }
    }

    return digitsBeforePoint - zerosBeforeFirstDigit + exponent > 0;
}

}  // namespace

bool ParseNumber(const char* begin, const char* end, Number* out, const char** stop) {
    // Check the grammar first:
    //   number = [ "-" ] int [ "." digits ] [ ("e" | "E") [ "+" | "-" ] digits ]
    //   int    = "0" | digit1-9 *digit
    const char* cursor = begin;
    bool negative = false;
    if (cursor != end && *cursor == '-') {
        negative = true;
        ++cursor;
    }

    if (cursor == end || !IsDigit(*cursor)) {
        *stop = cursor;
        return false;
    }
    if (*cursor == '0') {
        ++cursor;  // A leading zero stands alone: "01" is "0" followed by "1".
    } else {
        cursor = SkipDigits(cursor, end);
    }

    bool isInteger = true;
    if (cursor != end && *cursor == '.') {
        isInteger = false;
        ++cursor;
        if (cursor == end || !IsDigit(*cursor)) {
            *stop = cursor;
            return false;
        }
        cursor = SkipDigits(cursor, end);
    }
    if (cursor != end && (*cursor == 'e' || *cursor == 'E')) {
        isInteger = false;
        ++cursor;
        if (cursor != end && (*cursor == '+' || *cursor == '-')) {
            ++cursor;
        }
        if (cursor == end || !IsDigit(*cursor)) {
            *stop = cursor;
            return false;
        }
        cursor = SkipDigits(cursor, end);
    }

    // [begin, cursor) is a valid number. Now convert it.
    *stop = cursor;

    bool isNegativeZero = negative && cursor - begin == 2 && begin[1] == '0';
    if (isInteger && !isNegativeZero) {
        if (negative) {
            int64_t value = 0;
            if (std::from_chars(begin, cursor, value).ec == std::errc()) {
                out->kind = NumberKind::Int;
                out->intValue = value;
                return true;
            }
        } else {
            uint64_t value = 0;
            if (std::from_chars(begin, cursor, value).ec == std::errc()) {
                if (value <= static_cast<uint64_t>(INT64_MAX)) {
                    out->kind = NumberKind::Int;
                    out->intValue = static_cast<int64_t>(value);
                } else {
                    out->kind = NumberKind::Uint;
                    out->uintValue = value;
                }
                return true;
            }
        }
        // Too many digits for 64 bits: fall through and read it as a double.
    }

    double value = 0.0;
    std::errc status = std::from_chars(begin, cursor, value).ec;
    if (status == std::errc::result_out_of_range) {
        if (IsTooLarge(begin, cursor)) {
            *stop = begin;
            return false;
        }
        value = negative ? -0.0 : 0.0;  // Too close to zero to represent.
    }
    out->kind = NumberKind::Double;
    out->doubleValue = value;
    return true;
}

}  // namespace json
}  // namespace tinycodec
