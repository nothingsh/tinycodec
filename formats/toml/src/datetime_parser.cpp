#include "datetime_parser.h"

namespace tinycodec {
namespace toml {

namespace {

bool IsDigit(char character) {
    return character >= '0' && character <= '9';
}

// Reads word from left to right. Every step records where it failed.
class Cursor {
public:
    explicit Cursor(std::string_view word) : _word(word) {}

    bool AtEnd() const { return _position == _word.size(); }
    char Peek() const { return _word[_position]; }
    size_t Position() const { return _position; }
    void Skip() { ++_position; }

    // Reads exactly `count` digits.
    bool Digits(int count, int* out) {
        int value = 0;
        for (int i = 0; i < count; ++i) {
            if (AtEnd() || !IsDigit(Peek())) {
                return false;
            }
            value = value * 10 + (Peek() - '0');
            ++_position;
        }
        *out = value;
        return true;
    }

    bool Expect(char character) {
        if (AtEnd() || Peek() != character) {
            return false;
        }
        ++_position;
        return true;
    }

private:
    std::string_view _word;
    size_t _position = 0;
};

bool ParseDate(Cursor& cursor, int* year, int* month, int* day) {
    return cursor.Digits(4, year) && cursor.Expect('-') && cursor.Digits(2, month)
        && cursor.Expect('-') && cursor.Digits(2, day);
}

bool ParseTime(Cursor& cursor, int* hour, int* minute, int* second, uint32_t* nanosecond) {
    if (!cursor.Digits(2, hour) || !cursor.Expect(':') || !cursor.Digits(2, minute)
            || !cursor.Expect(':') || !cursor.Digits(2, second)) {
        return false;
    }
    *nanosecond = 0;
    if (cursor.AtEnd() || cursor.Peek() != '.') {
        return true;
    }
    cursor.Skip();  // '.'
    if (cursor.AtEnd() || !IsDigit(cursor.Peek())) {
        return false;
    }
    // Keep the first nine digits, then pad to nanoseconds.
    int kept = 0;
    while (!cursor.AtEnd() && IsDigit(cursor.Peek())) {
        if (kept < 9) {
            *nanosecond = *nanosecond * 10 + static_cast<uint32_t>(cursor.Peek() - '0');
            ++kept;
        }
        cursor.Skip();
    }
    for (; kept < 9; ++kept) {
        *nanosecond *= 10;
    }
    return true;
}

}  // namespace

bool IsDateTimeWord(std::string_view word) {
    if (word.find(':') != std::string_view::npos) {
        return true;
    }
    return word.size() >= 5 && IsDigit(word[0]) && IsDigit(word[1]) && IsDigit(word[2])
        && IsDigit(word[3]) && word[4] == '-';
}

ErrorCode ParseDateTime(std::string_view word, DateTime* out, size_t* errorOffset) {
    Cursor cursor(word);
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    int offsetHour = 0, offsetMinute = 0, offsetSign = 1;
    uint32_t nanosecond = 0;
    DateTime::Kind kind = DateTime::Kind::LocalTime;
    bool ok = false;

    if (word.size() > 2 && word[2] == ':') {
        ok = ParseTime(cursor, &hour, &minute, &second, &nanosecond);
    } else if (ParseDate(cursor, &year, &month, &day)) {
        kind = DateTime::Kind::LocalDate;
        ok = true;
        if (!cursor.AtEnd()) {
            char separator = cursor.Peek();
            ok = separator == 'T' || separator == 't' || separator == ' ';
            if (ok) {
                cursor.Skip();
                kind = DateTime::Kind::LocalDateTime;
                ok = ParseTime(cursor, &hour, &minute, &second, &nanosecond);
            }
            if (ok && !cursor.AtEnd()) {
                kind = DateTime::Kind::OffsetDateTime;
                char zone = cursor.Peek();
                if (zone == 'Z' || zone == 'z') {
                    cursor.Skip();
                } else if (zone == '+' || zone == '-') {
                    offsetSign = zone == '-' ? -1 : 1;
                    cursor.Skip();
                    ok = cursor.Digits(2, &offsetHour) && cursor.Expect(':') && cursor.Digits(2, &offsetMinute);
                } else {
                    ok = false;
                }
            }
        }
    }
    if (!ok || !cursor.AtEnd()) {
        *errorOffset = cursor.Position();
        return ErrorCode::UnexpectedChar;
    }

    // The form is right; now the ranges. The digit counts keep every field
    // small enough for its type.
    DateTime value;
    value.kind = kind;
    value.year = static_cast<int16_t>(year);
    value.month = static_cast<uint8_t>(month);
    value.day = static_cast<uint8_t>(day);
    value.hour = static_cast<uint8_t>(hour);
    value.minute = static_cast<uint8_t>(minute);
    value.second = static_cast<uint8_t>(second);
    value.nanosecond = nanosecond;
    value.offsetMinutes = static_cast<int16_t>(offsetSign * (offsetHour * 60 + offsetMinute));
    if (offsetHour > 23 || offsetMinute > 59 || !value.IsValid()) {
        *errorOffset = 0;
        return ErrorCode::InvalidDateTime;
    }
    *out = value;
    return ErrorCode::Ok;
}

}  // namespace toml
}  // namespace tinycodec
