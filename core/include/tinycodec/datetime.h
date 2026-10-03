#ifndef TINYCODEC_DATETIME_H
#define TINYCODEC_DATETIME_H

#include <cstdint>

namespace tinycodec {

// A date, a time of day, or both, as in RFC 3339. Which fields are in use
// depends on the kind:
//
//   kind            year/month/day   hour/minute/second/nanosecond   offsetMinutes
//   OffsetDateTime  yes              yes                             yes
//   LocalDateTime   yes              yes                             0
//   LocalDate       yes              0                               0
//   LocalTime       0                yes                             0
//
// The fields are ordered to keep the struct small, not in reading order.
struct DateTime {
    enum class Kind : uint8_t { OffsetDateTime, LocalDateTime, LocalDate, LocalTime };

    int16_t year = 0;            // 0 to 9999.
    int16_t offsetMinutes = 0;   // -1439 to 1439, east of UTC positive; "Z" is 0.
    uint32_t nanosecond = 0;     // 0 to 999999999.
    uint8_t month = 0;           // 1 to 12.
    uint8_t day = 0;             // 1 to the length of the month, leap years included.
    uint8_t hour = 0;            // 0 to 23.
    uint8_t minute = 0;          // 0 to 59.
    uint8_t second = 0;          // 0 to 60; 60 is a leap second, which is not checked further.
    Kind kind = Kind::LocalDate;

    // True when the fields the kind uses are in range and the others are 0.
    bool IsValid() const;

    // Field by field. The same instant with different offsets is a
    // different value.
    bool operator==(const DateTime& other) const;
    bool operator!=(const DateTime& other) const { return !(*this == other); }
};

}  // namespace tinycodec

#endif
