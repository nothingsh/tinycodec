#ifndef TINYCODEC_TESTS_DATETIMES_H
#define TINYCODEC_TESTS_DATETIMES_H

#include <cstdint>
#include <cstdio>
#include <string>

#include "tinycodec/datetime.h"

// Test helpers for building DateTime values and showing them as text.

inline tinycodec::DateTime MakeDate(int year, int month, int day) {
    tinycodec::DateTime value;
    value.kind = tinycodec::DateTime::Kind::LocalDate;
    value.year = static_cast<int16_t>(year);
    value.month = static_cast<uint8_t>(month);
    value.day = static_cast<uint8_t>(day);
    return value;
}

inline tinycodec::DateTime MakeTime(int hour, int minute, int second, uint32_t nanosecond = 0) {
    tinycodec::DateTime value;
    value.kind = tinycodec::DateTime::Kind::LocalTime;
    value.hour = static_cast<uint8_t>(hour);
    value.minute = static_cast<uint8_t>(minute);
    value.second = static_cast<uint8_t>(second);
    value.nanosecond = nanosecond;
    return value;
}

inline tinycodec::DateTime MakeLocalDateTime(int year, int month, int day,
                                             int hour, int minute, int second, uint32_t nanosecond = 0) {
    tinycodec::DateTime value = MakeTime(hour, minute, second, nanosecond);
    value.kind = tinycodec::DateTime::Kind::LocalDateTime;
    value.year = static_cast<int16_t>(year);
    value.month = static_cast<uint8_t>(month);
    value.day = static_cast<uint8_t>(day);
    return value;
}

inline tinycodec::DateTime MakeOffsetDateTime(int year, int month, int day,
                                              int hour, int minute, int second, uint32_t nanosecond,
                                              int offsetMinutes) {
    tinycodec::DateTime value = MakeLocalDateTime(year, month, day, hour, minute, second, nanosecond);
    value.kind = tinycodec::DateTime::Kind::OffsetDateTime;
    value.offsetMinutes = static_cast<int16_t>(offsetMinutes);
    return value;
}

// Returns value as RFC 3339 text, the way toml::Writer writes it: "T"
// between date and time, the fraction without trailing zeros, "Z" for a
// zero offset. Fields are printed as they are, even when out of range.
inline std::string DateTimeText(const tinycodec::DateTime& value) {
    using Kind = tinycodec::DateTime::Kind;
    char buffer[64];
    std::string text;
    if (value.kind != Kind::LocalTime) {
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", value.year, value.month, value.day);
        text += buffer;
    }
    if (value.kind == Kind::LocalDate) {
        return text;
    }
    if (value.kind != Kind::LocalTime) {
        text += 'T';
    }
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", value.hour, value.minute, value.second);
    text += buffer;
    if (value.nanosecond != 0) {
        std::snprintf(buffer, sizeof(buffer), ".%09u", static_cast<unsigned>(value.nanosecond));
        std::string fraction = buffer;
        fraction.erase(fraction.find_last_not_of('0') + 1);
        text += fraction;
    }
    if (value.kind == Kind::OffsetDateTime) {
        if (value.offsetMinutes == 0) {
            text += 'Z';
        } else {
            int minutes = value.offsetMinutes < 0 ? -value.offsetMinutes : value.offsetMinutes;
            std::snprintf(buffer, sizeof(buffer), "%c%02d:%02d",
                          value.offsetMinutes < 0 ? '-' : '+', minutes / 60, minutes % 60);
            text += buffer;
        }
    }
    return text;
}

#endif
