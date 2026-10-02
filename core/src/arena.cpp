#include "tinycodec/arena.h"

#include <cassert>
#include <cstdint>
#include <new>

namespace tinycodec {

namespace {

// Number of bytes to skip so that pointer becomes a multiple of alignment.
size_t Padding(const char* pointer, size_t alignment) {
    size_t misalignment = reinterpret_cast<uintptr_t>(pointer) & (alignment - 1);
    return misalignment == 0 ? 0 : alignment - misalignment;
}

}  // namespace

Arena::~Arena() {
    Clear();
}

// Allocates a block with capacity usable bytes, links it into the list and
// returns the address of its first usable byte.
char* Arena::NewBlock(size_t capacity) {
    Block* block = static_cast<Block*>(::operator new(sizeof(Block) + capacity));
    block->next = _blocks;
    _blocks = block;
    return reinterpret_cast<char*>(block + 1);
}

void* Arena::Allocate(size_t size, size_t alignment) {
    assert(alignment != 0 && (alignment & (alignment - 1)) == 0);

    if (_cursor != nullptr) {
        size_t padding = Padding(_cursor, alignment);
        size_t available = static_cast<size_t>(_limit - _cursor);
        if (padding <= available && size <= available - padding) {
            char* result = _cursor + padding;
            _cursor = result + size;
            return result;
        }
    }

    // A request too large for a regular block gets a block of its own. The
    // current block stays current, so its remaining space is not wasted.
    if (size + alignment > BLOCK_SIZE) {
        char* data = NewBlock(size + alignment);
        return data + Padding(data, alignment);
    }

    char* data = NewBlock(BLOCK_SIZE);
    char* result = data + Padding(data, alignment);
    _cursor = result + size;
    _limit = data + BLOCK_SIZE;
    return result;
}

void Arena::Clear() {
    while (_blocks != nullptr) {
        Block* next = _blocks->next;
        ::operator delete(_blocks);
        _blocks = next;
    }
    _cursor = nullptr;
    _limit = nullptr;
}

}  // namespace tinycodec
