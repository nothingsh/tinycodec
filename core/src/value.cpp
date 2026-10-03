#include "tinycodec/value.h"

#include "tinycodec/document.h"
#include "tinycodec/visitor.h"

namespace tinycodec {

namespace {

bool IsInteger(Type type) {
    return type == Type::Int || type == Type::Uint;
}

}  // namespace

bool Value::QueryBool(bool* out) const {
    if (_type != Type::Bool) {
        return false;
    }
    *out = _bool;
    return true;
}

bool Value::QueryInt(int64_t* out) const {
    if (_type == Type::Int) {
        *out = _int;
        return true;
    }
    if (_type == Type::Uint && _uint <= static_cast<uint64_t>(INT64_MAX)) {
        *out = static_cast<int64_t>(_uint);
        return true;
    }
    return false;
}

bool Value::QueryUint(uint64_t* out) const {
    if (_type == Type::Uint) {
        *out = _uint;
        return true;
    }
    if (_type == Type::Int && _int >= 0) {
        *out = static_cast<uint64_t>(_int);
        return true;
    }
    return false;
}

bool Value::QueryDouble(double* out) const {
    switch (_type) {
    case Type::Double:
        *out = _double;
        return true;
    case Type::Int:
        *out = static_cast<double>(_int);
        return true;
    case Type::Uint:
        *out = static_cast<double>(_uint);
        return true;
    default:
        return false;
    }
}

bool Value::QueryString(std::string_view* out) const {
    if (_type != Type::String) {
        return false;
    }
    *out = std::string_view(_string.data, _string.size);
    return true;
}

bool Value::QueryBytes(std::string_view* out) const {
    if (_type != Type::Bytes) {
        return false;
    }
    *out = std::string_view(_string.data, _string.size);
    return true;
}

size_t Value::Size() const {
    return IsContainer() ? _container.count : 0;
}

const Value* Value::Find(std::string_view key) const {
    if (_type != Type::Object) {
        return nullptr;
    }
    for (const Value* child = _container.first; child != nullptr; child = child->_next) {
        if (child->Key() == key) {
            return child;
        }
    }
    return nullptr;
}

Value* Value::Find(std::string_view key) {
    return const_cast<Value*>(static_cast<const Value*>(this)->Find(key));
}

const Value* Value::At(size_t index) const {
    if (_type != Type::Array || index >= _container.count) {
        return nullptr;
    }
    const Value* child = _container.first;
    for (size_t i = 0; i < index; ++i) {
        child = child->_next;
    }
    return child;
}

Value* Value::At(size_t index) {
    return const_cast<Value*>(static_cast<const Value*>(this)->At(index));
}

const Value* Value::FirstChild() const {
    return IsContainer() ? _container.first : nullptr;
}

Value* Value::FirstChild() {
    return IsContainer() ? _container.first : nullptr;
}

std::string_view Value::Key() const {
    return std::string_view(_key.data, _key.size);
}

bool Value::CanAttach(const Value* child) const {
    if (child == nullptr || child->_document != _document) {
        return false;
    }
    if (child->_parent != nullptr || child == _document->Root()) {
        return false;
    }
    // Attaching this value or one of its ancestors would create a cycle.
    for (const Value* ancestor = this; ancestor != nullptr; ancestor = ancestor->_parent) {
        if (ancestor == child) {
            return false;
        }
    }
    return true;
}

void Value::LinkAtEnd(Value* child) {
    child->_parent = this;
    child->_next = nullptr;
    if (_container.last == nullptr) {
        _container.first = child;
    } else {
        _container.last->_next = child;
    }
    _container.last = child;
    ++_container.count;
}

bool Value::Append(Value* child) {
    if (_type != Type::Array || !CanAttach(child)) {
        return false;
    }
    LinkAtEnd(child);
    return true;
}

bool Value::AddMember(std::string_view key, Value* child) {
    if (_type != Type::Object || !CanAttach(child)) {
        return false;
    }
    child->_key.data = _document->CopyString(key);
    child->_key.size = key.size();
    LinkAtEnd(child);
    return true;
}

bool Value::Set(std::string_view key, Value* child) {
    if (_type != Type::Object || !CanAttach(child)) {
        return false;
    }

    Value* previous = nullptr;
    Value* existing = _container.first;
    while (existing != nullptr && existing->Key() != key) {
        previous = existing;
        existing = existing->_next;
    }
    if (existing == nullptr) {
        return AddMember(key, child);
    }

    // Put child where the existing member is, and take over its key.
    child->_key = existing->_key;
    child->_parent = this;
    child->_next = existing->_next;
    if (previous == nullptr) {
        _container.first = child;
    } else {
        previous->_next = child;
    }
    if (_container.last == existing) {
        _container.last = child;
    }

    existing->_key = {nullptr, 0};
    existing->_parent = nullptr;
    existing->_next = nullptr;
    return true;
}

bool Value::Accept(Visitor& visitor) const {
    switch (_type) {
    case Type::Null:
        return visitor.Null();
    case Type::Bool:
        return visitor.Bool(_bool);
    case Type::Int:
        return visitor.Int(_int);
    case Type::Uint:
        if (_uint <= static_cast<uint64_t>(INT64_MAX)) {
            return visitor.Int(static_cast<int64_t>(_uint));
        }
        return visitor.Uint(_uint);
    case Type::Double:
        return visitor.Double(_double);
    case Type::String:
        return visitor.String(std::string_view(_string.data, _string.size));
    case Type::Bytes:
        return visitor.Bytes(std::string_view(_string.data, _string.size));
    case Type::Array:
        if (!visitor.EnterArray()) {
            return false;
        }
        for (const Value* child = _container.first; child != nullptr; child = child->_next) {
            if (!child->Accept(visitor)) {
                return false;
            }
        }
        return visitor.ExitArray();
    case Type::Object:
        if (!visitor.EnterObject()) {
            return false;
        }
        for (const Value* child = _container.first; child != nullptr; child = child->_next) {
            if (!visitor.Key(child->Key()) || !child->Accept(visitor)) {
                return false;
            }
        }
        return visitor.ExitObject();
    }
    return false;
}

bool Value::Equals(const Value& other) const {
    if (IsInteger(_type) && IsInteger(other._type)) {
        if (_type == other._type) {
            return _type == Type::Int ? _int == other._int : _uint == other._uint;
        }
        const Value& asInt = _type == Type::Int ? *this : other;
        const Value& asUint = _type == Type::Int ? other : *this;
        return asInt._int >= 0 && static_cast<uint64_t>(asInt._int) == asUint._uint;
    }
    if (_type != other._type) {
        return false;
    }

    switch (_type) {
    case Type::Null:
        return true;
    case Type::Bool:
        return _bool == other._bool;
    case Type::Double:
        return _double == other._double;
    case Type::String:
    case Type::Bytes:
        return std::string_view(_string.data, _string.size)
            == std::string_view(other._string.data, other._string.size);
    case Type::Array:
    case Type::Object: {
        if (_container.count != other._container.count) {
            return false;
        }
        const Value* mine = _container.first;
        const Value* theirs = other._container.first;
        while (mine != nullptr) {
            if (_type == Type::Object && mine->Key() != theirs->Key()) {
                return false;
            }
            if (!mine->Equals(*theirs)) {
                return false;
            }
            mine = mine->_next;
            theirs = theirs->_next;
        }
        return true;
    }
    default:
        return false;  // Int and Uint were handled above.
    }
}

}  // namespace tinycodec
