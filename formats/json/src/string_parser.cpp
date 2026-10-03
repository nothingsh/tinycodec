#include "string_parser.h"

#include "tinycodec/utf8.h"

namespace tinycodec {
namespace json {

namespace {

void AppendUtf8(unsigned codePoint, std::string* out) {
    if (codePoint < 0x80) {
        out->push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        out->push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        out->push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint < 0x10000) {
        out->push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        out->push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        out->push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        out->push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

// Reads the four hex digits of a \u escape. Returns Ok, UnexpectedEnd when
// the input ends first, or InvalidEscape for a character that is not a hex
// digit.
ErrorCode ParseHex4(const char* cursor, const char* end, unsigned* out) {
    unsigned value = 0;
    for (int i = 0; i < 4; ++i, ++cursor) {
        if (cursor == end) {
            return ErrorCode::UnexpectedEnd;
        }
        char character = *cursor;
        unsigned digit = 0;
        if (character >= '0' && character <= '9') {
            digit = static_cast<unsigned>(character - '0');
        } else if (character >= 'a' && character <= 'f') {
            digit = static_cast<unsigned>(character - 'a') + 10;
        } else if (character >= 'A' && character <= 'F') {
            digit = static_cast<unsigned>(character - 'A') + 10;
        } else {
            return ErrorCode::InvalidEscape;
        }
        value = value * 16 + digit;
    }
    *out = value;
    return ErrorCode::Ok;
}

// Decodes a \uXXXX escape, or a surrogate pair of two of them. cursor
// points just past the "\u". On success sets *next past everything read.
ErrorCode ParseUnicodeEscape(const char* cursor, const char* end, std::string* out, const char** next) {
    unsigned unit = 0;
    ErrorCode code = ParseHex4(cursor, end, &unit);
    if (code != ErrorCode::Ok) {
        return code;
    }
    cursor += 4;

    if (unit >= 0xDC00 && unit <= 0xDFFF) {
        return ErrorCode::InvalidEscape;  // A low surrogate with no high one before it.
    }
    if (unit >= 0xD800 && unit <= 0xDBFF) {
        // A high surrogate must be followed by "\u" and a low surrogate.
        if (cursor == end) {
            return ErrorCode::UnexpectedEnd;
        }
        if (*cursor != '\\') {
            return ErrorCode::InvalidEscape;
        }
        if (cursor + 1 == end) {
            return ErrorCode::UnexpectedEnd;
        }
        if (cursor[1] != 'u') {
            return ErrorCode::InvalidEscape;
        }
        unsigned low = 0;
        code = ParseHex4(cursor + 2, end, &low);
        if (code != ErrorCode::Ok) {
            return code;
        }
        if (low < 0xDC00 || low > 0xDFFF) {
            return ErrorCode::InvalidEscape;
        }
        cursor += 6;
        unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
    }

    AppendUtf8(unit, out);
    *next = cursor;
    return ErrorCode::Ok;
}

// Decodes one escape sequence. cursor points at the backslash. On success
// sets *next past the escape.
ErrorCode ParseEscape(const char* cursor, const char* end, std::string* out, const char** next) {
    ++cursor;  // The backslash.
    if (cursor == end) {
        return ErrorCode::UnexpectedEnd;
    }
    switch (*cursor) {
    case '"':  out->push_back('"'); break;
    case '\\': out->push_back('\\'); break;
    case '/':  out->push_back('/'); break;
    case 'b':  out->push_back('\b'); break;
    case 'f':  out->push_back('\f'); break;
    case 'n':  out->push_back('\n'); break;
    case 'r':  out->push_back('\r'); break;
    case 't':  out->push_back('\t'); break;
    case 'u':
        return ParseUnicodeEscape(cursor + 1, end, out, next);
    default:
        return ErrorCode::InvalidEscape;
    }
    *next = cursor + 1;
    return ErrorCode::Ok;
}

}  // namespace

ErrorCode ParseString(const char* begin, const char* end, std::string* out, const char** stop) {
    out->clear();
    const char* cursor = begin;
    for (;;) {
        if (cursor == end) {
            *stop = end;
            return ErrorCode::UnexpectedEnd;
        }

        unsigned char byte = static_cast<unsigned char>(*cursor);
        if (byte == '"') {
            *stop = cursor + 1;
            return ErrorCode::Ok;
        }
        if (byte == '\\') {
            const char* next = nullptr;
            ErrorCode code = ParseEscape(cursor, end, out, &next);
            if (code != ErrorCode::Ok) {
                *stop = code == ErrorCode::UnexpectedEnd ? end : cursor;
                return code;
            }
            cursor = next;
        } else if (byte < 0x20) {
            *stop = cursor;
            return ErrorCode::UnexpectedChar;
        } else if (byte < 0x80) {
            out->push_back(static_cast<char>(byte));
            ++cursor;
        } else {
            size_t length = Utf8SequenceLength(cursor, end);
            if (length == 0) {
                *stop = cursor;
                return ErrorCode::InvalidUtf8;
            }
            out->append(cursor, length);
            cursor += length;
        }
    }
}

}  // namespace json
}  // namespace tinycodec
