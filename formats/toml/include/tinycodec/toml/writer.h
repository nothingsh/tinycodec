#ifndef TINYCODEC_TOML_WRITER_H
#define TINYCODEC_TOML_WRITER_H

#include <cstdint>
#include <string_view>

#include "tinycodec/builder.h"
#include "tinycodec/document.h"
#include "tinycodec/sink.h"
#include "tinycodec/toml/limits.h"
#include "tinycodec/visitor.h"

namespace tinycodec {
namespace toml {

// A Visitor that writes the events of one table as TOML text. It can be
// driven by Value::Accept, by a Reader, or by calling the events directly.
//
// Within a table, TOML puts plain key/value lines before subtables, which
// the events do not; so the events are first built into a Document, and
// the text is made from it and handed to the sink in a single Write when
// the root table is complete. The sink therefore never sees part of a
// document.
//
// Layout: plain members first, one per line; then each object member as a
// [table] section, and each array whose elements are all objects as
// [[array of tables]] sections. A table with no plain members and at least
// one subtable gets no header of its own. Other arrays, and the objects in
// them, are written on one line. Strings are basic strings, keys are bare
// where possible, datetimes are RFC 3339 with "T" and "Z".
//
// An event returns false, changing nothing, when:
//  - the root would not be an object;
//  - it is Null or Bytes, which TOML cannot express, or a Uint, which is
//    beyond TOML's int64 integers;
//  - it is a DateTime whose IsValid() is false;
//  - it would nest tables and arrays deeper than kMaxDepth;
//  - it does not fit the sequence so far, or the root is already complete.
// The event that completes the root returns false when an object has the
// same key twice, or when the sink fails.
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
    bool DateTime(const tinycodec::DateTime& value) override;

private:
    bool Enter(bool isObject);
    bool Exit(bool isObject);

    Sink& _sink;
    Document _document;
    DocumentBuilder _builder{_document};
    int _depth = 0;   // Containers entered and not yet exited.
};

}  // namespace toml
}  // namespace tinycodec

#endif
