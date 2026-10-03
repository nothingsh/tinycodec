#include "tinycodec/toml/writer.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <string>

#include "tinycodec/value.h"

namespace tinycodec {
namespace toml {

namespace {

bool IsBareKeyCharacter(char character) {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')
        || (character >= '0' && character <= '9') || character == '_' || character == '-';
}

void AppendQuoted(std::string& out, std::string_view text) {
    static const char HEX[] = "0123456789ABCDEF";

    out += '"';
    for (char character : text) {
        unsigned char byte = static_cast<unsigned char>(character);
        switch (byte) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\f': out += "\\f"; break;
        case '\r': out += "\\r"; break;
        default:
            if (byte < 0x20 || byte == 0x7F) {
                out += "\\u00";
                out += HEX[byte >> 4];
                out += HEX[byte & 0x0F];
            } else {
                out += character;
            }
        }
    }
    out += '"';
}

void AppendKey(std::string& out, std::string_view key) {
    bool bare = !key.empty();
    for (char character : key) {
        bare = bare && IsBareKeyCharacter(character);
    }
    if (bare) {
        out.append(key.data(), key.size());
    } else {
        AppendQuoted(out, key);
    }
}

void AppendDouble(std::string& out, double value) {
    if (std::isnan(value)) {
        out += "nan";
        return;
    }
    if (std::isinf(value)) {
        out += value < 0 ? "-inf" : "inf";
        return;
    }
    // The shortest text that reads back as exactly the same double.
    char buffer[32];
    std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    std::string text(buffer, result.ptr - buffer);
    // TOML reads "1" as an integer: write "1.0".
    if (text.find_first_of(".e") == std::string::npos) {
        text += ".0";
    }
    out += text;
}

void AppendDateTime(std::string& out, const DateTime& value) {
    char buffer[16];
    if (value.kind != DateTime::Kind::LocalTime) {
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", value.year, value.month, value.day);
        out += buffer;
        if (value.kind == DateTime::Kind::LocalDate) {
            return;
        }
        out += 'T';
    }
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", value.hour, value.minute, value.second);
    out += buffer;
    if (value.nanosecond != 0) {
        std::snprintf(buffer, sizeof(buffer), ".%09u", static_cast<unsigned>(value.nanosecond));
        std::string fraction = buffer;
        fraction.erase(fraction.find_last_not_of('0') + 1);
        out += fraction;
    }
    if (value.kind == DateTime::Kind::OffsetDateTime) {
        if (value.offsetMinutes == 0) {
            out += 'Z';
        } else {
            int minutes = value.offsetMinutes < 0 ? -value.offsetMinutes : value.offsetMinutes;
            std::snprintf(buffer, sizeof(buffer), "%c%02d:%02d",
                          value.offsetMinutes < 0 ? '-' : '+', minutes / 60, minutes % 60);
            out += buffer;
        }
    }
}

bool HasRepeatedKey(const Value& object) {
    for (const Value* member = object.FirstChild(); member != nullptr; member = member->Next()) {
        for (const Value* later = member->Next(); later != nullptr; later = later->Next()) {
            if (member->Key() == later->Key()) {
                return true;
            }
        }
    }
    return false;
}

// A non-empty array whose elements are all objects: an array of tables.
bool IsTableArray(const Value& value) {
    if (value.GetType() != Type::Array || value.Size() == 0) {
        return false;
    }
    for (const Value* element = value.FirstChild(); element != nullptr; element = element->Next()) {
        if (element->GetType() != Type::Object) {
            return false;
        }
    }
    return true;
}

// A member written as a section of its own rather than as a key/value line.
bool IsSubtable(const Value& member) {
    return member.GetType() == Type::Object || IsTableArray(member);
}

// Appends a value as it appears after "key = ". Returns false when an
// inline table has a repeated key.
bool AppendInline(std::string& out, const Value& value) {
    switch (value.GetType()) {
    case Type::Bool: {
        bool flag = false;
        value.QueryBool(&flag);
        out += flag ? "true" : "false";
        return true;
    }
    case Type::Int: {
        int64_t number = 0;
        value.QueryInt(&number);
        char buffer[24];
        std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), number);
        out.append(buffer, result.ptr - buffer);
        return true;
    }
    case Type::Double: {
        double number = 0.0;
        value.QueryDouble(&number);
        AppendDouble(out, number);
        return true;
    }
    case Type::String: {
        std::string_view text;
        value.QueryString(&text);
        AppendQuoted(out, text);
        return true;
    }
    case Type::DateTime: {
        DateTime dateTime;
        value.QueryDateTime(&dateTime);
        AppendDateTime(out, dateTime);
        return true;
    }
    case Type::Array: {
        out += '[';
        for (const Value* element = value.FirstChild(); element != nullptr; element = element->Next()) {
            if (element != value.FirstChild()) {
                out += ", ";
            }
            if (!AppendInline(out, *element)) {
                return false;
            }
        }
        out += ']';
        return true;
    }
    case Type::Object: {
        if (HasRepeatedKey(value)) {
            return false;
        }
        out += '{';
        for (const Value* member = value.FirstChild(); member != nullptr; member = member->Next()) {
            if (member != value.FirstChild()) {
                out += ", ";
            }
            AppendKey(out, member->Key());
            out += " = ";
            if (!AppendInline(out, *member)) {
                return false;
            }
        }
        out += '}';
        return true;
    }
    default:
        // The Writer never lets Null, Uint or Bytes into its document.
        return false;
    }
}

