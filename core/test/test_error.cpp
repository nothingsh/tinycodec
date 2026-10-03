#include "doctest.h"

#include <string>

#include "tinycodec/error.h"

using namespace tinycodec;

TEST_CASE("a default Error is Ok and has no position") {
    Error error;
    CHECK(error.Ok());
    CHECK(error.code == ErrorCode::Ok);
    CHECK(error.offset == 0);
    CHECK(error.line == 0);
    CHECK(error.column == 0);
}

TEST_CASE("an Error with any other code is not Ok") {
    Error error;
    error.code = ErrorCode::Aborted;
    CHECK_FALSE(error.Ok());
}

TEST_CASE("ErrorName returns the enumerator name") {
    CHECK(std::string(ErrorName(ErrorCode::Ok)) == "Ok");
    CHECK(std::string(ErrorName(ErrorCode::UnexpectedEnd)) == "UnexpectedEnd");
    CHECK(std::string(ErrorName(ErrorCode::UnexpectedChar)) == "UnexpectedChar");
    CHECK(std::string(ErrorName(ErrorCode::InvalidNumber)) == "InvalidNumber");
    CHECK(std::string(ErrorName(ErrorCode::InvalidEscape)) == "InvalidEscape");
    CHECK(std::string(ErrorName(ErrorCode::InvalidUtf8)) == "InvalidUtf8");
    CHECK(std::string(ErrorName(ErrorCode::DepthExceeded)) == "DepthExceeded");
    CHECK(std::string(ErrorName(ErrorCode::Unsupported)) == "Unsupported");
    CHECK(std::string(ErrorName(ErrorCode::Aborted)) == "Aborted");
}
