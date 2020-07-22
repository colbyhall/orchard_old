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
    Memory_Arena* arena = allocator.data;

    if (size) {
        if (ptr) {
            u8* result = arena_alloc(allocator, 0, size, alignment);
            mem_copy(result, ptr, size);
            return result;
        }

        u8* result = arena->base + arena->used;
        usize offset = get_alignment_offset(result, alignment);
        assert(arena->used + size + offset < arena->total);
        result += offset;
        arena->used += size + offset;
        return result;
    }

    return 0;
}

Allocator arena_allocator_raw(void* base, usize size) {
    assert(sizeof(Memory_Arena) < size);

    Memory_Arena* arena = base;
    *arena = (Memory_Arena) { 
        (u8*)base + sizeof(Memory_Arena),
        0,
        size - sizeof(Memory_Arena)
    };

    return (Allocator) { base, arena_alloc };
}

Allocator arena_allocator(Allocator allocator, usize size) {
    size += sizeof(Memory_Arena);
    return arena_allocator_raw(mem_alloc_array(allocator, u8, size), size);
}

static void* null_alloc(Allocator allocator, void* ptr, usize size, usize alignment) {
    return 0;
}

Allocator null_allocator(void) {
    return (Allocator) { 0, null_alloc };
}

static void* pool_alloc(Allocator allocator, void* ptr, usize size, usize alignment) {
    Pool_Allocator* pool = allocator.data;

    if (size) {
        if (ptr) {
            // Try to expand the allocation if we have the space
            int old_size = 0;
            b32 found_allocation = false;
            for (int i = 0; i < pool->bucket_count; ++i) {
                Pool_Bucket* bucket = &pool->buckets[i];

                if (bucket->allocation == ptr) found_allocation = true;
                if (found_allocation && bucket->allocation && bucket->allocation != ptr) break;

                // Set expanded buckets with allocation data
                bucket->allocation = ptr;
                bucket->used = pool->bucket_cap;
                if (old_size + pool->bucket_cap >= (int)size) {
                    bucket->used = (int)size - old_size;
                    return bucket->allocation;
                }
                old_size += bucket->used;
            }

            assert(found_allocation && old_size < size);

            // If we couldnt find enough space just alloc and cpy memory
            void* result = pool_alloc(allocator, 0, size, alignment);
            mem_copy(result, ptr, old_size);
            pool_alloc(allocator, ptr, 0, 0);
            return result;
        }

        // Find enough sequential buckets for this allocation
        int start_index = -1;
        int space_found = 0;
        for (int i = 0; i < pool->bucket_count; ++i) {
            Pool_Bucket bucket = pool->buckets[i];
            if (!bucket.allocation && start_index == -1) {
                start_index = i;
            } else if (bucket.allocation) {
                start_index = -1;
                space_found = 0;
            }

            if (start_index != -1) ++space_found;

            if (space_found * pool->bucket_cap >= (int)size) break;
        }

        //  If we couldn't find enough room then return nullptr
        if (start_index == -1 || space_found * pool->bucket_cap < (int)size) return 0;

        // Find the allocation in the raw memory
        u8* allocation = pool->memory + (start_index * pool->bucket_cap); // @TODO(colby): alignment

        // Update bucket data on pool allocator
        for (int i = 0; i < space_found; ++i) {
            Pool_Bucket* bucket = pool->buckets + i + start_index;
            bucket->allocation = allocation;
            bucket->used = (int)(size > pool->bucket_cap ? pool->bucket_cap : size);
            size -= pool->bucket_cap;
        }

        return allocation;
    }

    // Find al the buckets that have ptr as their allocation and reset their data
    b32 found_allocation = false;
    for (int i = 0; i < pool->bucket_count; ++i) {
        Pool_Bucket* bucket = &pool->buckets[i];
        if (bucket->allocation == ptr) found_allocation = true;

        if (found_allocation && bucket->allocation != ptr) break;

        // Reset bucket data
        if (found_allocation) {
            bucket->allocation = 0;
            bucket->used = 0;
        }
    }

    // Assert if we didn't find the allocation
    assert(found_allocation);
    return 0;
}

Allocator pool_allocator(Allocator allocator, int bucket_count, int bucket_cap) {
    Pool_Bucket* buckets = mem_alloc_array(allocator, Pool_Bucket, bucket_count);
    mem_set(buckets, 0, bucket_count * sizeof(Pool_Bucket));

    u8* allocator_data = mem_alloc(allocator, bucket_count * bucket_cap + sizeof(Pool_Allocator));
    Pool_Allocator* header = (Pool_Allocator*)allocator_data;
    *header = (Pool_Allocator) { buckets, bucket_count, bucket_cap, allocator_data + sizeof(Pool_Allocator) };

    return (Allocator) { header, pool_alloc };
}