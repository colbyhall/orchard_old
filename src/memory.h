#ifndef MEMORY_H
#define MEMORY_H

#include "language_layer.h"

typedef struct Allocator {
    void* data;
    void* (*proc)(struct Allocator allocator, void* ptr, usize size, usize alignment);
} Allocator;

inline void* mem_alloc_aligned(Allocator allocator, usize size, usize alignment) {
    return allocator.proc(allocator, 0, size, alignment);
}

inline void* mem_alloc(Allocator allocator, usize size) { 
    return mem_alloc_aligned(allocator, size, 4);
}

#define mem_alloc_struct(allocator, type) mem_alloc(allocator, sizeof(type))

inline void mem_free(Allocator allocator, void* ptr) {
    allocator.proc(allocator, ptr, 0, 0);
}

Allocator heap_allocator(void);

typedef struct Memory_Arena {
    u8*     base;
    usize   used;
    usize   total;
} Memory_Arena;

Allocator arena_allocator(void* base, usize size);

#endif /* MEMORY_H */