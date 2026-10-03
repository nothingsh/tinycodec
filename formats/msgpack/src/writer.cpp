#include "tinycodec/msgpack/writer.h"

#include <cstring>
#include <limits>

namespace tinycodec {
namespace msgpack {

static_assert(std::numeric_limits<double>::is_iec559, "Double must be IEEE 754 binary64");

namespace {

// The largest length or count that MessagePack can encode.
constexpr uint64_t kMaxLength = 0xFFFFFFFF;

// Appends the lowest `size` bytes of value, most significant first.
void AppendBigEndian(std::string* out, uint64_t value, int size) {
    for (int shift = (size - 1) * 8; shift >= 0; shift -= 8) {
        out->push_back(static_cast<char>((value >> shift) & 0xFF));
    }
}

void AppendTyped(std::string* out, unsigned char type, uint64_t value, int size) {
    out->push_back(static_cast<char>(type));
    AppendBigEndian(out, value, size);
}

void AppendUnsigned(std::string* out, uint64_t value) {
    if (value <= 0x7F) {
        out->push_back(static_cast<char>(value));   // positive fixint
    } else if (value <= 0xFF) {
        AppendTyped(out, 0xCC, value, 1);
    } else if (value <= 0xFFFF) {
        AppendTyped(out, 0xCD, value, 2);
    } else if (value <= 0xFFFFFFFF) {
        AppendTyped(out, 0xCE, value, 4);
    } else {
        AppendTyped(out, 0xCF, value, 8);
    }
}

// value must be negative.
void AppendNegative(std::string* out, int64_t value) {
    // Converting to uint64_t keeps the two's complement bit pattern, and
    // AppendBigEndian keeps only as many low bytes as the encoding needs.
    uint64_t bits = static_cast<uint64_t>(value);
    if (value >= -32) {
        out->push_back(static_cast<char>(bits & 0xFF));   // negative fixint
    } else if (value >= INT8_MIN) {
        AppendTyped(out, 0xD0, bits, 1);
    } else if (value >= INT16_MIN) {
        AppendTyped(out, 0xD1, bits, 2);
    } else if (value >= INT32_MIN) {
        AppendTyped(out, 0xD2, bits, 4);
    } else {
        AppendTyped(out, 0xD3, bits, 8);
    }
}

// The headers below hold a length or count, which the caller has already
// checked against kMaxLength.

void AppendStrHeader(std::string* out, uint64_t length) {
    if (length <= 31) {
        out->push_back(static_cast<char>(0xA0 | length));   // fixstr
    } else if (length <= 0xFF) {
        AppendTyped(out, 0xD9, length, 1);
    } else if (length <= 0xFFFF) {
        AppendTyped(out, 0xDA, length, 2);
    } else {
        AppendTyped(out, 0xDB, length, 4);
    }
}

void AppendBinHeader(std::string* out, uint64_t length) {
    if (length <= 0xFF) {
        AppendTyped(out, 0xC4, length, 1);
    } else if (length <= 0xFFFF) {
        AppendTyped(out, 0xC5, length, 2);
    } else {
        AppendTyped(out, 0xC6, length, 4);
    }
}

void AppendContainerHeader(std::string* out, bool isObject, uint64_t count) {
    if (count <= 15) {
        out->push_back(static_cast<char>((isObject ? 0x80 : 0x90) | count));   // fixmap, fixarray
    } else if (count <= 0xFFFF) {
        AppendTyped(out, isObject ? 0xDE : 0xDC, count, 2);
    } else {
        AppendTyped(out, isObject ? 0xDF : 0xDD, count, 4);
    }
}

}  // namespace

Writer::Writer(Sink& sink) : _sink(sink) {}

// Checks that a value may come next and counts it as an array element.
// Object members are counted by Key instead.
bool Writer::BeginValue() {
    if (_levels.empty()) {
        if (_rootStarted) {
            return false;
        }
        _rootStarted = true;
        return true;
    }

    Level& level = _levels.back();
    if (level.isObject) {
        if (!_keyPending) {
            return false;
        }
        _keyPending = false;
        return true;
    }
    ++level.count;
    return true;
}

// Called when a value is complete. If it was the root, the output is done
// and goes to the sink.
bool Writer::EndValue() {
    if (!_levels.empty()) {
        return true;
    }
    bool written = _sink.Write(_buffer);
    _buffer.clear();
    return written;
}

bool Writer::Enter(bool isObject) {
    if (!BeginValue()) {
        return false;
    }
    _levels.push_back({isObject, _buffer.size(), 0});
    return true;
}

bool Writer::Exit(bool isObject) {
    if (_levels.empty() || _levels.back().isObject != isObject || _keyPending) {
        return false;
    }
    Level level = _levels.back();
    if (level.count > kMaxLength) {
        return false;
    }
    std::string header;
    AppendContainerHeader(&header, isObject, level.count);
    _buffer.insert(level.start, header);
    _levels.pop_back();
    return EndValue();
}

void Writer::WriteString(std::string_view value) {
    AppendStrHeader(&_buffer, value.size());
    _buffer.append(value.data(), value.size());
}

bool Writer::Null() {
    if (!BeginValue()) {
        return false;
    }
    _buffer.push_back(static_cast<char>(0xC0));
    return EndValue();
}

bool Writer::Bool(bool value) {
    if (!BeginValue()) {
        return false;
    }
    _buffer.push_back(static_cast<char>(value ? 0xC3 : 0xC2));
    return EndValue();
}

bool Writer::Int(int64_t value) {
    if (!BeginValue()) {
        return false;
    }
    if (value >= 0) {
        AppendUnsigned(&_buffer, static_cast<uint64_t>(value));
    } else {
        AppendNegative(&_buffer, value);
    }
    return EndValue();
}

bool Writer::Uint(uint64_t value) {
    if (!BeginValue()) {
        return false;
    }
    AppendUnsigned(&_buffer, value);
    return EndValue();
}

bool Writer::Double(double value) {
    if (!BeginValue()) {
        return false;
    }
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    AppendTyped(&_buffer, 0xCB, bits, 8);
    return EndValue();
}

bool Writer::String(std::string_view value) {
    if (value.size() > kMaxLength || !BeginValue()) {
        return false;
    }
    WriteString(value);
    return EndValue();
}

bool Writer::Bytes(std::string_view value) {
    if (value.size() > kMaxLength || !BeginValue()) {
        return false;
    }
    AppendBinHeader(&_buffer, value.size());
    _buffer.append(value.data(), value.size());
    return EndValue();
}

bool Writer::EnterObject() {
    return Enter(true);
}

bool Writer::Key(std::string_view key) {
    if (_levels.empty() || !_levels.back().isObject || _keyPending || key.size() > kMaxLength) {
        return false;
    }
    ++_levels.back().count;
    _keyPending = true;
    WriteString(key);
    return true;
}

bool Writer::ExitObject() {
    return Exit(true);
}

bool Writer::EnterArray() {
    return Enter(false);
}

bool Writer::ExitArray() {
    return Exit(false);
}

}  // namespace msgpack
}  // namespace tinycodec
