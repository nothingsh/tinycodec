#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "number_parser.h"

using namespace tinycodec::toml;

namespace {

void CheckInt(const std::string& word, int64_t expected) {
    CAPTURE(word);
    Number number;
    REQUIRE(ParseNumber(word, &number));
    CHECK(number.kind == NumberKind::Int);
    CHECK(number.intValue == expected);
}

void CheckDouble(const std::string& word, double expected) {
    CAPTURE(word);
    Number number;
    REQUIRE(ParseNumber(word, &number));
    CHECK(number.kind == NumberKind::Double);
    CHECK(number.doubleValue == expected);
    CHECK(std::signbit(number.doubleValue) == std::signbit(expected));
}

void CheckRejected(const std::string& word) {
    CAPTURE(word);
    Number number;
    CHECK_FALSE(ParseNumber(word, &number));
}

}  // namespace

TEST_CASE("decimal integers, with an optional sign") {
    CheckInt("0", 0);
    CheckInt("+0", 0);
    CheckInt("-0", 0);
    CheckInt("42", 42);
    CheckInt("+99", 99);
    CheckInt("-17", -17);
    CheckInt("9223372036854775807", INT64_MAX);
    CheckInt("-9223372036854775808", INT64_MIN);
}

TEST_CASE("underscores are allowed only between two digits") {
    CheckInt("1_000", 1000);
    CheckInt("5_349_221", 5349221);
    CheckInt("1_2_3_4_5", 12345);
    CheckRejected("1__000");
    CheckRejected("_1000");
    CheckRejected("1000_");
    CheckRejected("+_1");
}

TEST_CASE("decimal integers have no leading zeros") {
    CheckRejected("01");
    CheckRejected("+01");
    CheckRejected("-01");
    CheckRejected("00");
    CheckRejected("0_1");
}

TEST_CASE("integers outside int64 are rejected") {
    CheckRejected("9223372036854775808");
    CheckRejected("-9223372036854775809");
    CheckRejected("99999999999999999999999");
}

TEST_CASE("hexadecimal, octal and binary integers") {
    CheckInt("0xDEADBEEF", 0xDEADBEEF);
    CheckInt("0xdeadbeef", 0xDEADBEEF);
    CheckInt("0xdead_beef", 0xDEADBEEF);
    CheckInt("0x00ff", 255);
    CheckInt("0o755", 0755);
    CheckInt("0o0_1", 1);
    CheckInt("0b1101_0110", 0xD6);
    CheckInt("0x7FFFFFFFFFFFFFFF", INT64_MAX);
    CheckRejected("0x8000000000000000");
}

TEST_CASE("prefixed integers need a lowercase prefix, no sign and valid digits") {
    CheckRejected("0X1F");
    CheckRejected("0O7");
    CheckRejected("0B1");
    CheckRejected("+0x1");
    CheckRejected("-0o1");
    CheckRejected("0x");
    CheckRejected("0x_1");
    CheckRejected("0xG");
    CheckRejected("0o8");
    CheckRejected("0b2");
}

TEST_CASE("floats with a fraction, an exponent or both") {
    CheckDouble("1.0", 1.0);
    CheckDouble("+1.5", 1.5);
    CheckDouble("3.1415", 3.1415);
    CheckDouble("-0.01", -0.01);
    CheckDouble("5e+22", 5e22);
    CheckDouble("1e06", 1e6);
    CheckDouble("-2E-2", -2e-2);
    CheckDouble("6.626e-34", 6.626e-34);
    CheckDouble("224_617.445_991_228", 224617.445991228);
    CheckDouble("1e1_0", 1e10);
    CheckDouble("0e0", 0.0);
    CheckDouble("-0.0", -0.0);
    CheckDouble("+0.0", 0.0);
}

TEST_CASE("floats need digits on both sides of the point and in the exponent") {
    CheckRejected(".5");
    CheckRejected("1.");
    CheckRejected("1.e5");
    CheckRejected("+.5");
    CheckRejected("1e");
    CheckRejected("1e+");
    CheckRejected("1.5.3");
    CheckRejected("1e5.0");
    CheckRejected("1_.5");
    CheckRejected("1._5");
    CheckRejected("1e_5");
    CheckRejected("03.14");
    CheckRejected("1,5");
}

TEST_CASE("inf and nan, with an optional sign") {
    CheckDouble("inf", HUGE_VAL);
    CheckDouble("+inf", HUGE_VAL);
    CheckDouble("-inf", -HUGE_VAL);
    for (const char* word : {"nan", "+nan", "-nan"}) {
        CAPTURE(word);
        Number number;
        REQUIRE(ParseNumber(word, &number));
        CHECK(number.kind == NumberKind::Double);
        CHECK(std::isnan(number.doubleValue));
    }
    CheckRejected("Inf");
    CheckRejected("NaN");
    CheckRejected("infinity");
    CheckRejected("nan1");
}

TEST_CASE("a float that overflows is rejected, one that underflows becomes zero") {
    CheckRejected("1e309");
    CheckRejected("-1e1000");
    CheckDouble("1e-400", 0.0);
    CheckDouble("-1e-400", -0.0);
    CheckDouble("1.7976931348623157e308", 1.7976931348623157e308);
    CheckDouble("5e-324", 5e-324);
}

TEST_CASE("anything else is rejected") {
    CheckRejected("");
    CheckRejected("+");
    CheckRejected("-");
    CheckRejected("1a");
    CheckRejected("1-2");
    CheckRejected("++1");
}
