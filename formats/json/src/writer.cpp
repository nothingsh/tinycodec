#include "tinycodec/json/writer.h"

#include <charconv>
#include <cmath>
#include <string>

namespace tinycodec {
namespace json {

Writer::Writer(Sink& sink, WriterOptions options) : _sink(sink), _options(options) {}

// Writes whatever has to come before a value: nothing for the root or
// after a key, otherwise the comma and line break between array elements.
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

    if (level.count > 0 && !_sink.Write(",")) {
        return false;
    }
    ++level.count;
    return WriteLineBreak();
}

// In indented mode, starts a new line indented for the current depth.
bool Writer::WriteLineBreak() {
    if (_options.indent <= 0) {
        return true;
    }
    std::string text = "\n";
    text.append(_levels.size() * static_cast<size_t>(_options.indent), ' ');
    return _sink.Write(text);
}

bool Writer::Enter(bool isObject) {
    if (!BeginValue() || !_sink.Write(isObject ? "{" : "[")) {
        return false;
    }
    _levels.push_back({isObject, 0});
    return true;
}

bool Writer::Exit(bool isObject) {
    if (_levels.empty() || _levels.back().isObject != isObject || _keyPending) {
        return false;
    }
    bool wasEmpty = _levels.back().count == 0;
    _levels.pop_back();
    if (!wasEmpty && !WriteLineBreak()) {
        return false;
    }
    return _sink.Write(isObject ? "}" : "]");
}

bool Writer::WriteQuoted(std::string_view text) {
    static const char HEX[] = "0123456789abcdef";

    std::string out = "\"";
    for (char character : text) {
        unsigned char byte = static_cast<unsigned char>(character);
        switch (byte) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (byte < 0x20) {
                out += "\\u00";
                out += HEX[byte >> 4];
                out += HEX[byte & 0x0F];
            } else {
                out += character;
            }
        }
    }
    out += '"';
    return _sink.Write(out);
}

bool Writer::Null() {
    return BeginValue() && _sink.Write("null");
}

bool Writer::Bool(bool value) {
    return BeginValue() && _sink.Write(value ? "true" : "false");
}

bool Writer::Int(int64_t value) {
    char buffer[24];
    std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return BeginValue() && _sink.Write(std::string_view(buffer, result.ptr - buffer));
}

bool Writer::Uint(uint64_t value) {
    char buffer[24];
    std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return BeginValue() && _sink.Write(std::string_view(buffer, result.ptr - buffer));
}

bool Writer::Double(double value) {
    if (!std::isfinite(value)) {
        return false;
    }
    // The shortest text that reads back as exactly the same double.
    char buffer[32];
    std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    std::string text(buffer, result.ptr - buffer);
    // Keep it recognisable as a floating point number: "1" becomes "1.0".
    if (text.find_first_of(".eE") == std::string::npos) {
        text += ".0";
    }
    return BeginValue() && _sink.Write(text);
}

bool Writer::String(std::string_view value) {
    return BeginValue() && WriteQuoted(value);
}

bool Writer::EnterObject() {
    return Enter(true);
}

bool Writer::Key(std::string_view key) {
    if (_levels.empty() || !_levels.back().isObject || _keyPending) {
        return false;
    }
    Level& level = _levels.back();
    if (level.count > 0 && !_sink.Write(",")) {
        return false;
    }
    ++level.count;
    if (!WriteLineBreak() || !WriteQuoted(key)) {
        return false;
    }
    _keyPending = true;
    return _sink.Write(_options.indent > 0 ? ": " : ":");
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

}  // namespace json
}  // namespace tinycodec
