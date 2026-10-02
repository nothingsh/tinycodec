#ifndef TINYCODEC_ERROR_H
#define TINYCODEC_ERROR_H

#include <cstddef>

namespace tinycodec {

enum class ErrorCode {
    Ok,
    UnexpectedEnd,   // Input ended before the value was complete, including empty input.
    UnexpectedChar,  // A character that is not allowed here, including content after the root.
    InvalidNumber,   // Malformed number, or a number that overflows to infinity.
    InvalidEscape,   // Malformed escape sequence, including unpaired surrogates.
    InvalidUtf8,     // Malformed UTF-8 byte sequence inside a string.
    DepthExceeded,   // Containers nested deeper than the limit.
    Aborted,         // A Visitor returned false.
};

struct Error {
    ErrorCode code = ErrorCode::Ok;
    size_t offset = 0;  // Byte offset, starting at 0.
    int line = 0;       // Starting at 1; 0 when code is Ok.
    int column = 0;     // Starting at 1, counted in bytes; 0 when code is Ok.

    bool Ok() const { return code == ErrorCode::Ok; }
};

// Returns the enumerator's name, e.g. "UnexpectedEnd". Never returns null.
const char* ErrorName(ErrorCode code);

}  // namespace tinycodec

#endif
