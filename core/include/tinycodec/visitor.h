#ifndef TINYCODEC_VISITOR_H
#define TINYCODEC_VISITOR_H

#include <cstdint>
#include <string_view>

#include "tinycodec/datetime.h"

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
//  - Producers send only DateTime values whose IsValid() is true.
//
// Events that are not pure virtual are extension events. Their default
// implementation returns false, so a consumer that does not know one aborts
// instead of silently dropping data. Producers send an extension event only
// for what the basic events cannot express, and every rule above applies
// to extension events too.
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

    // Extension event: a value made of arbitrary bytes, such as a
    // MessagePack bin. It may appear wherever String may. Text that String
    // can carry is never sent as Bytes.
    virtual bool Bytes(std::string_view /*value*/) { return false; }

    // Extension event: a date, a time, or both, such as a TOML datetime.
    // It may appear wherever String may. Inside Visitor and the classes
    // derived from it, the name DateTime means this function, so the type
    // has to be written as tinycodec::DateTime.
    virtual bool DateTime(const tinycodec::DateTime& /*value*/) { return false; }
};

}  // namespace tinycodec

#endif
