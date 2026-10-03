#ifndef TINYCODEC_JSON_WRITER_H
#define TINYCODEC_JSON_WRITER_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "tinycodec/sink.h"
#include "tinycodec/visitor.h"

namespace tinycodec {
namespace json {

struct WriterOptions {
    // Spaces per nesting level. 0 or less writes everything on one line
    // with no whitespace at all.
    int indent = 0;
};

// A Visitor that writes the events of one value as JSON text. It can be
// driven by Value::Accept, by a Reader, or by calling the events directly.
//
// An event returns false, writing nothing more, when the sink fails, when a
// Double is NaN or infinite, when the event does not fit the sequence so
// far, or when the root value is already complete. JSON has no way to
// write Bytes, so that event always returns false.
class Writer : public Visitor {
public:
    explicit Writer(Sink& sink, WriterOptions options = {});

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

private:
    struct Level {
        bool isObject;
        size_t count;   // Elements or members written so far.
    };

    bool BeginValue();
    bool Enter(bool isObject);
    bool Exit(bool isObject);
    bool WriteLineBreak();
    bool WriteQuoted(std::string_view text);

    Sink& _sink;
    WriterOptions _options;
    std::vector<Level> _levels;   // One entry per open container.
    bool _keyPending = false;     // A Key was written and its value has not been.
    bool _rootStarted = false;
};

}  // namespace json
}  // namespace tinycodec

#endif
