#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "number_parser.h"

using namespace tinycodec::json;

namespace {

struct Result {
    bool ok = false;
    Number number;
    size_t stop = 0;   // Offset of *stop from the start of the text.
};

Result Parse(const std::string& text) {
    Result result;
    const char* stop = nullptr;
    result.ok = ParseNumber(text.data(), text.data() + text.size(), &result.number, &stop);
    result.stop = static_cast<size_t>(stop - text.data());
    return result;
}

void CheckInt(const std::string& text, int64_t expected) {
    CAPTURE(text);
    Result result = Parse(text);
    REQUIRE(result.ok);
    CHECK(result.number.kind == NumberKind::Int);
    CHECK(result.number.intValue == expected);
    CHECK(result.stop == text.size());
}

void CheckUint(const std::string& text, uint64_t expected) {
    CAPTURE(text);
    Result result = Parse(text);
    REQUIRE(result.ok);
    CHECK(result.number.kind == NumberKind::Uint);
    CHECK(result.number.uintValue == expected);
    CHECK(result.stop == text.size());
}

void CheckDouble(const std::string& text, double expected) {
    CAPTURE(text);
    Result result = Parse(text);
    REQUIRE(result.ok);
    CHECK(result.number.kind == NumberKind::Double);
    CHECK(result.number.doubleValue == expected);
    CHECK(result.stop == text.size());
}

void CheckFails(const std::string& text, size_t expectedStop) {
    CAPTURE(text);
    Result result = Parse(text);
    CHECK_FALSE(result.ok);
    CHECK(result.stop == expectedStop);
}

}  // namespace

TEST_CASE("integers that fit int64_t are Int") {
    CheckInt("0", 0);
    CheckInt("7", 7);
    CheckInt("-7", -7);
    CheckInt("1234567890", 1234567890);
    CheckInt("9223372036854775807", INT64_MAX);
    CheckInt("-9223372036854775808", INT64_MIN);
}

TEST_CASE("integers above INT64_MAX that fit uint64_t are Uint") {
    CheckUint("9223372036854775808", static_cast<uint64_t>(INT64_MAX) + 1);
    CheckUint("18446744073709551615", UINT64_MAX);
}

TEST_CASE("integers beyond 64 bits are Double") {
    CheckDouble("18446744073709551616", 18446744073709551616.0);
    CheckDouble("-9223372036854775809", -9223372036854775809.0);
    CheckDouble("123123123123123123123123123123", 123123123123123123123123123123.0);
}

TEST_CASE("numbers with a fraction or an exponent are Double") {
    CheckDouble("0.5", 0.5);
    CheckDouble("-0.5", -0.5);
    CheckDouble("1.0", 1.0);
    CheckDouble("1e2", 100.0);
    CheckDouble("1E2", 100.0);
    CheckDouble("1e+2", 100.0);
    CheckDouble("1e-2", 0.01);
    CheckDouble("12.5e3", 12500.0);
    CheckDouble("0e0", 0.0);
    CheckDouble("1.7976931348623157e308", std::numeric_limits<double>::max());
    CheckDouble("5e-324", std::numeric_limits<double>::denorm_min());
}

TEST_CASE("minus zero is a Double that keeps its sign") {
    Result result = Parse("-0");
    REQUIRE(result.ok);
    CHECK(result.number.kind == NumberKind::Double);
    CHECK(result.number.doubleValue == 0.0);
    CHECK(std::signbit(result.number.doubleValue));

    Result fraction = Parse("-0.0");
    REQUIRE(fraction.ok);
    CHECK(fraction.number.kind == NumberKind::Double);
    CHECK(std::signbit(fraction.number.doubleValue));
}

TEST_CASE("parsing stops at the first character that is not part of the number") {
    Result comma = Parse("12,34");
    REQUIRE(comma.ok);
    CHECK(comma.number.intValue == 12);
    CHECK(comma.stop == 2);

    // A leading zero is a complete number; the rest is left for the caller.
    Result leadingZero = Parse("0123");
    REQUIRE(leadingZero.ok);
    CHECK(leadingZero.number.intValue == 0);
    CHECK(leadingZero.stop == 1);

    Result negativeLeadingZero = Parse("-012");
    REQUIRE(negativeLeadingZero.ok);
    CHECK(negativeLeadingZero.number.kind == NumberKind::Double);
    CHECK(negativeLeadingZero.stop == 2);

    Result bracket = Parse("1.5]");
    REQUIRE(bracket.ok);
    CHECK(bracket.number.doubleValue == 1.5);
    CHECK(bracket.stop == 3);
}

TEST_CASE("malformed numbers fail at the offending position") {
    CheckFails("", 0);
    CheckFails("-", 1);
    CheckFails("-a", 1);
    CheckFails("+1", 0);
    CheckFails(".5", 0);
    CheckFails("1.", 2);
    CheckFails("1.e5", 2);
    CheckFails("1e", 2);
    CheckFails("1e+", 3);
    CheckFails("1e-x", 3);
    CheckFails("1ex", 2);
    CheckFails("-.5", 1);
}

TEST_CASE("numbers too large for a double fail at the start of the number") {
    CheckFails("1e999", 0);
    CheckFails("-1e999", 0);
    CheckFails("1.7976931348623159e308", 0);
    CheckFails("0.4e00669999999999999999999999999999999999999999999999999999999999999999999969999999006", 0);
    CheckFails("123123e100000", 0);
    CheckFails("1" + std::string(400, '0'), 0);
    CheckFails("1" + std::string(400, '0') + ".0", 0);
}

TEST_CASE("numbers too close to zero become zero with the right sign") {
    for (const char* text : {"1e-999", "123.456e-789", "123e-10000000", "2e-324"}) {
        CAPTURE(text);
        Result result = Parse(text);
        REQUIRE(result.ok);
        CHECK(result.number.kind == NumberKind::Double);
        CHECK(result.number.doubleValue == 0.0);
        CHECK_FALSE(std::signbit(result.number.doubleValue));
    }

    Result negative = Parse("-1e-999");
    REQUIRE(negative.ok);
    CHECK(negative.number.doubleValue == 0.0);
    CHECK(std::signbit(negative.number.doubleValue));

    std::string tiny = "0." + std::string(400, '0') + "1";
    Result manyZeros = Parse(tiny);
    REQUIRE(manyZeros.ok);
    CHECK(manyZeros.number.doubleValue == 0.0);
}

TEST_CASE("zero with a huge exponent is still zero") {
    CheckDouble("0e999", 0.0);
    CheckDouble("0.0e999", 0.0);
}
