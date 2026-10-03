#ifndef TINYCODEC_MSGPACK_READER_H
#define TINYCODEC_MSGPACK_READER_H

#include <cstdint>
#include <string_view>

#include "tinycodec/error.h"
#include "tinycodec/visitor.h"

namespace tinycodec {
namespace msgpack {

// Arrays and maps may be nested at most this deep.
constexpr int kMaxDepth = 500;

// Parses MessagePack and reports it to a Visitor as events. A bin becomes
// a Bytes event; everything else maps onto the basic events.
//
// Some valid input cannot be represented and fails with Unsupported: ext
// values (including timestamps) and map keys that are not strings.
//
// Errors carry only an offset; line and column are always 0.
//
// A Reader can be used for any number of Parse calls, one at a time; it must
// not be shared between threads.
class Reader {
public:
    // Parses data, which must hold exactly one MessagePack value and nothing
    // after it. Returns an Error whose code is Ok on success. When the
    // visitor returns false, parsing stops and the code is Aborted,
    // positioned at the type byte of the value or key whose event was
    // rejected, or, for ExitArray and ExitObject, just past the container.
    Error Parse(std::string_view data, Visitor& visitor);

private:
    bool ParseValue(int depth);
    bool ParseKey();
    bool ParseInteger(const char* start, int size, bool isSigned);
    bool ParseFloat(const char* start, int size);
    bool ParseString(const char* start, int lengthSize, bool isKey);
    bool ParseBytes(const char* start, int lengthSize);
    bool ParseContainer(const char* start, bool isObject, int countSize, int depth);
    bool ReadNumber(int size, uint64_t* out);
    bool ReadContent(uint64_t length, std::string_view* out);
    bool Fail(ErrorCode code, const char* position);
    bool Delivered(bool accepted, const char* position);

    const char* _begin = nullptr;
    const char* _cursor = nullptr;
    const char* _end = nullptr;
    Visitor* _visitor = nullptr;
    Error _error;
};

}  // namespace msgpack
}  // namespace tinycodec

#endif
