#include "tinycodec/datetime.h"

namespace tinycodec {

namespace {

bool IsLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int DaysInMonth(int year, int month) {
    static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && IsLeapYear(year) ? 29 : DAYS[month - 1];
}

bool IsValidDate(const DateTime& value) {
    return value.year >= 0 && value.year <= 9999
        && value.month >= 1 && value.month <= 12
        && value.day >= 1 && value.day <= DaysInMonth(value.year, value.month);
}

bool IsValidTime(const DateTime& value) {
    return value.hour <= 23 && value.minute <= 59 && value.second <= 60
        && value.nanosecond <= 999999999;
}

bool HasNoDate(const DateTime& value) {
    return value.year == 0 && value.month == 0 && value.day == 0;
}

bool HasNoTime(const DateTime& value) {
    return value.hour == 0 && value.minute == 0 && value.second == 0 && value.nanosecond == 0;
}

}  // namespace

bool DateTime::IsValid() const {
    bool hasDate = false;
    bool hasTime = false;
    bool hasOffset = false;
    switch (kind) {
    case Kind::OffsetDateTime: hasDate = true; hasTime = true; hasOffset = true; break;
    case Kind::LocalDateTime:  hasDate = true; hasTime = true; break;
    case Kind::LocalDate:      hasDate = true; break;
    case Kind::LocalTime:      hasTime = true; break;
    default:                   return false;
    }

    if (hasDate ? !IsValidDate(*this) : !HasNoDate(*this)) {
        return false;
    }
    if (hasTime ? !IsValidTime(*this) : !HasNoTime(*this)) {
        return false;
    }
    if (hasOffset) {
        return offsetMinutes >= -1439 && offsetMinutes <= 1439;
    }
    return offsetMinutes == 0;
}

bool DateTime::operator==(const DateTime& other) const {
    return kind == other.kind
        && year == other.year && month == other.month && day == other.day
        && hour == other.hour && minute == other.minute && second == other.second
        && nanosecond == other.nanosecond && offsetMinutes == other.offsetMinutes;
}

}  // namespace tinycodec
