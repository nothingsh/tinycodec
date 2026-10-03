#ifndef TINYCODEC_DOCUMENT_H
#define TINYCODEC_DOCUMENT_H

#include <cstdint>
#include <string_view>

#include "tinycodec/arena.h"
#include "tinycodec/value.h"

namespace tinycodec {

// Owns a tree of Values and the memory behind them. A Document cannot be
// copied or moved, because every Value points back at its Document.
class Document {
public:
    Document() = default;
    ~Document() = default;
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    Document(Document&&) = delete;
    Document& operator=(Document&&) = delete;

    const Value* Root() const { return _root; }   // Null for an empty document.
    Value* Root() { return _root; }

    // Makes value the root, replacing any previous root. Fails, changing
    // nothing, when value is null, belongs to another Document or is
    // already attached somewhere.
    bool SetRoot(Value* value);

    // Each of these returns a new, unattached Value owned by this Document.
    Value* NewNull();
    Value* NewBool(bool value);
    Value* NewInt(int64_t value);
    Value* NewUint(uint64_t value);
    Value* NewDouble(double value);
    Value* NewString(std::string_view value);     // The text is copied.
    Value* NewBytes(std::string_view value);      // The bytes are copied.
    Value* NewDateTime(const DateTime& value);    // Stored as given; IsValid() is not checked.
    Value* NewArray();
    Value* NewObject();

    // Empties the document. Every Value pointer obtained so far becomes invalid.
    void Clear();

private:
    friend class Value;

    Value* NewValue(Type type);
    // Copies text into the arena. The result is not NUL-terminated.
    const char* CopyString(std::string_view text);

    Arena _arena;
    Value* _root = nullptr;
};

}  // namespace tinycodec

#endif
