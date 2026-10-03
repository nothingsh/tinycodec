#ifndef TINYCODEC_TOML_READER_H
#define TINYCODEC_TOML_READER_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "tinycodec/arena.h"
#include "tinycodec/error.h"
#include "tinycodec/toml/limits.h"
#include "tinycodec/visitor.h"

namespace tinycodec {
namespace toml {

// Parses TOML 1.0.0 text and reports it to a Visitor as events: the root
// table and every other table as an object, arrays (including arrays of
// tables) as arrays, and datetimes as DateTime events. Integers are always
// Int events.
//
// TOML lets a table be written in pieces ([a.b] ... [c] ... [a]), so the
// text is first parsed into a tree, and only when all of it is valid is the
// tree replayed as events. A Visitor therefore sees no event at all for
// invalid text. Members come in the order in which their keys first appear.
//
// A Reader can be used for any number of Parse calls, one at a time; it must
// not be shared between threads.
class Reader {
public:
    // Parses text, which must be a whole TOML document. Returns an Error
    // whose code is Ok on success. When the visitor returns false, parsing
    // stops and the code is Aborted, positioned in the source of the
    // rejected event:
    //  - a scalar: the start of the value;
    //  - a Key: the key segment that named the member;
    //  - EnterObject, EnterArray, ExitObject, ExitArray: the container. That
    //    is the key segment that created a table or array of tables, the
    //    "[[" of an array-of-tables element, the "[" or "{" of an array or
    //    inline table, and offset 0 for the root table.
    Error Parse(std::string_view text, Visitor& visitor);

private:
    enum class NodeKind : uint8_t;   // Defined with Node in reader.cpp.
    struct Node;
    struct KeySegment {
        std::string_view text;
        const char* position;   // The first character of the segment.
    };

    bool ParseLine();
    bool ParseHeader();
    Node* DescendHeader(Node* parent, const KeySegment& segment);
    bool EndLine();
    bool SkipComment();
    bool SkipBlank();
    void SkipSpaces();
    bool ParseKey(std::vector<KeySegment>* segments);
    bool ParseKeyValue(Node* table);
    Node* DescendDotted(Node* parent, const KeySegment& segment);
    Node* ParseValue(int depth);
    Node* ParseStringValue();
    Node* ParseLiteral(std::string_view literal, bool value);
    Node* ParseWord();
    Node* ParseArray(int depth);
    Node* ParseInlineTable(int depth);
    static Node* FindChild(Node* table, std::string_view key);
    static void AddChild(Node* parent, Node* child);
    Node* NewNode(NodeKind kind, const char* position);
    Node* NewContainer(NodeKind kind, int depth, const char* position);
    std::string_view Keep(std::string_view text);
    bool Emit(const Node* node);
    bool Fail(ErrorCode code, const char* position);
    bool Delivered(bool accepted, const char* position);

    const char* _begin = nullptr;
    const char* _cursor = nullptr;
    const char* _end = nullptr;
    Visitor* _visitor = nullptr;
    Error _error;
    Arena _arena;           // The tree of the current Parse.
    std::string _scratch;   // Holds the decoded text of the current string.
    Node* _root = nullptr;
    Node* _current = nullptr;   // The table that key/value lines go into.
};

}  // namespace toml
}  // namespace tinycodec

#endif
