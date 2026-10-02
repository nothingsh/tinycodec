#ifndef TINYCODEC_BUILDER_H
#define TINYCODEC_BUILDER_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "tinycodec/visitor.h"

namespace tinycodec {

class Document;
class Value;

// A Visitor that turns the events of one value into a tree inside a
// Document. When that value is complete it becomes the document's root;
// until then the document keeps whatever root it had, so a build that is
// abandoned half way leaves the root alone. Once the value is complete,
// every further event returns false. Repeated object keys are all kept.
//
// The builder does not clear the document first; that is up to the caller.
class DocumentBuilder : public Visitor {
public:
    explicit DocumentBuilder(Document& document);

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
    bool Add(Value* value);
    bool Finish();
    bool Enter(Value* container);
    bool Exit(bool isObject);

    Document& _document;
    std::vector<Value*> _open;   // Containers entered but not yet exited.
    std::string _key;            // Key for the next member of the innermost object.
    bool _hasKey = false;
    Value* _root = nullptr;      // The outermost value, once its first event has arrived.
};

}  // namespace tinycodec

#endif
