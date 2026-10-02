#include "tinycodec/json/reader.h"

#include "number_parser.h"
#include "string_parser.h"

namespace tinycodec {
namespace json {

Error Reader::Parse(std::string_view text, Visitor& visitor) {
    _begin = text.data();
    _cursor = _begin;
    _end = _begin + text.size();
    _visitor = &visitor;
    _error = Error();

    SkipWhitespace();
    if (!ParseValue(0)) {
        return _error;
    }
    SkipWhitespace();
    if (_cursor != _end) {
        Fail(ErrorCode::UnexpectedChar, _cursor);
    }
    return _error;
}

void Reader::SkipWhitespace() {
    while (_cursor != _end
            && (*_cursor == ' ' || *_cursor == '\t' || *_cursor == '\n' || *_cursor == '\r')) {
        ++_cursor;
    }
}

// Records an error at position and returns false, so that callers can
// write `return Fail(...)`.
bool Reader::Fail(ErrorCode code, const char* position) {
    _error.code = code;
    _error.offset = static_cast<size_t>(position - _begin);
    // Line and column are only needed for errors, so they are worked out
    // here instead of being tracked while parsing.
    _error.line = 1;
    _error.column = 1;
    for (const char* cursor = _begin; cursor != position; ++cursor) {
        if (*cursor == '\n') {
            ++_error.line;
            _error.column = 1;
        } else {
            ++_error.column;
        }
    }
    return false;
}

// Turns the result of a visitor call into this parser's result: a rejected
// event becomes an Aborted error at the token that caused it.
bool Reader::Delivered(bool accepted, const char* tokenStart) {
    return accepted || Fail(ErrorCode::Aborted, tokenStart);
}

bool Reader::ParseValue(int depth) {
    if (_cursor == _end) {
        return Fail(ErrorCode::UnexpectedEnd, _cursor);
    }

    const char* start = _cursor;
    switch (*_cursor) {
    case 'n':
        return ParseLiteral("null") && Delivered(_visitor->Null(), start);
    case 't':
        return ParseLiteral("true") && Delivered(_visitor->Bool(true), start);
    case 'f':
        return ParseLiteral("false") && Delivered(_visitor->Bool(false), start);
    case '"':
        return ParseStringToken(false);
    case '[':
        return ParseArray(depth);
    case '{':
        return ParseObject(depth);
    default:
        if (*_cursor == '-' || (*_cursor >= '0' && *_cursor <= '9')) {
            return ParseNumberToken();
        }
        return Fail(ErrorCode::UnexpectedChar, _cursor);
    }
}

bool Reader::ParseLiteral(std::string_view literal) {
    for (char expected : literal) {
        if (_cursor == _end) {
            return Fail(ErrorCode::UnexpectedEnd, _cursor);
        }
        if (*_cursor != expected) {
            return Fail(ErrorCode::UnexpectedChar, _cursor);
        }
        ++_cursor;
    }
    return true;
}

bool Reader::ParseNumberToken() {
    const char* start = _cursor;
    Number number;
    const char* stop = nullptr;
    if (!ParseNumber(_cursor, _end, &number, &stop)) {
        return Fail(ErrorCode::InvalidNumber, stop);
    }
    _cursor = stop;

    switch (number.kind) {
    case NumberKind::Int:
        return Delivered(_visitor->Int(number.intValue), start);
    case NumberKind::Uint:
        return Delivered(_visitor->Uint(number.uintValue), start);
    case NumberKind::Double:
        return Delivered(_visitor->Double(number.doubleValue), start);
    }
    return false;
}

// Parses the string at the cursor and reports it as a Key or a String.
bool Reader::ParseStringToken(bool isKey) {
    const char* start = _cursor;
    const char* stop = nullptr;
    ErrorCode code = ParseString(_cursor + 1, _end, &_scratch, &stop);
    if (code != ErrorCode::Ok) {
        return Fail(code, stop);
    }
    _cursor = stop;
    return Delivered(isKey ? _visitor->Key(_scratch) : _visitor->String(_scratch), start);
}

// depth is the number of containers already open around this one.
bool Reader::ParseArray(int depth) {
    const char* start = _cursor;
    if (depth >= kMaxDepth) {
        return Fail(ErrorCode::DepthExceeded, start);
    }
    ++_cursor;  // '['
    if (!Delivered(_visitor->EnterArray(), start)) {
        return false;
    }

    SkipWhitespace();
    if (_cursor != _end && *_cursor == ']') {
        const char* close = _cursor++;
        return Delivered(_visitor->ExitArray(), close);
    }

    for (;;) {
        if (!ParseValue(depth + 1)) {
            return false;
        }
        SkipWhitespace();
        if (_cursor == _end) {
            return Fail(ErrorCode::UnexpectedEnd, _cursor);
        }
        if (*_cursor == ']') {
            const char* close = _cursor++;
            return Delivered(_visitor->ExitArray(), close);
        }
        if (*_cursor != ',') {
            return Fail(ErrorCode::UnexpectedChar, _cursor);
        }
        ++_cursor;  // ','
        SkipWhitespace();
    }
}

bool Reader::ParseObject(int depth) {
    const char* start = _cursor;
    if (depth >= kMaxDepth) {
        return Fail(ErrorCode::DepthExceeded, start);
    }
    ++_cursor;  // '{'
    if (!Delivered(_visitor->EnterObject(), start)) {
        return false;
    }

    SkipWhitespace();
    if (_cursor != _end && *_cursor == '}') {
        const char* close = _cursor++;
        return Delivered(_visitor->ExitObject(), close);
    }

    for (;;) {
        if (_cursor == _end) {
            return Fail(ErrorCode::UnexpectedEnd, _cursor);
        }
        if (*_cursor != '"') {
            return Fail(ErrorCode::UnexpectedChar, _cursor);
        }
        if (!ParseStringToken(true)) {
            return false;
        }

        SkipWhitespace();
        if (_cursor == _end) {
            return Fail(ErrorCode::UnexpectedEnd, _cursor);
        }
        if (*_cursor != ':') {
            return Fail(ErrorCode::UnexpectedChar, _cursor);
        }
        ++_cursor;  // ':'
        SkipWhitespace();

        if (!ParseValue(depth + 1)) {
            return false;
        }
        SkipWhitespace();
        if (_cursor == _end) {
            return Fail(ErrorCode::UnexpectedEnd, _cursor);
        }
        if (*_cursor == '}') {
            const char* close = _cursor++;
            return Delivered(_visitor->ExitObject(), close);
        }
        if (*_cursor != ',') {
            return Fail(ErrorCode::UnexpectedChar, _cursor);
        }
        ++_cursor;  // ','
        SkipWhitespace();
    }
}

}  // namespace json
}  // namespace tinycodec
