#include "tinycodec/msgpack/reader.h"

#include <cstring>
#include <limits>

#include "tinycodec/utf8.h"

namespace tinycodec {
namespace msgpack {

static_assert(std::numeric_limits<float>::is_iec559, "float must be IEEE 754 binary32");
static_assert(std::numeric_limits<double>::is_iec559, "double must be IEEE 754 binary64");

namespace {

// Interprets the low `size` bytes of raw as a two's complement number.
int64_t ToSigned(uint64_t raw, int size) {
    uint64_t signBit = uint64_t{1} << (size * 8 - 1);
    if ((raw & signBit) == 0) {
        return static_cast<int64_t>(raw);
    }
    uint64_t mask = signBit | (signBit - 1);
    return -static_cast<int64_t>(~raw & mask) - 1;
}

}  // namespace

Error Reader::Parse(std::string_view data, Visitor& visitor) {
    _begin = data.data();
    _cursor = _begin;
    _end = _begin + data.size();
    _visitor = &visitor;
    _error = Error();

    if (ParseValue(0) && _cursor != _end) {
        Fail(ErrorCode::UnexpectedChar, _cursor);
    }
    return _error;
}

// Records an error at position and returns false, so that callers can
// write `return Fail(...)`. Line and column stay 0: binary input has none.
bool Reader::Fail(ErrorCode code, const char* position) {
    _error.code = code;
    _error.offset = static_cast<size_t>(position - _begin);
    return false;
}

// Turns the result of a visitor call into this parser's result: a rejected
// event becomes an Aborted error at position.
bool Reader::Delivered(bool accepted, const char* position) {
    return accepted || Fail(ErrorCode::Aborted, position);
}

// Reads a big-endian unsigned number of `size` bytes and moves past it.
bool Reader::ReadNumber(int size, uint64_t* out) {
    if (_end - _cursor < size) {
        return Fail(ErrorCode::UnexpectedEnd, _end);
    }
    uint64_t value = 0;
    for (int i = 0; i < size; ++i) {
        value = (value << 8) | static_cast<unsigned char>(*_cursor++);
    }
    *out = value;
    return true;
}

// Takes the next `length` bytes as the content of a str or bin.
bool Reader::ReadContent(uint64_t length, std::string_view* out) {
    if (static_cast<uint64_t>(_end - _cursor) < length) {
        return Fail(ErrorCode::UnexpectedEnd, _end);
    }
    *out = std::string_view(_cursor, static_cast<size_t>(length));
    _cursor += length;
    return true;
}

// depth is the number of containers already open around this value.
bool Reader::ParseValue(int depth) {
    if (_cursor == _end) {
        return Fail(ErrorCode::UnexpectedEnd, _cursor);
    }

    const char* start = _cursor++;
    unsigned char type = static_cast<unsigned char>(*start);
    if (type <= 0x7F) {
        return Delivered(_visitor->Int(type), start);                   // positive fixint
    }
    if (type >= 0xE0) {
        return Delivered(_visitor->Int(ToSigned(type, 1)), start);      // negative fixint
    }
    if (type <= 0x8F) {
        return ParseContainer(start, true, 0, depth);                    // fixmap
    }
    if (type <= 0x9F) {
        return ParseContainer(start, false, 0, depth);                   // fixarray
    }
    if (type <= 0xBF) {
        return ParseString(start, 0, false);                             // fixstr
    }

    switch (type) {
    case 0xC0:
        return Delivered(_visitor->Null(), start);
    case 0xC1:
        return Fail(ErrorCode::UnexpectedChar, start);   // Never used by the format.
    case 0xC2:
    case 0xC3:
        return Delivered(_visitor->Bool(type == 0xC3), start);
    case 0xC4:
    case 0xC5:
    case 0xC6:
        return ParseBytes(start, 1 << (type - 0xC4));
    case 0xCA:
        return ParseFloat(start, 4);
    case 0xCB:
        return ParseFloat(start, 8);
    case 0xCC:
    case 0xCD:
    case 0xCE:
    case 0xCF:
        return ParseInteger(start, 1 << (type - 0xCC), false);
    case 0xD0:
    case 0xD1:
    case 0xD2:
    case 0xD3:
        return ParseInteger(start, 1 << (type - 0xD0), true);
    case 0xD9:
    case 0xDA:
    case 0xDB:
        return ParseString(start, 1 << (type - 0xD9), false);
    case 0xDC:
        return ParseContainer(start, false, 2, depth);
    case 0xDD:
        return ParseContainer(start, false, 4, depth);
    case 0xDE:
        return ParseContainer(start, true, 2, depth);
    case 0xDF:
        return ParseContainer(start, true, 4, depth);
    default:
        // ext 8/16/32 (c7-c9) and fixext 1-16 (d4-d8), including timestamps.
        return Fail(ErrorCode::Unsupported, start);
    }
}

// Parses a map key, which must be a str.
bool Reader::ParseKey() {
    if (_cursor == _end) {
        return Fail(ErrorCode::UnexpectedEnd, _cursor);
    }

    const char* start = _cursor++;
    unsigned char type = static_cast<unsigned char>(*start);
    if (type >= 0xA0 && type <= 0xBF) {
        return ParseString(start, 0, true);
    }
    if (type >= 0xD9 && type <= 0xDB) {
        return ParseString(start, 1 << (type - 0xD9), true);
    }
    if (type == 0xC1) {
        return Fail(ErrorCode::UnexpectedChar, start);
    }
    return Fail(ErrorCode::Unsupported, start);
}

bool Reader::ParseInteger(const char* start, int size, bool isSigned) {
    uint64_t raw = 0;
    if (!ReadNumber(size, &raw)) {
        return false;
    }
    if (isSigned) {
        return Delivered(_visitor->Int(ToSigned(raw, size)), start);
    }
    if (raw > static_cast<uint64_t>(INT64_MAX)) {
        return Delivered(_visitor->Uint(raw), start);
    }
    return Delivered(_visitor->Int(static_cast<int64_t>(raw)), start);
}

bool Reader::ParseFloat(const char* start, int size) {
    uint64_t raw = 0;
    if (!ReadNumber(size, &raw)) {
        return false;
    }
    double value = 0.0;
    if (size == 4) {
        uint32_t bits = static_cast<uint32_t>(raw);
        float narrow = 0.0f;
        std::memcpy(&narrow, &bits, sizeof(narrow));
        value = narrow;   // Every float is exactly representable as a double.
    } else {
        std::memcpy(&value, &raw, sizeof(value));
    }
    return Delivered(_visitor->Double(value), start);
}

// A str whose type byte is at start. Its length is in the type byte itself
// when lengthSize is 0, otherwise in the lengthSize bytes after it.
bool Reader::ParseString(const char* start, int lengthSize, bool isKey) {
    uint64_t length = static_cast<unsigned char>(*start) & 0x1F;
    if (lengthSize != 0 && !ReadNumber(lengthSize, &length)) {
        return false;
    }
    std::string_view text;
    if (!ReadContent(length, &text)) {
        return false;
    }
    size_t valid = ValidUtf8Prefix(text);
    if (valid != text.size()) {
        return Fail(ErrorCode::InvalidUtf8, text.data() + valid);
    }
    return Delivered(isKey ? _visitor->Key(text) : _visitor->String(text), start);
}

bool Reader::ParseBytes(const char* start, int lengthSize) {
    uint64_t length = 0;
    std::string_view bytes;
    if (!ReadNumber(lengthSize, &length) || !ReadContent(length, &bytes)) {
        return false;
    }
    return Delivered(_visitor->Bytes(bytes), start);
}

// An array or map whose type byte is at start. Its count is in the type
// byte itself when countSize is 0, otherwise in the countSize bytes after
// it. Nothing is allocated for the count, so a count far larger than the
// input simply runs into the end of the input.
bool Reader::ParseContainer(const char* start, bool isObject, int countSize, int depth) {
    if (depth >= kMaxDepth) {
        return Fail(ErrorCode::DepthExceeded, start);
    }
    uint64_t count = static_cast<unsigned char>(*start) & 0x0F;
    if (countSize != 0 && !ReadNumber(countSize, &count)) {
        return false;
    }

    if (!Delivered(isObject ? _visitor->EnterObject() : _visitor->EnterArray(), start)) {
        return false;
    }
    for (uint64_t i = 0; i < count; ++i) {
        if (isObject && !ParseKey()) {
            return false;
        }
        if (!ParseValue(depth + 1)) {
            return false;
        }
    }
    return Delivered(isObject ? _visitor->ExitObject() : _visitor->ExitArray(), _cursor);
}

}  // namespace msgpack
}  // namespace tinycodec
