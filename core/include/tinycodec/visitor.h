#ifndef TINYCODEC_VISITOR_H
#define TINYCODEC_VISITOR_H

#include <cstdint>
#include <string_view>

namespace tinycodec {

// Receives a value as a sequence of events. Readers and Value::Accept
// produce events; writers and DocumentBuilder consume them.
//
// Rules every producer and consumer relies on:
//  - Returning false aborts: the producer stops at once, sends no further
//    events and reports the failure to its caller.
//  - A string_view argument is only valid during the call. It holds UTF-8,
//    is not NUL-terminated and may contain NUL bytes.
//  - Producers send well-formed sequences: Enter and Exit calls are paired,
//    and inside an object every value is preceded by exactly one Key.
//  - Producers send Int for every integer that fits in int64_t, and Uint
//    only for integers above INT64_MAX.
class Visitor {
public:
    virtual ~Visitor() = default;

    virtual bool Null() = 0;
    virtual bool Bool(bool value) = 0;
    virtual bool Int(int64_t value) = 0;
    virtual bool Uint(uint64_t value) = 0;
    virtual bool Double(double value) = 0;
    virtual bool String(std::string_view value) = 0;

    virtual bool EnterObject() = 0;
    virtual bool Key(std::string_view key) = 0;
    virtual bool ExitObject() = 0;

    virtual bool EnterArray() = 0;
    virtual bool ExitArray() = 0;
};

}  // namespace tinycodec

#endif
