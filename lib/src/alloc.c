// Copyright (c) 2021 Johannes Stoelp

#include <alloc.h>
#include <common.h>

#include <stdint.h>

// Extremely simple and non-thread safe implementation of a dynamic
// memory allocator. Which will greatly suffer under fragmentation as
// we neither use splitting nor coalesce free blocks. It uses
// first-fit and always traverses the block list from the beginning.
//
// Bottom line, this allocator can be optimized in so many ways but it
// doesn't really matter for the purpose of this studies and therefore
// the allocator is implemented in the most naive way.

// Allocation block descriptor.
struct BlockDescriptor {
    unsigned mFree;
    unsigned mSize;
    struct BlockDescriptor* mNext;
};

// Global Allocator.

// Size of available memory to the allocator.
enum { MEMORY_SIZE = 1 * 1024 * 1024 };
// Memory for the allocator (statically reserved in the `.bss` section).
uint8_t gMemory[MEMORY_SIZE];

// Top index into `gMemory` to indicate next free memory.
unsigned gMemoryTop;

// List of allocated blocks (free + used).
struct BlockDescriptor* gHead;

// Request free memory from `gMemory` and advance the `gMemoryTop` index.
static void* brk(unsigned size) {
    ERROR_ON(gMemoryTop + size >= MEMORY_SIZE, "Allocator OOM!");
    const unsigned old_top = gMemoryTop;
    gMemoryTop += size;
    return (void*)(gMemory + old_top);
}

// Allocate memory chunk of `size` and return pointer to the chunk.
void* alloc(unsigned size) {
    struct BlockDescriptor* current = 0;

    // Check if we have a free block in the list of allocated blocks
    // that matches the requested size.
    current = gHead;
    while (current) {
        if (current->mFree && current->mSize < size) {
            current->mFree = 0;
            return (void*)(current + 1);
        };
        current = current->mNext;
    }

    // Compute real allocation size: Payload + BlockDescriptor.
    unsigned real_size = size + sizeof(struct BlockDescriptor);

    // No free block found in the list of blocks, allocate new block.
    current = brk(real_size);

    // Initialize new block.
    current->mFree = 0;
    current->mSize = size;
    current->mNext = 0;

    // Insert new block at the beginning of the list of blocks.
    if (gHead != 0) {
        current->mNext = gHead;
    }
    gHead = current;

    return (void*)(current + 1);
}

void dealloc(void* ptr) {
    // Get descriptor block.
    struct BlockDescriptor* current = (struct BlockDescriptor*)ptr - 1;

    // Mark block as free.
    ERROR_ON(current->mFree, "Tried to de-alloc free block!");
    current->mFree = 1;
}
