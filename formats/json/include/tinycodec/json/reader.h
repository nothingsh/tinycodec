#ifndef TINYCODEC_JSON_READER_H
#define TINYCODEC_JSON_READER_H

#include <string>
#include <string_view>

#include "tinycodec/error.h"
#include "tinycodec/visitor.h"

namespace tinycodec {
namespace json {

// Arrays and objects may be nested at most this deep.
constexpr int kMaxDepth = 500;

// Parses JSON text (RFC 8259) and reports it to a Visitor as events.
//
// A Reader can be used for any number of Parse calls, one at a time; it must
// not be shared between threads.
class Reader {
public:
    // Parses text, which must hold exactly one JSON value, optionally
    // surrounded by whitespace. Returns an Error whose code is Ok on
    // success. When the visitor returns false, parsing stops and the code is
    // Aborted, positioned at the token that produced the rejected event.
    Error Parse(std::string_view text, Visitor& visitor);

private:
    bool ParseValue(int depth);
    bool ParseArray(int depth);
    bool ParseObject(int depth);
    bool ParseLiteral(std::string_view literal);
    bool ParseNumberToken();
    bool ParseStringToken(bool isKey);
    void SkipWhitespace();
    bool Fail(ErrorCode code, const char* position);
    bool Delivered(bool accepted, const char* tokenStart);

    const char* _begin = nullptr;
    const char* _cursor = nullptr;
    const char* _end = nullptr;
    Visitor* _visitor = nullptr;
    Error _error;
    std::string _scratch;   // Holds the decoded text of the current string.
};

}  // namespace json
}  // namespace tinycodec

#endif
