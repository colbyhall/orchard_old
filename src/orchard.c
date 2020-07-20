#include "platform.h"

// Include the OS stuff before any implementations
#if PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define WIN32_MEAN_AND_LEAN
#define NOMINMAX
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#undef WIN32_MEAN_AND_LEAN
#undef NOMINMAX
#undef far
#undef near
#undef FAR
#undef NEAR
#else
#error Platform not yet implemented.
#endif

#include "memory.c"
#include "string.c"
#include "math.c"
#include "debug.c"
#include "opengl.c"
#include "draw.c"
#include "asset.c"

Platform* g_platform = 0;

Hash_Table _make_hash_table(int key_size, int value_size, Hash_Table_Func* func, Allocator allocator) {
    assert(func);
    return (Hash_Table) { 
        .key_size = key_size, 
        .value_size = value_size, 
        .func = func, 
        .allocator = allocator
    };
}

static void rebuild_hash_table(Hash_Table* ht) {
    for (int i = 0; i < ht->pair_count; ++i) {
        Hash_Bucket* const bucket = ht->buckets + i;
        bucket->hash = ht->func((u8*)ht->keys + ht->key_size * i, 0, ht->key_size);
        bucket->index = i;
        bucket->next = 0;

        const int index = bucket->hash % ht->pair_cap;
        Hash_Bucket** last = 0;
        Hash_Bucket** slot = ht->bucket_layout + index;
        while (*slot) {
            last = slot;
            slot = &(*slot)->next;
        }
        *slot = bucket;
        if (last) (*last)->next = *slot;
    }
}

void* _push_hash_table(Hash_Table* ht, void* key, int key_size, void* value, int value_size) {
    assert(key_size == ht->key_size && value_size == ht->value_size);

    void* found = _find_hash_table(ht, key, key_size);
    if (found) return found;

    if (ht->pair_count == ht->pair_cap) {
        reserve_hash_table(ht, 1);
        mem_copy((u8*)ht->keys + key_size * ht->pair_count, key, key_size);
        mem_copy((u8*)ht->values + value_size * ht->pair_count, value, value_size);
        ht->pair_count += 1;
        rebuild_hash_table(ht);
        return 0;
    }

    mem_copy((u8*)ht->keys + key_size * ht->pair_count, key, key_size);
    mem_copy((u8*)ht->values + value_size * ht->pair_count, value, value_size);

    Hash_Bucket* const bucket = ht->buckets + ht->pair_count;
    bucket->hash = ht->func((u8*)ht->keys + key_size * ht->pair_count, 0, key_size);
    bucket->index = ht->pair_count;
    bucket->next = 0;

    const int index = bucket->hash % ht->pair_cap;
    Hash_Bucket** last = 0;
    Hash_Bucket** slot = ht->bucket_layout + index;
    while (*slot) {
        last = slot;
        slot = &(*slot)->next;
    }
    *slot = bucket;
    if (last) (*last)->next = *slot;

    ht->pair_count++;

    return 0;
}

void reserve_hash_table(Hash_Table* ht, int reserve_amount) {
    assert(reserve_amount > 0);

    ht->pair_cap += reserve_amount; // @TODO(colby): Small alg for best reserve amount
    ht->keys          = mem_realloc(ht->allocator, ht->keys, ht->key_size * ht->pair_cap);
    ht->values        = mem_realloc(ht->allocator, ht->values, ht->value_size * ht->pair_cap);
    ht->buckets       = mem_realloc(ht->allocator, ht->buckets, sizeof(Hash_Bucket) * ht->pair_cap);
    ht->bucket_layout = mem_realloc(ht->allocator, ht->bucket_layout, sizeof(Hash_Bucket*) * ht->pair_cap);
    mem_set(ht->bucket_layout, 0, sizeof(Hash_Bucket*) * ht->pair_cap);
}

void* _find_hash_table(Hash_Table* ht, void* key, int key_size) {
    assert(key_size == ht->key_size);

    if (!ht->pair_count) return 0;

    const u64 hash = ht->func(key, 0, key_size);
    const int index = hash % ht->pair_cap;

    Hash_Bucket* found = ht->bucket_layout[index];
    while (found) {
        const u64 my_hash = found->hash;
        if (my_hash == hash) { // @TEMP } && ht->func(key, (u8*)ht->keys + found->index * key_size, key_size)) {
            return (u8*)ht->values + ht->value_size * found->index;
        }
        found = found->next;
    }

    return 0;
}

u64 hash_string(void* a, void* b, int size) {
    assert(size == sizeof(String));

    String* const s_a = a;
    String* const s_b = b;

    if (b) return string_equal(*s_a, *s_b);

    return fnv1_hash(s_a->data, s_a->len);
}

typedef struct Game_State {
    b32 is_initialized;
} Game_State;

static Game_State* g_game_state;

DLL_EXPORT void init_game(Platform* platform) {
    g_platform = platform;

    init_logger(platform);
    init_opengl(platform);
    init_asset_manager(platform);
    init_draw(platform);
    // init_editor(platform);

    // Init Game State
    g_game_state = mem_alloc_struct(platform->permanent_arena, Game_State);
    if (g_game_state->is_initialized) return;
    g_game_state->is_initialized = true;
}

DLL_EXPORT void tick_game(f32 dt) {
    for (int i = 0; i < g_platform->num_events; ++i) {
        const OS_Event event = g_platform->events[i];

        switch (event.type) {
        case OET_Window_Resized:
            resize_draw(g_platform->window_width, g_platform->window_height);
            break;
        }
    }

    swap_gl_buffers(g_platform);
}

DLL_EXPORT void shutdown_game(void) {

}