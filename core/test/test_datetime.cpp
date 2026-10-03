#include "doctest.h"

#include "datetimes.h"
#include "tinycodec/datetime.h"

using namespace tinycodec;

TEST_CASE("DateTime fits in 16 bytes") {
    CHECK(sizeof(DateTime) <= 16);
}

TEST_CASE("a valid value of each kind is valid") {
    CHECK(MakeOffsetDateTime(1979, 5, 27, 7, 32, 0, 999000000, -420).IsValid());
    CHECK(MakeLocalDateTime(1979, 5, 27, 7, 32, 0).IsValid());
    CHECK(MakeDate(1979, 5, 27).IsValid());
    CHECK(MakeTime(7, 32, 0).IsValid());
}

TEST_CASE("a default DateTime is not valid: it is a LocalDate with no date") {
    CHECK_FALSE(DateTime().IsValid());
}

TEST_CASE("February has 29 days in leap years only") {
    CHECK(MakeDate(2000, 2, 29).IsValid());        // Divisible by 400.
    CHECK(MakeDate(2024, 2, 29).IsValid());        // Divisible by 4, not by 100.
    CHECK_FALSE(MakeDate(1900, 2, 29).IsValid());  // Divisible by 100, not by 400.
    CHECK_FALSE(MakeDate(2023, 2, 29).IsValid());
    CHECK(MakeDate(2023, 2, 28).IsValid());
}

TEST_CASE("every month ends on its last day") {
    const int LAST_DAY[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int month = 1; month <= 12; ++month) {
        CAPTURE(month);
        CHECK(MakeDate(2023, month, LAST_DAY[month - 1]).IsValid());
        CHECK_FALSE(MakeDate(2023, month, LAST_DAY[month - 1] + 1).IsValid());
    }
}

TEST_CASE("date fields outside their range are not valid") {
    CHECK(MakeDate(0, 1, 1).IsValid());
    CHECK(MakeDate(9999, 12, 31).IsValid());
    CHECK_FALSE(MakeDate(-1, 1, 1).IsValid());
    CHECK_FALSE(MakeDate(10000, 1, 1).IsValid());
    CHECK_FALSE(MakeDate(2023, 0, 1).IsValid());
    CHECK_FALSE(MakeDate(2023, 13, 1).IsValid());
    CHECK_FALSE(MakeDate(2023, 1, 0).IsValid());
}

TEST_CASE("time fields outside their range are not valid") {
    CHECK(MakeTime(23, 59, 59, 999999999).IsValid());
    CHECK(MakeTime(23, 59, 60).IsValid());          // A leap second.
    CHECK_FALSE(MakeTime(24, 0, 0).IsValid());
    CHECK_FALSE(MakeTime(0, 60, 0).IsValid());
    CHECK_FALSE(MakeTime(0, 0, 61).IsValid());
    CHECK_FALSE(MakeTime(0, 0, 0, 1000000000).IsValid());
}

TEST_CASE("the offset ranges over one day less a minute, either way") {
    CHECK(MakeOffsetDateTime(2023, 1, 1, 0, 0, 0, 0, 1439).IsValid());
    CHECK(MakeOffsetDateTime(2023, 1, 1, 0, 0, 0, 0, -1439).IsValid());
    CHECK_FALSE(MakeOffsetDateTime(2023, 1, 1, 0, 0, 0, 0, 1440).IsValid());
    CHECK_FALSE(MakeOffsetDateTime(2023, 1, 1, 0, 0, 0, 0, -1440).IsValid());
}

TEST_CASE("fields a kind does not use must be 0") {
    DateTime date = MakeDate(2023, 1, 1);
    date.hour = 1;
    CHECK_FALSE(date.IsValid());
    date = MakeDate(2023, 1, 1);
    date.nanosecond = 1;
    CHECK_FALSE(date.IsValid());
    date = MakeDate(2023, 1, 1);
    date.offsetMinutes = 60;
    CHECK_FALSE(date.IsValid());

    DateTime time = MakeTime(1, 2, 3);
    time.year = 2023;
    CHECK_FALSE(time.IsValid());
    time = MakeTime(1, 2, 3);
    time.month = 1;
    CHECK_FALSE(time.IsValid());
    time = MakeTime(1, 2, 3);
    time.offsetMinutes = 60;
    CHECK_FALSE(time.IsValid());

    DateTime local = MakeLocalDateTime(2023, 1, 1, 1, 2, 3);
    local.offsetMinutes = 60;
    CHECK_FALSE(local.IsValid());
}

TEST_CASE("a kind outside the enumeration is not valid") {
    DateTime value = MakeDate(2023, 1, 1);
    value.kind = static_cast<DateTime::Kind>(4);
    CHECK_FALSE(value.IsValid());
}

TEST_CASE("DateTimes compare field by field") {
    CHECK(MakeDate(2023, 1, 1) == MakeDate(2023, 1, 1));
    CHECK(MakeDate(2023, 1, 1) != MakeDate(2023, 1, 2));
    CHECK(MakeTime(1, 2, 3, 4) != MakeTime(1, 2, 3, 5));
    // The kind counts: midnight on a date is not the date.
    CHECK(MakeLocalDateTime(2023, 1, 1, 0, 0, 0) != MakeDate(2023, 1, 1));
    // The same instant with different offsets is a different value.
    CHECK(MakeOffsetDateTime(2023, 1, 1, 12, 0, 0, 0, 0)
          != MakeOffsetDateTime(2023, 1, 1, 13, 0, 0, 0, 60));
}
