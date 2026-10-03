#include "string_parser.h"

#include <cstdint>

namespace tinycodec {
namespace toml {

namespace {

void AppendUtf8(uint32_t codePoint, std::string* out) {
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

int HexValue(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

bool IsSpace(char character) {
    return character == ' ' || character == '\t';
}

// Decodes the escape whose backslash is at *cursor, appending to *out and
// moving *cursor past it. In a multi-line string a backslash that ends the
// line removes it and all whitespace and line breaks after it.
ErrorCode ParseEscape(const char** cursor, const char* end, bool multiline,
                      std::string* out, const char** stop) {
    const char* backslash = *cursor;
    const char* next = backslash + 1;
    if (next == end) {
        *stop = end;
        return ErrorCode::UnexpectedEnd;
    }

    switch (*next) {
    case 'b':  out->push_back('\b'); *cursor = next + 1; return ErrorCode::Ok;
    case 't':  out->push_back('\t'); *cursor = next + 1; return ErrorCode::Ok;
    case 'n':  out->push_back('\n'); *cursor = next + 1; return ErrorCode::Ok;
    case 'f':  out->push_back('\f'); *cursor = next + 1; return ErrorCode::Ok;
    case 'r':  out->push_back('\r'); *cursor = next + 1; return ErrorCode::Ok;
    case '"':  out->push_back('"');  *cursor = next + 1; return ErrorCode::Ok;
    case '\\': out->push_back('\\'); *cursor = next + 1; return ErrorCode::Ok;
    case 'u':
    case 'U': {
        int digits = *next == 'u' ? 4 : 8;
        uint32_t codePoint = 0;
        const char* digit = next + 1;
        for (int i = 0; i < digits; ++i, ++digit) {
            if (digit == end) {
                *stop = end;
                return ErrorCode::UnexpectedEnd;
            }
            int value = HexValue(*digit);
            if (value < 0) {
                *stop = backslash;
                return ErrorCode::InvalidEscape;
            }
            codePoint = codePoint * 16 + static_cast<uint32_t>(value);
        }
        if ((codePoint >= 0xD800 && codePoint <= 0xDFFF) || codePoint > 0x10FFFF) {
            *stop = backslash;
            return ErrorCode::InvalidEscape;
        }
        AppendUtf8(codePoint, out);
        *cursor = digit;
        return ErrorCode::Ok;
    }
    default:
        break;
    }

    if (!multiline || (!IsSpace(*next) && *next != '\n' && *next != '\r')) {
        *stop = backslash;
        return ErrorCode::InvalidEscape;
    }
    // A line-ending backslash: only whitespace may follow it on its line.
    while (next != end && IsSpace(*next)) {
        ++next;
    }
    if (next == end) {
        *stop = end;
        return ErrorCode::UnexpectedEnd;
    }
    if (*next != '\n' && *next != '\r') {
        *stop = backslash;
        return ErrorCode::InvalidEscape;
    }
    // Skip whitespace and line breaks up to the next content. A CR without
    // its LF is left for the caller to reject.
    for (;;) {
        if (next != end && (IsSpace(*next) || *next == '\n')) {
            ++next;
        } else if (end - next >= 2 && next[0] == '\r' && next[1] == '\n') {
            next += 2;
        } else {
            break;
        }
    }
    *cursor = next;
    return ErrorCode::Ok;
}

}  // namespace

ErrorCode ParseString(const char* begin, const char* end, std::string* scratch,
                      std::string_view* out, bool* multiline, const char** stop) {
    const char quote = *begin;
    const bool isLiteral = quote == '\'';
    const char* cursor = begin + 1;
    const bool isMultiline = end - cursor >= 2 && cursor[0] == quote && cursor[1] == quote;
    if (isMultiline) {
        cursor += 2;
        // A line break right after the opening delimiter is not content.
        if (cursor != end && *cursor == '\n') {
            cursor += 1;
        } else if (end - cursor >= 2 && cursor[0] == '\r' && cursor[1] == '\n') {
            cursor += 2;
        }
    }
    *multiline = isMultiline;

    // The content is a view of the input until the first escape; from then
    // on it is built in *scratch.
    const char* contentStart = cursor;
    bool decoding = false;
    for (;;) {
        if (cursor == end) {
            *stop = end;
            return ErrorCode::UnexpectedEnd;
        }
        const char character = *cursor;

        if (character == quote) {
            size_t quotes = 1;
            if (isMultiline) {
                while (cursor + quotes != end && cursor[quotes] == quote) {
                    ++quotes;
                }
                if (quotes < 3) {
                    // One or two quotes are content.
                    if (decoding) {
                        scratch->append(cursor, quotes);
                    }
                    cursor += quotes;
                    continue;
                }
                if (quotes > 5) {
                    *stop = cursor + 5;
                    return ErrorCode::UnexpectedChar;
                }
                // Up to two quotes before the closing delimiter are content.
                size_t content = quotes - 3;
                if (decoding) {
                    scratch->append(cursor, content);
                }
                cursor += content;
                quotes = 3;
            }
            *out = decoding ? std::string_view(*scratch)
                            : std::string_view(contentStart, static_cast<size_t>(cursor - contentStart));
            *stop = cursor + quotes;
            return ErrorCode::Ok;
        }

        if (character == '\\' && !isLiteral) {
            if (!decoding) {
                scratch->assign(contentStart, cursor);
                decoding = true;
            }
            ErrorCode code = ParseEscape(&cursor, end, isMultiline, scratch, stop);
            if (code != ErrorCode::Ok) {
                return code;
            }
            continue;
        }

        size_t length = 1;
        if (character == '\n') {
            if (!isMultiline) {
                *stop = cursor;
                return ErrorCode::UnexpectedChar;
            }
        } else if (character == '\r') {
            if (!isMultiline || end - cursor < 2 || cursor[1] != '\n') {
                *stop = cursor;
                return ErrorCode::UnexpectedChar;
            }
            length = 2;
        } else {
            unsigned char byte = static_cast<unsigned char>(character);
            if ((byte < 0x20 && byte != '\t') || byte == 0x7F) {
                *stop = cursor;
                return ErrorCode::UnexpectedChar;
            }
        }
        if (decoding) {
            scratch->append(cursor, length);
        }
        cursor += length;
    }
}

}  // namespace toml
}  // namespace tinycodec
