#include "language_layer.h"

static usize get_alignment_offset(void* ptr, usize alignment) {
    if ((usize)ptr & (alignment - 1)) {
        return alignment - ((usize)ptr & alignment);
    }
    return 0;
}

static void* heap_alloc(Allocator allocator, void* ptr, usize size, usize alignment) {
    if (size) {
        if (ptr) return realloc(ptr, size);
        else return malloc(size);
    } else {
        free(ptr);
        return 0;
    }
}

Allocator heap_allocator(void) {
    return (Allocator) { 0, heap_alloc };
}

static void* arena_alloc(Allocator allocator, void* ptr, usize size, usize alignment) {
    Memory_Arena* const arena = allocator.data;

    if (size) {
        if (ptr) assert(false); // @Incomplete
        else {
            u8* result = arena->base + arena->used;
            const usize offset = get_alignment_offset(result, alignment);
            assert(arena->used + size + offset < arena->total);
            result += offset;
            arena->used += size + offset;
            return result;
        }
    }

    return 0;
}

Allocator arena_allocator(void* base, usize size) {
    assert(sizeof(Memory_Arena) < size);

    Memory_Arena* const arena = base;
    *arena = (Memory_Arena) { 
        (u8*)base + sizeof(Memory_Arena),
        0,
        size - sizeof(Memory_Arena)
    };

    return (Allocator) { base, arena_alloc };
}