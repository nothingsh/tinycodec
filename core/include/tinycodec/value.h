#ifndef TINYCODEC_VALUE_H
#define TINYCODEC_VALUE_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tinycodec {

class Document;
class Visitor;

enum class Type { Null, Bool, Int, Uint, Double, String, Bytes, Array, Object };

// One node of a document tree. Values are created by a Document, which owns
// them; they are never copied or deleted individually.
class Value {
public:
    Type GetType() const { return _type; }

    // Scalar access. On a type mismatch these return false and leave *out
    // untouched. Numbers convert only when no information is lost.
    bool QueryBool(bool* out) const;
    bool QueryInt(int64_t* out) const;        // Int, or Uint up to INT64_MAX.
    bool QueryUint(uint64_t* out) const;      // Uint, or non-negative Int.
    bool QueryDouble(double* out) const;      // Double, Int or Uint.
    bool QueryString(std::string_view* out) const;   // String only, never Bytes.
    bool QueryBytes(std::string_view* out) const;    // Bytes only, never String.

    // Container access. These return null when the type does not match or
    // nothing is found.
    size_t Size() const;                             // 0 unless Array or Object.
    const Value* Find(std::string_view key) const;   // Object; first match. O(n).
    Value* Find(std::string_view key);
    const Value* At(size_t index) const;             // Array. O(n).
    Value* At(size_t index);
    const Value* FirstChild() const;                 // Array or Object.
    Value* FirstChild();
    const Value* Next() const { return _next; }      // Next sibling.
    Value* Next() { return _next; }
    std::string_view Key() const;                    // Empty unless an Object member.

    // Container modification. On failure these return false and change
    // nothing. They fail when this value has the wrong type, or when child
    // is null, belongs to another Document, is already attached somewhere,
    // or is this value or one of its ancestors.
    bool Append(Value* child);                       // Array.
    bool Set(std::string_view key, Value* child);    // Object; replaces the first match.

    // Replays this subtree as events. Returns false as soon as the visitor does.
    bool Accept(Visitor& visitor) const;

    // Deep comparison. Int and Uint compare by numeric value; a Double only
    // equals a Double; Bytes only equal Bytes, never a String with the same
    // content; object members must match in order.
    bool Equals(const Value& other) const;

private:
    friend class Document;
    friend class DocumentBuilder;

    Value() = default;
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;

    bool IsContainer() const { return _type == Type::Array || _type == Type::Object; }
    bool CanAttach(const Value* child) const;
    void LinkAtEnd(Value* child);
    // Appends a member without looking for an existing key.
    bool AddMember(std::string_view key, Value* child);

    struct StringData {
        const char* data;
        size_t size;
    };
    struct ContainerData {
        Value* first;
        Value* last;
        size_t count;
    };

    Type _type = Type::Null;
    Document* _document = nullptr;
    Value* _parent = nullptr;
    Value* _next = nullptr;
    StringData _key = {nullptr, 0};
    union {
        bool _bool;
        int64_t _int;
        uint64_t _uint;
        double _double;
        StringData _string;   // String and Bytes.
        ContainerData _container = {nullptr, nullptr, 0};
    };
};

}  // namespace tinycodec

#endif
