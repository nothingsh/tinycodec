#ifndef TINYCODEC_TESTS_RECORDING_VISITOR_H
#define TINYCODEC_TESTS_RECORDING_VISITOR_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hex.h"
#include "tinycodec/visitor.h"

// Test helper: writes every event it receives into `events` as text, e.g.
// "Int(1)", "Key(name)" or "Bytes(00ff)" (Bytes are shown in hex). Set
// `failAt` to make the event with that index (counting from 0) return
// false; that event is still recorded.
class RecordingVisitor : public tinycodec::Visitor {
public:
    std::vector<std::string> events;
    int failAt = -1;

    bool Null() override { return Record("Null"); }
    bool Bool(bool value) override { return Record(value ? "Bool(true)" : "Bool(false)"); }
    bool Int(int64_t value) override { return Record("Int(" + std::to_string(value) + ")"); }
    bool Uint(uint64_t value) override { return Record("Uint(" + std::to_string(value) + ")"); }
    bool Double(double value) override { return Record("Double(" + std::to_string(value) + ")"); }
    bool String(std::string_view value) override { return Record("String(" + std::string(value) + ")"); }
    bool EnterObject() override { return Record("EnterObject"); }
    bool Key(std::string_view key) override { return Record("Key(" + std::string(key) + ")"); }
    bool ExitObject() override { return Record("ExitObject"); }
    bool EnterArray() override { return Record("EnterArray"); }
    bool ExitArray() override { return Record("ExitArray"); }
    bool Bytes(std::string_view value) override { return Record("Bytes(" + ToHex(value) + ")"); }

private:
    bool Record(std::string event) {
        events.push_back(std::move(event));
        return static_cast<int>(events.size()) - 1 != failAt;
    }
};

#endif
