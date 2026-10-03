#ifndef TINYCODEC_TESTS_HEX_H
#define TINYCODEC_TESTS_HEX_H

#include <string>
#include <string_view>

// Test helpers for showing binary data as text.

// Returns bytes as lowercase hex digits, two per byte, e.g. "00ff".
inline std::string ToHex(std::string_view bytes) {
    static const char DIGITS[] = "0123456789abcdef";
    std::string hex;
    for (char character : bytes) {
        unsigned char byte = static_cast<unsigned char>(character);
        hex += DIGITS[byte >> 4];
        hex += DIGITS[byte & 0x0F];
    }
    return hex;
}

// The reverse of ToHex. Spaces and '-' between the digits are skipped, so
// "cc 80" and "cc-80" both give the two bytes 0xCC 0x80. Every other
// character must be a hex digit, and the digits must come in pairs.
inline std::string FromHex(std::string_view hex) {
    auto value = [](char digit) {
        if (digit >= '0' && digit <= '9') {
            return digit - '0';
        }
        if (digit >= 'a' && digit <= 'f') {
            return digit - 'a' + 10;
        }
        return digit - 'A' + 10;
    };
    std::string bytes;
    int high = -1;   // The first digit of a pair, while waiting for the second.
    for (char digit : hex) {
        if (digit == ' ' || digit == '-') {
            continue;
        }
        if (high < 0) {
            high = value(digit);
        } else {
            bytes += static_cast<char>(high * 16 + value(digit));
            high = -1;
        }
    }
    return bytes;
}

#endif