void AppendHeader(std::string& out, const std::string& path, bool isTableArray) {
    if (!out.empty()) {
        out += '\n';   // A blank line between sections.
    }
    out += isTableArray ? "[[" : "[";
    out += path;
    out += isTableArray ? "]]\n" : "]\n";
}

// Appends the content of a table whose header, if any, is already written:
// its plain members, then its subtables. path is the table's dotted path,
// with each key already quoted where needed; empty for the root.
bool AppendTable(std::string& out, const Value& table, const std::string& path) {
    if (HasRepeatedKey(table)) {
        return false;
    }

    for (const Value* member = table.FirstChild(); member != nullptr; member = member->Next()) {
        if (IsSubtable(*member)) {
            continue;
        }
        AppendKey(out, member->Key());
        out += " = ";
        if (!AppendInline(out, *member)) {
            return false;
        }
        out += '\n';
    }

    for (const Value* member = table.FirstChild(); member != nullptr; member = member->Next()) {
        if (!IsSubtable(*member)) {
            continue;
        }
        std::string childPath = path;
        if (!childPath.empty()) {
            childPath += '.';
        }
        AppendKey(childPath, member->Key());

        if (member->GetType() == Type::Object) {
            bool hasPlainMember = false;
            for (const Value* child = member->FirstChild(); child != nullptr; child = child->Next()) {
                hasPlainMember = hasPlainMember || !IsSubtable(*child);
            }
            // A table holding only subtables needs no header: their headers
            // create it.
            if (hasPlainMember || member->Size() == 0) {
                AppendHeader(out, childPath, false);
            }
            if (!AppendTable(out, *member, childPath)) {
                return false;
            }
        } else {
            for (const Value* element = member->FirstChild(); element != nullptr; element = element->Next()) {
                AppendHeader(out, childPath, true);
                if (!AppendTable(out, *element, childPath)) {
                    return false;
                }
            }
        }
    }
    return true;
}

}  // namespace

Writer::Writer(Sink& sink) : _sink(sink) {}

bool Writer::Enter(bool isObject) {
    if (_depth == 0 && !isObject) {
        return false;   // The root must be a table.
    }
    if (_depth >= kMaxDepth) {
        return false;
    }
    if (!(isObject ? _builder.EnterObject() : _builder.EnterArray())) {
        return false;
    }
    ++_depth;
    return true;
}

bool Writer::Exit(bool isObject) {
    if (!(isObject ? _builder.ExitObject() : _builder.ExitArray())) {
        return false;
    }
    if (--_depth > 0) {
        return true;
    }

    // The root table is complete.
    std::string text;
    return AppendTable(text, *_document.Root(), std::string()) && _sink.Write(text);
}

bool Writer::Null() {
    return false;
}

bool Writer::Bool(bool value) {
    return _depth > 0 && _builder.Bool(value);
}

bool Writer::Int(int64_t value) {
    return _depth > 0 && _builder.Int(value);
}

bool Writer::Uint(uint64_t /*value*/) {
    // Producers send Uint only above INT64_MAX, beyond TOML's integers.
    return false;
}

bool Writer::Double(double value) {
    return _depth > 0 && _builder.Double(value);
}

bool Writer::String(std::string_view value) {
    return _depth > 0 && _builder.String(value);
}

bool Writer::EnterObject() {
    return Enter(true);
}

bool Writer::Key(std::string_view key) {
    return _builder.Key(key);
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

bool Writer::Bytes(std::string_view /*value*/) {
    return false;
}

bool Writer::DateTime(const tinycodec::DateTime& value) {
    return _depth > 0 && value.IsValid() && _builder.DateTime(value);
}

}  // namespace toml
}  // namespace tinycodec
