#include "doctest.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "tinycodec/arena.h"

using namespace tinycodec;

namespace {

bool IsAligned(const void* pointer, size_t alignment) {
    return reinterpret_cast<uintptr_t>(pointer) % alignment == 0;
}

}  // namespace

TEST_CASE("Allocate honours the requested alignment") {
    Arena arena;
    arena.Allocate(1, 1);  // Leave the cursor at an odd address.
    for (size_t alignment : {size_t(1), size_t(2), size_t(4), size_t(8), size_t(16), size_t(64)}) {
        void* pointer = arena.Allocate(3, alignment);
        CHECK(pointer != nullptr);
        CHECK(IsAligned(pointer, alignment));
    }
}

TEST_CASE("allocations do not overlap, also across block boundaries") {
    Arena arena;
    std::vector<unsigned char*> chunks;
    // 200 chunks of 100 bytes need about five 4 KiB blocks.
    for (int i = 0; i < 200; ++i) {
        unsigned char* chunk = static_cast<unsigned char*>(arena.Allocate(100, 1));
        std::memset(chunk, i, 100);
        chunks.push_back(chunk);
    }
    for (int i = 0; i < 200; ++i) {
        bool intact = true;
        for (int j = 0; j < 100; ++j) {
            intact = intact && chunks[i][j] == static_cast<unsigned char>(i);
        }
        CHECK(intact);
    }
}

TEST_CASE("a request larger than a block is served and leaves earlier data intact") {
    Arena arena;
    char* small = static_cast<char*>(arena.Allocate(8, 1));
    std::memcpy(small, "before!", 8);

    char* large = static_cast<char*>(arena.Allocate(100000, 16));
    CHECK(IsAligned(large, 16));
    std::memset(large, 'x', 100000);

    char* after = static_cast<char*>(arena.Allocate(8, 1));
    std::memcpy(after, "after!!", 8);

    CHECK(std::strcmp(small, "before!") == 0);
    CHECK(std::strcmp(after, "after!!") == 0);
    CHECK(large[0] == 'x');
    CHECK(large[99999] == 'x');
}

TEST_CASE("a zero-sized request returns a usable pointer") {
    Arena arena;
    CHECK(arena.Allocate(0, 1) != nullptr);
}

TEST_CASE("the arena can be used again after Clear") {
    Arena arena;
    arena.Allocate(5000, 8);
    arena.Allocate(10, 8);
    arena.Clear();

    char* pointer = static_cast<char*>(arena.Allocate(16, 8));
    CHECK(pointer != nullptr);
    std::memset(pointer, 0, 16);
    arena.Clear();
    arena.Clear();  // Clearing an empty arena is harmless.
}
