#include "doctest.h"

#include <string>

#include "datetime_parser.h"
#include "datetimes.h"
#include "tinycodec/error.h"

using namespace tinycodec;
using namespace tinycodec::toml;

namespace {

void CheckParses(const std::string& word, const DateTime& expected) {
    CAPTURE(word);
    DateTime out;
    size_t errorOffset = 99;
    REQUIRE(ParseDateTime(word, &out, &errorOffset) == ErrorCode::Ok);
    CHECK(DateTimeText(out) == DateTimeText(expected));
    CHECK(out == expected);
}

void CheckFails(const std::string& word, ErrorCode expectedCode, size_t expectedOffset) {
    CAPTURE(word);
    DateTime out;
    size_t errorOffset = 99;
    CHECK(ParseDateTime(word, &out, &errorOffset) == expectedCode);
    CHECK(errorOffset == expectedOffset);
}

}  // namespace

TEST_CASE("IsDateTimeWord spots a ':' or a leading year") {
    CHECK(IsDateTimeWord("1979-05-27"));
    CHECK(IsDateTimeWord("07:32:00"));
    CHECK(IsDateTimeWord("1979-"));
    CHECK(IsDateTimeWord("1:2"));
    CHECK_FALSE(IsDateTimeWord("1979"));
    CHECK_FALSE(IsDateTimeWord("197-05-27"));
    CHECK_FALSE(IsDateTimeWord("-1979-05"));
    CHECK_FALSE(IsDateTimeWord("1e-5"));
    CHECK_FALSE(IsDateTimeWord("inf"));
}

TEST_CASE("the four kinds") {
    CheckParses("1979-05-27T07:32:00Z", MakeOffsetDateTime(1979, 5, 27, 7, 32, 0, 0, 0));
    CheckParses("1979-05-27T07:32:00", MakeLocalDateTime(1979, 5, 27, 7, 32, 0));
    CheckParses("1979-05-27", MakeDate(1979, 5, 27));
    CheckParses("07:32:00", MakeTime(7, 32, 0));
}

TEST_CASE("the separator may be T, t or a space, and Z may be lowercase") {
    CheckParses("1979-05-27t07:32:00", MakeLocalDateTime(1979, 5, 27, 7, 32, 0));
    CheckParses("1979-05-27 07:32:00", MakeLocalDateTime(1979, 5, 27, 7, 32, 0));
    CheckParses("1979-05-27 07:32:00z", MakeOffsetDateTime(1979, 5, 27, 7, 32, 0, 0, 0));
}

TEST_CASE("offsets east and west of UTC") {
    CheckParses("1979-05-27T00:32:00-07:00", MakeOffsetDateTime(1979, 5, 27, 0, 32, 0, 0, -420));
    CheckParses("1979-05-27T00:32:00+05:30", MakeOffsetDateTime(1979, 5, 27, 0, 32, 0, 0, 330));
    CheckParses("1979-05-27T00:32:00+00:00", MakeOffsetDateTime(1979, 5, 27, 0, 32, 0, 0, 0));
    CheckParses("1979-05-27T00:32:00-00:00", MakeOffsetDateTime(1979, 5, 27, 0, 32, 0, 0, 0));
    CheckParses("1979-05-27T00:32:00+23:59", MakeOffsetDateTime(1979, 5, 27, 0, 32, 0, 0, 1439));
}

TEST_CASE("fractional seconds are kept to the nanosecond and truncated beyond") {
    CheckParses("07:32:00.5", MakeTime(7, 32, 0, 500000000));
    CheckParses("07:32:00.999999", MakeTime(7, 32, 0, 999999000));
    CheckParses("07:32:00.000000001", MakeTime(7, 32, 0, 1));
    CheckParses("07:32:00.1234567899", MakeTime(7, 32, 0, 123456789));
    CheckParses("1979-05-27T00:32:00.999-07:00", MakeOffsetDateTime(1979, 5, 27, 0, 32, 0, 999000000, -420));
}

TEST_CASE("leap days and leap seconds") {
    CheckParses("2000-02-29", MakeDate(2000, 2, 29));
    CheckParses("2024-02-29T15:15:15Z", MakeOffsetDateTime(2024, 2, 29, 15, 15, 15, 0, 0));
    CheckParses("23:59:60", MakeTime(23, 59, 60));
}

TEST_CASE("a character that does not fit is UnexpectedChar at that character") {
    CheckFails("1979-5-27", ErrorCode::UnexpectedChar, 6);
    CheckFails("79-05-27", ErrorCode::UnexpectedChar, 2);
    CheckFails("1979/05/27", ErrorCode::UnexpectedChar, 4);
    CheckFails("1979-05-27X07:32:00", ErrorCode::UnexpectedChar, 10);
    CheckFails("1979-05-27T7:32:00", ErrorCode::UnexpectedChar, 12);
    CheckFails("1979-05-27T07:32:00+0700", ErrorCode::UnexpectedChar, 22);
    CheckFails("1979-05-27T07:32:00 Z", ErrorCode::UnexpectedChar, 19);
    CheckFails("1979-05-27T07:32:00ZZ", ErrorCode::UnexpectedChar, 20);
    CheckFails("07:32:00Z", ErrorCode::UnexpectedChar, 8);
    CheckFails("07:32:00+01:00", ErrorCode::UnexpectedChar, 8);
    CheckFails("1979-05-27-07:00", ErrorCode::UnexpectedChar, 10);
}

TEST_CASE("a word that ends too early is UnexpectedChar at its end") {
    CheckFails("1979-05", ErrorCode::UnexpectedChar, 7);
    CheckFails("1979-05-27T", ErrorCode::UnexpectedChar, 11);
    CheckFails("1979-05-27T07:32", ErrorCode::UnexpectedChar, 16);
    CheckFails("07:32", ErrorCode::UnexpectedChar, 5);
    CheckFails("07:32:00.", ErrorCode::UnexpectedChar, 9);
    CheckFails("1979-05-27T07:32:00+07", ErrorCode::UnexpectedChar, 22);
}

TEST_CASE("fields out of range are InvalidDateTime at the start") {
    CheckFails("2023-02-29", ErrorCode::InvalidDateTime, 0);
    CheckFails("1900-02-29", ErrorCode::InvalidDateTime, 0);
    CheckFails("2023-04-31", ErrorCode::InvalidDateTime, 0);
    CheckFails("2023-00-01", ErrorCode::InvalidDateTime, 0);
    CheckFails("2023-13-01", ErrorCode::InvalidDateTime, 0);
    CheckFails("2023-01-00", ErrorCode::InvalidDateTime, 0);
    CheckFails("24:00:00", ErrorCode::InvalidDateTime, 0);
    CheckFails("00:60:00", ErrorCode::InvalidDateTime, 0);
    CheckFails("00:00:61", ErrorCode::InvalidDateTime, 0);
    CheckFails("1979-05-27T00:00:00+24:00", ErrorCode::InvalidDateTime, 0);
    CheckFails("1979-05-27T00:00:00+00:60", ErrorCode::InvalidDateTime, 0);
    CheckFails("1979-05-27T00:00:00-23:60", ErrorCode::InvalidDateTime, 0);
}
