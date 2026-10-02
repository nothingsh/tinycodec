#include "tinycodec/document.h"

#include <cstring>
#include <new>
#include <type_traits>

namespace tinycodec {

// The arena never runs destructors.
static_assert(std::is_trivially_destructible<Value>::value, "Value must be trivially destructible");

bool Document::SetRoot(Value* value) {
    if (value == nullptr || value->_document != this) {
        return false;
    }
    if (value->_parent != nullptr || value == _root) {
        return false;
    }
    _root = value;
    return true;
}

Value* Document::NewValue(Type type) {
    void* memory = _arena.Allocate(sizeof(Value), alignof(Value));
    Value* value = new (memory) Value();
    value->_type = type;
    value->_document = this;
    return value;
}

const char* Document::CopyString(std::string_view text) {
    if (text.empty()) {
        return "";
    }
    char* copy = static_cast<char*>(_arena.Allocate(text.size(), 1));
    std::memcpy(copy, text.data(), text.size());
    return copy;
}

Value* Document::NewNull() {
    return NewValue(Type::Null);
}

Value* Document::NewBool(bool value) {
    Value* result = NewValue(Type::Bool);
    result->_bool = value;
    return result;
}

Value* Document::NewInt(int64_t value) {
    Value* result = NewValue(Type::Int);
    result->_int = value;
    return result;
}

Value* Document::NewUint(uint64_t value) {
    Value* result = NewValue(Type::Uint);
    result->_uint = value;
    return result;
}

Value* Document::NewDouble(double value) {
    Value* result = NewValue(Type::Double);
    result->_double = value;
    return result;
}

Value* Document::NewString(std::string_view value) {
    Value* result = NewValue(Type::String);
    result->_string.data = CopyString(value);
    result->_string.size = value.size();
    return result;
}

Value* Document::NewArray() {
    Value* result = NewValue(Type::Array);
    result->_container = {nullptr, nullptr, 0};
    return result;
}

Value* Document::NewObject() {
    Value* result = NewValue(Type::Object);
    result->_container = {nullptr, nullptr, 0};
    return result;
}

void Document::Clear() {
    _arena.Clear();
    _root = nullptr;
}

}  // namespace tinycodec
