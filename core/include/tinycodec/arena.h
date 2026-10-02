#ifndef TINYCODEC_ARENA_H
#define TINYCODEC_ARENA_H

#include <cstddef>

namespace tinycodec {

// A bump allocator. Memory is handed out from 4 KiB blocks and is only
// released all at once, by Clear() or the destructor. No destructors are
// run, so only trivially destructible types may be placed in an Arena.
class Arena {
public:
    Arena() = default;
    ~Arena();
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    // Returns size bytes aligned to alignment, which must be a power of two.
    // Never returns null.
    void* Allocate(size_t size, size_t alignment);

    // Releases every block. All pointers handed out so far become invalid.
    void Clear();

private:
    enum : size_t { BLOCK_SIZE = 4 * 1024 };

    // Header at the start of every block; the usable bytes follow it.
    // The alignment keeps those bytes aligned for any ordinary type.
    struct alignas(std::max_align_t) Block {
        Block* next;
    };

    char* NewBlock(size_t capacity);

    Block* _blocks = nullptr;   // Every block, newest first.
    char* _cursor = nullptr;    // Next free byte in the current block.
    char* _limit = nullptr;     // End of the current block.
};

}  // namespace tinycodec

#endif
