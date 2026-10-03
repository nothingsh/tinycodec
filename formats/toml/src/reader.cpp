#include "tinycodec/toml/reader.h"

#include <cstring>
#include <new>
#include <type_traits>

#include "datetime_parser.h"
#include "number_parser.h"
#include "string_parser.h"
#include "tinycodec/datetime.h"
#include "tinycodec/utf8.h"

namespace tinycodec {
namespace toml {

// How a node came to be. For tables this decides what may still be added
// to them later (spec 4.2.5).
enum class Reader::NodeKind : uint8_t {
    HeaderTable,     // Defined by a [table] header; also the root.
    ImplicitTable,   // Created by a header path on the way to its last key.
    DottedTable,     // Created by a dotted key on the way to its last key.
    InlineTable,     // { ... }; closed for good once written.
    TableArray,      // [[array of tables]]; its elements are HeaderTables.
    StaticArray,     // [ ... ]; closed for good once written.
    String,
    Integer,
    Float,
    Boolean,
    DateTime,
};

// One value of the parsed document. Nodes live in the Reader's arena.
struct Reader::Node {
    NodeKind kind;
    int depth;                // Containers only: the root is 1.
    const char* position;     // Where Aborted points for this node's own events.
    const char* keyPosition;  // The key segment that named this member; null otherwise.
    std::string_view key;
    Node* first;              // Children, in the order they first appeared.
    Node* last;
    Node* next;               // The next sibling.
    std::string_view text;
    int64_t integer;
    double number;
    bool boolean;
    tinycodec::DateTime dateTime;
};

namespace {

bool IsDigit(char character) {
    return character >= '0' && character <= '9';
}

bool IsBareKeyCharacter(char character) {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')
        || IsDigit(character) || character == '_' || character == '-';
}

// The characters a number or datetime can be made of.
bool IsWordCharacter(char character) {
    return IsBareKeyCharacter(character) || character == '.' || character == ':' || character == '+';
}

// A local date on its own: "1979-05-27".
bool IsDateWord(std::string_view word) {
    if (word.size() != 10 || word[4] != '-' || word[7] != '-') {
        return false;
    }
    for (size_t i : {0, 1, 2, 3, 5, 6, 8, 9}) {
        if (!IsDigit(word[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace

Error Reader::Parse(std::string_view text, Visitor& visitor) {
    _begin = text.data();
    _cursor = _begin;
    _end = _begin + text.size();
    _visitor = &visitor;
    _error = Error();
    _arena.Clear();

    // TOML text must be valid UTF-8 throughout, comments included.
    size_t valid = ValidUtf8Prefix(text);
    if (valid != text.size()) {
        Fail(ErrorCode::InvalidUtf8, _begin + valid);
        return _error;
    }
    if (text.size() >= 3 && std::memcmp(_begin, "\xEF\xBB\xBF", 3) == 0) {
        Fail(ErrorCode::UnexpectedChar, _begin);
        return _error;
    }

    _root = NewContainer(NodeKind::HeaderTable, 1, _begin);
    _current = _root;
    for (;;) {
        SkipSpaces();
        if (_cursor == _end) {
            break;
        }
        if (!ParseLine()) {
            return _error;
        }
    }

    Emit(_root);
    return _error;
}

// Records an error at position and returns false, so that callers can
// write `return Fail(...)`.
bool Reader::Fail(ErrorCode code, const char* position) {
    _error.code = code;
    _error.offset = static_cast<size_t>(position - _begin);
    // Line and column are only needed for errors, so they are worked out
    // here instead of being tracked while parsing. A CRLF counts once.
    _error.line = 1;
    _error.column = 1;
    for (const char* cursor = _begin; cursor != position; ++cursor) {
        if (*cursor == '\n') {
            ++_error.line;
            _error.column = 1;
        } else {
            ++_error.column;
        }
    }
    return false;
}

bool Reader::Delivered(bool accepted, const char* position) {
    return accepted || Fail(ErrorCode::Aborted, position);
}

void Reader::SkipSpaces() {
    while (_cursor != _end && (*_cursor == ' ' || *_cursor == '\t')) {
        ++_cursor;
    }
}

// Parses one line, from its first non-space character to its end.
bool Reader::ParseLine() {
    char character = *_cursor;
    if (character == '[') {
        if (!ParseHeader()) {
            return false;
        }
    } else if (character != '#' && character != '\n' && character != '\r') {
        if (!ParseKeyValue(_current)) {
            return false;
        }
    }
    return EndLine();
}

// Parses a [table] or [[array of tables]] header and makes the table it
// names the current one.
bool Reader::ParseHeader() {
    const char* start = _cursor;
    const bool isArray = _end - _cursor >= 2 && _cursor[1] == '[';
    _cursor += isArray ? 2 : 1;

    std::vector<KeySegment> key;
    if (!ParseKey(&key)) {
        return false;
    }
    for (int i = 0; i < (isArray ? 2 : 1); ++i) {
        if (_cursor == _end) {
            return Fail(ErrorCode::UnexpectedEnd, _cursor);
        }
        if (*_cursor != ']') {
            return Fail(ErrorCode::UnexpectedChar, _cursor);
        }
        ++_cursor;
    }

    Node* parent = _root;
    for (size_t i = 0; i + 1 < key.size(); ++i) {
        parent = DescendHeader(parent, key[i]);
        if (parent == nullptr) {
            return false;
        }
    }
    const KeySegment& last = key.back();
    Node* table = FindChild(parent, last.text);

    if (!isArray) {
        if (table == nullptr) {
            table = NewContainer(NodeKind::HeaderTable, parent->depth + 1, last.position);
            if (table == nullptr) {
                return false;
            }
            table->key = last.text;
            table->keyPosition = last.position;
            AddChild(parent, table);
        } else if (table->kind == NodeKind::ImplicitTable) {
            table->kind = NodeKind::HeaderTable;   // Defined at last.
        } else {
            return Fail(ErrorCode::DuplicateKey, last.position);
        }
        _current = table;
        return true;
    }

    if (table == nullptr) {
        table = NewContainer(NodeKind::TableArray, parent->depth + 1, last.position);
        if (table == nullptr) {
            return false;
        }
        table->key = last.text;
        table->keyPosition = last.position;
        AddChild(parent, table);
    } else if (table->kind != NodeKind::TableArray) {
        return Fail(ErrorCode::DuplicateKey, last.position);
    }
    Node* element = NewContainer(NodeKind::HeaderTable, table->depth + 1, start);
    if (element == nullptr) {
        return false;
    }
    AddChild(table, element);
    _current = element;
    return true;
}

// Follows one segment of a header path from parent, creating an implicit
// table when it does not exist yet. A path goes into the last element of
// an array of tables, but never into an inline table or a value.
Reader::Node* Reader::DescendHeader(Node* parent, const KeySegment& segment) {
    Node* child = FindChild(parent, segment.text);
    if (child == nullptr) {
        child = NewContainer(NodeKind::ImplicitTable, parent->depth + 1, segment.position);
        if (child == nullptr) {
            return nullptr;
        }
        child->key = segment.text;
        child->keyPosition = segment.position;
        AddChild(parent, child);
        return child;
    }
    switch (child->kind) {
    case NodeKind::HeaderTable:
    case NodeKind::ImplicitTable:
    case NodeKind::DottedTable:
        return child;
    case NodeKind::TableArray:
        return child->last;
    default:
        Fail(ErrorCode::DuplicateKey, segment.position);
        return nullptr;
    }
}

// Accepts what may follow the content of a line: spaces, a comment, and
// the line break or the end of the input.
bool Reader::EndLine() {
    SkipSpaces();
    if (!SkipComment()) {
        return false;
    }
    if (_cursor == _end) {
        return true;
    }
    if (*_cursor == '\n') {
        ++_cursor;
        return true;
    }
    if (*_cursor == '\r' && _end - _cursor >= 2 && _cursor[1] == '\n') {
        _cursor += 2;
        return true;
    }
    return Fail(ErrorCode::UnexpectedChar, _cursor);
}

// Skips a comment, if there is one at the cursor, up to its line break.
bool Reader::SkipComment() {
    if (_cursor == _end || *_cursor != '#') {
        return true;
    }
    for (++_cursor; _cursor != _end && *_cursor != '\n'; ++_cursor) {
        unsigned char byte = static_cast<unsigned char>(*_cursor);
        if (byte == '\r' && _end - _cursor >= 2 && _cursor[1] == '\n') {
            break;
        }
        if ((byte < 0x20 && byte != '\t') || byte == 0x7F) {
            return Fail(ErrorCode::UnexpectedChar, _cursor);
        }
    }
    return true;
}

// Skips spaces, line breaks and comments, as allowed between array elements.
bool Reader::SkipBlank() {
    for (;;) {
        SkipSpaces();
        if (!SkipComment()) {
            return false;
        }
        if (_cursor == _end) {
            return true;
        }
        if (*_cursor == '\n') {
            ++_cursor;
        } else if (*_cursor == '\r') {
            if (_end - _cursor < 2 || _cursor[1] != '\n') {
                return Fail(ErrorCode::UnexpectedChar, _cursor);
            }
            _cursor += 2;
        } else {
            return true;
        }
    }
}

Reader::Node* Reader::NewNode(NodeKind kind, const char* position) {
    static_assert(std::is_trivially_destructible<Node>::value, "the arena never runs destructors");
    void* memory = _arena.Allocate(sizeof(Node), alignof(Node));
    Node* node = new (memory) Node();
    node->kind = kind;
    node->position = position;
    return node;
}

// Creates a table or array `depth` levels deep, or fails with
// DepthExceeded at position.
Reader::Node* Reader::NewContainer(NodeKind kind, int depth, const char* position) {
    if (depth > kMaxDepth) {
        Fail(ErrorCode::DepthExceeded, position);
        return nullptr;
    }
    Node* node = NewNode(kind, position);
    node->depth = depth;
    return node;
}

// Returns the content of a string just parsed, which is either part of
// the input or held in _scratch; in the second case it is copied into the
// arena, because _scratch is reused for the next string.
std::string_view Reader::Keep(std::string_view text) {
    if (text.data() != _scratch.data() || text.empty()) {
        return text;
    }
    char* copy = static_cast<char*>(_arena.Allocate(text.size(), 1));
    std::memcpy(copy, text.data(), text.size());
    return std::string_view(copy, text.size());
}

Reader::Node* Reader::FindChild(Node* table, std::string_view key) {
    for (Node* child = table->first; child != nullptr; child = child->next) {
        if (child->key == key) {
            return child;
        }
    }
    return nullptr;
}

void Reader::AddChild(Node* parent, Node* child) {
    if (parent->last == nullptr) {
        parent->first = child;
    } else {
        parent->last->next = child;
    }
    parent->last = child;
}

// Reads a key: one or more segments, bare or quoted, separated by dots.
bool Reader::ParseKey(std::vector<KeySegment>* segments) {
    segments->clear();
    for (;;) {
        SkipSpaces();
        if (_cursor == _end) {
            return Fail(ErrorCode::UnexpectedEnd, _cursor);
        }
        const char* start = _cursor;
        std::string_view text;
        if (*_cursor == '"' || *_cursor == '\'') {
            bool multiline = false;
            const char* stop = nullptr;
            ErrorCode code = ParseString(_cursor, _end, &_scratch, &text, &multiline, &stop);
            if (code != ErrorCode::Ok) {
                return Fail(code, stop);
            }
            if (multiline) {
                return Fail(ErrorCode::UnexpectedChar, start);
            }
            text = Keep(text);
            _cursor = stop;
        } else if (IsBareKeyCharacter(*_cursor)) {
            while (_cursor != _end && IsBareKeyCharacter(*_cursor)) {
                ++_cursor;
            }
            text = std::string_view(start, static_cast<size_t>(_cursor - start));
        } else {
            return Fail(ErrorCode::UnexpectedChar, _cursor);
        }
        segments->push_back({text, start});

        SkipSpaces();
        if (_cursor == _end || *_cursor != '.') {
            return true;
        }
        ++_cursor;  // '.'
    }
}

// Parses "key = value" and adds the value to table.
bool Reader::ParseKeyValue(Node* table) {
    std::vector<KeySegment> key;
    if (!ParseKey(&key)) {
        return false;
    }
    if (_cursor == _end) {
        return Fail(ErrorCode::UnexpectedEnd, _cursor);
    }
    if (*_cursor != '=') {
        return Fail(ErrorCode::UnexpectedChar, _cursor);
    }
    ++_cursor;  // '='
    SkipSpaces();

    Node* parent = table;
    for (size_t i = 0; i + 1 < key.size(); ++i) {
        parent = DescendDotted(parent, key[i]);
        if (parent == nullptr) {
            return false;
        }
    }
    const KeySegment& last = key.back();
    if (FindChild(parent, last.text) != nullptr) {
        return Fail(ErrorCode::DuplicateKey, last.position);
    }

    Node* value = ParseValue(parent->depth + 1);
    if (value == nullptr) {
        return false;
    }
    value->key = last.text;
    value->keyPosition = last.position;
    AddChild(parent, value);
    return true;
}

// Follows one segment of a dotted key from parent, creating the table when
// it does not exist yet. Only tables made by dotted keys, and tables a
// header path made on the way, may be entered this way.
Reader::Node* Reader::DescendDotted(Node* parent, const KeySegment& segment) {
    Node* child = FindChild(parent, segment.text);
    if (child == nullptr) {
        child = NewContainer(NodeKind::DottedTable, parent->depth + 1, segment.position);
        if (child == nullptr) {
            return nullptr;
        }
        child->key = segment.text;
        child->keyPosition = segment.position;
        AddChild(parent, child);
        return child;
    }
    if (child->kind == NodeKind::DottedTable || child->kind == NodeKind::ImplicitTable) {
        return child;
    }
    Fail(ErrorCode::DuplicateKey, segment.position);
    return nullptr;
}

// Parses the value at the cursor. depth is the level it gets if it is an
// array or inline table.
Reader::Node* Reader::ParseValue(int depth) {
    if (_cursor == _end) {
        Fail(ErrorCode::UnexpectedEnd, _cursor);
        return nullptr;
    }
    std::string_view rest(_cursor, static_cast<size_t>(_end - _cursor));
    switch (*_cursor) {
    case '"':
    case '\'':
        return ParseStringValue();
    case '[':
        return ParseArray(depth);
    case '{':
        return ParseInlineTable(depth);
    case 't':
        return ParseLiteral("true", true);
    case 'f':
        return ParseLiteral("false", false);
    default:
        if (IsDigit(*_cursor) || *_cursor == '+' || *_cursor == '-'
                || rest.compare(0, 3, "inf") == 0 || rest.compare(0, 3, "nan") == 0) {
            return ParseWord();
        }
        Fail(ErrorCode::UnexpectedChar, _cursor);
        return nullptr;
    }
}

Reader::Node* Reader::ParseStringValue() {
    const char* start = _cursor;
    std::string_view text;
    bool multiline = false;
    const char* stop = nullptr;
    ErrorCode code = ParseString(_cursor, _end, &_scratch, &text, &multiline, &stop);
    if (code != ErrorCode::Ok) {
        Fail(code, stop);
        return nullptr;
    }
    _cursor = stop;
    Node* node = NewNode(NodeKind::String, start);
    node->text = Keep(text);
    return node;
}

Reader::Node* Reader::ParseLiteral(std::string_view literal, bool value) {
    const char* start = _cursor;
    for (char expected : literal) {
        if (_cursor == _end) {
            Fail(ErrorCode::UnexpectedEnd, _cursor);
            return nullptr;
        }
        if (*_cursor != expected) {
            Fail(ErrorCode::UnexpectedChar, _cursor);
            return nullptr;
        }
        ++_cursor;
    }
    Node* node = NewNode(NodeKind::Boolean, start);
    node->boolean = value;
    return node;
}

// Parses a number or a datetime: first the whole word, then its meaning.
Reader::Node* Reader::ParseWord() {
    const char* start = _cursor;
    while (_cursor != _end && IsWordCharacter(*_cursor)) {
        ++_cursor;
    }
    std::string_view word(start, static_cast<size_t>(_cursor - start));
    // "1979-05-27 07:32:00": a space may separate the date from the time.
    if (IsDateWord(word) && _end - _cursor >= 2 && _cursor[0] == ' ' && IsDigit(_cursor[1])) {
        ++_cursor;
        while (_cursor != _end && IsWordCharacter(*_cursor)) {
            ++_cursor;
        }
        word = std::string_view(start, static_cast<size_t>(_cursor - start));
    }

    if (IsDateTimeWord(word)) {
        tinycodec::DateTime value;
        size_t errorOffset = 0;
        ErrorCode code = ParseDateTime(word, &value, &errorOffset);
        if (code != ErrorCode::Ok) {
            const char* position = start + errorOffset;
            Fail(code == ErrorCode::UnexpectedChar && position == _end ? ErrorCode::UnexpectedEnd : code, position);
            return nullptr;
        }
        Node* node = NewNode(NodeKind::DateTime, start);
        node->dateTime = value;
        return node;
    }

    Number number;
    if (!ParseNumber(word, &number)) {
        Fail(ErrorCode::InvalidNumber, start);
        return nullptr;
    }
    Node* node = nullptr;
    if (number.kind == NumberKind::Int) {
        node = NewNode(NodeKind::Integer, start);
        node->integer = number.intValue;
    } else {
        node = NewNode(NodeKind::Float, start);
        node->number = number.doubleValue;
    }
    return node;
}

// [ value, value, ... ] with line breaks and comments allowed anywhere
// between the brackets, and an optional comma after the last value.
Reader::Node* Reader::ParseArray(int depth) {
    Node* array = NewContainer(NodeKind::StaticArray, depth, _cursor);
    if (array == nullptr) {
        return nullptr;
    }
    ++_cursor;  // '['
    for (;;) {
        if (!SkipBlank()) {
            return nullptr;
        }
        if (_cursor != _end && *_cursor == ']') {
            ++_cursor;
            return array;
        }
        Node* element = ParseValue(depth + 1);
        if (element == nullptr) {
            return nullptr;
        }
        AddChild(array, element);

        if (!SkipBlank()) {
            return nullptr;
        }
        if (_cursor == _end) {
            Fail(ErrorCode::UnexpectedEnd, _cursor);
            return nullptr;
        }
        if (*_cursor == ']') {
            ++_cursor;
            return array;
        }
        if (*_cursor != ',') {
            Fail(ErrorCode::UnexpectedChar, _cursor);
            return nullptr;
        }
        ++_cursor;  // ','
    }
}

// { key = value, ... } on one line, with no comma after the last pair.
Reader::Node* Reader::ParseInlineTable(int depth) {
    Node* table = NewContainer(NodeKind::InlineTable, depth, _cursor);
    if (table == nullptr) {
        return nullptr;
    }
    ++_cursor;  // '{'
    SkipSpaces();
    if (_cursor != _end && *_cursor == '}') {
        ++_cursor;
        return table;
    }
    for (;;) {
        if (!ParseKeyValue(table)) {
            return nullptr;
        }
        SkipSpaces();
        if (_cursor == _end) {
            Fail(ErrorCode::UnexpectedEnd, _cursor);
            return nullptr;
        }
        if (*_cursor == '}') {
            ++_cursor;
            return table;
        }
        if (*_cursor != ',') {
            Fail(ErrorCode::UnexpectedChar, _cursor);
            return nullptr;
        }
        ++_cursor;  // ','
    }
}

// Sends node and everything below it to the visitor.
bool Reader::Emit(const Node* node) {
    switch (node->kind) {
    case NodeKind::HeaderTable:
    case NodeKind::ImplicitTable:
    case NodeKind::DottedTable:
    case NodeKind::InlineTable:
        if (!Delivered(_visitor->EnterObject(), node->position)) {
            return false;
        }
        for (const Node* child = node->first; child != nullptr; child = child->next) {
            if (!Delivered(_visitor->Key(child->key), child->keyPosition) || !Emit(child)) {
                return false;
            }
        }
        return Delivered(_visitor->ExitObject(), node->position);
    case NodeKind::TableArray:
    case NodeKind::StaticArray:
        if (!Delivered(_visitor->EnterArray(), node->position)) {
            return false;
        }
        for (const Node* child = node->first; child != nullptr; child = child->next) {
            if (!Emit(child)) {
                return false;
            }
        }
        return Delivered(_visitor->ExitArray(), node->position);
    case NodeKind::String:
        return Delivered(_visitor->String(node->text), node->position);
    case NodeKind::Integer:
        return Delivered(_visitor->Int(node->integer), node->position);
    case NodeKind::Float:
        return Delivered(_visitor->Double(node->number), node->position);
    case NodeKind::Boolean:
        return Delivered(_visitor->Bool(node->boolean), node->position);
    case NodeKind::DateTime:
        return Delivered(_visitor->DateTime(node->dateTime), node->position);
    }
    return false;
}

}  // namespace toml
}  // namespace tinycodec
