#ifndef TINYCODEC_MSGPACK_WRITER_H
#define TINYCODEC_MSGPACK_WRITER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "tinycodec/sink.h"
#include "tinycodec/visitor.h"

namespace tinycodec {
namespace msgpack {

// A Visitor that writes the events of one value as MessagePack, always in
// the shortest encoding. It can be driven by Value::Accept, by a Reader, or
// by calling the events directly.
//
// A MessagePack array or map starts with its element count, which the
// events do not give in advance, so the output is collected in memory and
// handed to the sink in a single Write when the root value is complete.
// The sink therefore never sees part of a value.
//
// An event returns false when it does not fit the sequence so far, when
// the root value is already complete, when a string, byte string or
// container is longer than the format allows (4294967295), or, for the
// event that completes the root, when the sink fails. A DateTime is not
// written as a timestamp ext; that event always returns false.
class Writer : public Visitor {
public:
    explicit Writer(Sink& sink);

    bool Null() override;
    bool Bool(bool value) override;
    bool Int(int64_t value) override;
    bool Uint(uint64_t value) override;
    bool Double(double value) override;
    bool String(std::string_view value) override;
    bool EnterObject() override;
    bool Key(std::string_view key) override;
    bool ExitObject() override;
    bool EnterArray() override;
    bool ExitArray() override;
    bool Bytes(std::string_view value) override;

private:
    struct Level {
        bool isObject;
        size_t start;   // Where the container's content begins in _buffer.
        size_t count;   // Elements, or members, so far.
    };

    bool BeginValue();
    bool EndValue();
    bool Enter(bool isObject);
    bool Exit(bool isObject);
    void WriteString(std::string_view value);

    Sink& _sink;
    std::string _buffer;          // Everything written so far, not yet given to the sink.
    std::vector<Level> _levels;   // One entry per open container.
    bool _keyPending = false;     // A Key was written and its value has not been.
    bool _rootStarted = false;
};

}  // namespace msgpack
}  // namespace tinycodec

#endif
