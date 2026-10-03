#include "tinycodec/builder.h"

#include "tinycodec/document.h"
#include "tinycodec/value.h"

namespace tinycodec {

DocumentBuilder::DocumentBuilder(Document& document) : _document(document) {}

// Attaches value to the innermost open container. The outermost value has
// no container; it is kept aside until Finish makes it the root.
bool DocumentBuilder::Add(Value* value) {
    if (_open.empty()) {
        if (_root != nullptr) {
            return false;
        }
        _root = value;
        return true;
    }

    Value* parent = _open.back();
    if (parent->GetType() == Type::Array) {
        return parent->Append(value);
    }
    if (!_hasKey) {
        return false;
    }
    _hasKey = false;
    return parent->AddMember(_key, value);
}

// Called when a value is complete. If it was the outermost one, the tree is
// done and becomes the document's root.
bool DocumentBuilder::Finish() {
    return !_open.empty() || _document.SetRoot(_root);
}

bool DocumentBuilder::Enter(Value* container) {
    if (!Add(container)) {
        return false;
    }
    _open.push_back(container);
    return true;
}

bool DocumentBuilder::Exit(bool isObject) {
    if (_open.empty() || _hasKey) {
        return false;
    }
    if ((_open.back()->GetType() == Type::Object) != isObject) {
        return false;
    }
    _open.pop_back();
    return Finish();
}

bool DocumentBuilder::Null() {
    return Add(_document.NewNull()) && Finish();
}

bool DocumentBuilder::Bool(bool value) {
    return Add(_document.NewBool(value)) && Finish();
}

bool DocumentBuilder::Int(int64_t value) {
    return Add(_document.NewInt(value)) && Finish();
}

bool DocumentBuilder::Uint(uint64_t value) {
    return Add(_document.NewUint(value)) && Finish();
}

bool DocumentBuilder::Double(double value) {
    return Add(_document.NewDouble(value)) && Finish();
}

bool DocumentBuilder::String(std::string_view value) {
    return Add(_document.NewString(value)) && Finish();
}

bool DocumentBuilder::EnterObject() {
    return Enter(_document.NewObject());
}

bool DocumentBuilder::Key(std::string_view key) {
    if (_open.empty() || _open.back()->GetType() != Type::Object || _hasKey) {
        return false;
    }
    _key.assign(key.data(), key.size());
    _hasKey = true;
    return true;
}

bool DocumentBuilder::ExitObject() {
    return Exit(true);
}

bool DocumentBuilder::EnterArray() {
    return Enter(_document.NewArray());
}

bool DocumentBuilder::ExitArray() {
    return Exit(false);
}

bool DocumentBuilder::Bytes(std::string_view value) {
    return Add(_document.NewBytes(value)) && Finish();
}

bool DocumentBuilder::DateTime(const tinycodec::DateTime& value) {
    return Add(_document.NewDateTime(value)) && Finish();
}

}  // namespace tinycodec
