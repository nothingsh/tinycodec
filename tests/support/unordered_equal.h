#ifndef TINYCODEC_TESTS_UNORDERED_EQUAL_H
#define TINYCODEC_TESTS_UNORDERED_EQUAL_H

#include <cmath>

#include "tinycodec/value.h"

// Test helper: like Value::Equals, except that object members may come in
// any order and a NaN equals a NaN. TOML tables are unordered, so this is
// how trees that went through TOML are compared. Objects must not repeat
// a key.
inline bool EqualsIgnoringOrder(const tinycodec::Value& a, const tinycodec::Value& b) {
    using tinycodec::Type;
    if (a.GetType() == Type::Object && b.GetType() == Type::Object) {
        if (a.Size() != b.Size()) {
            return false;
        }
        for (const tinycodec::Value* member = a.FirstChild(); member != nullptr; member = member->Next()) {
            const tinycodec::Value* other = b.Find(member->Key());
            if (other == nullptr || !EqualsIgnoringOrder(*member, *other)) {
                return false;
            }
        }
        return true;
    }
    if (a.GetType() == Type::Array && b.GetType() == Type::Array) {
        if (a.Size() != b.Size()) {
            return false;
        }
        const tinycodec::Value* other = b.FirstChild();
        for (const tinycodec::Value* element = a.FirstChild(); element != nullptr; element = element->Next()) {
            if (!EqualsIgnoringOrder(*element, *other)) {
                return false;
            }
            other = other->Next();
        }
        return true;
    }
    double x = 0.0;
    double y = 0.0;
    if (a.GetType() == Type::Double && b.GetType() == Type::Double
            && a.QueryDouble(&x) && b.QueryDouble(&y) && std::isnan(x) && std::isnan(y)) {
        return true;
    }
    return a.Equals(b);
}

#endif
