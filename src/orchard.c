#include "platform.h"
#include "orchard.h"

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

static Game_State* g_game_state;

static Entity_Manager make_entity_manager(Allocator allocator) {
    Entity_Manager result = { .tile_memory = allocator, .chunk_count = CHUNK_CAP, };

    for (int i = 0; i < result.chunk_count; ++i) {
        Chunk* const chunk = &result.chunks[i];

        chunk->tiles = mem_alloc_array(allocator, Tile, CHUNK_SIZE * CHUNK_SIZE);
    }

    return result;
}

static void regen_map(Entity_Manager* em, Random_Seed seed) {
    // *em = (Entity_Manager) { 0 };

    o_log("[Game] Generating map with seed %llu", seed.seed);

    const int chunk_cap_sq = (int)sqrt(CHUNK_CAP);
    for (int x = 0; x < chunk_cap_sq; ++x) {
        for (int y = 0; y < chunk_cap_sq; ++y) {
            Chunk* const chunk = &em->chunks[x + y * chunk_cap_sq];
            chunk->x = x;
            chunk->y = y;
            chunk->z = 0;

            for (int jx = 0; jx < CHUNK_SIZE; ++jx) {
                for (int jy = 0; jy < CHUNK_SIZE; ++jy) {
                    Tile* const tile = &chunk->tiles[jx + jy * CHUNK_SIZE];

                    const f32 final_x = (f32)(x * CHUNK_SIZE) + (f32)jx;
                    const f32 final_y = (f32)(y * CHUNK_SIZE) + (f32)jy;
                    const f32 noise   = perlin_get_2d(seed, final_x, final_y, 0.1f, 4);

                    tile->type = (noise > 0.65f) + 1;
                }
            }
        }
    }
}

Vector2 mouse_pos_in_world_space(void) {
    const f32 ratio = (g_game_state->current_ortho_size * 2.f) / (f32)g_platform->window_height;
    const int adjusted_x = g_platform->input.state.mouse_x - g_platform->window_width / 2;
    const int adjusted_y = g_platform->input.state.mouse_y - g_platform->window_height / 2;
    return v2_add(v2_mul(v2((f32)adjusted_x, (f32)adjusted_y), v2s(ratio)), g_game_state->cam_pos);
}

DLL_EXPORT void init_game(Platform* platform) {
    g_platform = platform;

    init_logger(platform);
    init_opengl(platform);
    init_asset_manager(platform);
    init_draw(platform);
    // init_editor(platform);

    // Init Game State
    g_game_state = mem_alloc_struct(platform->permanent_arena, Game_State);
    g_game_state->entity_manager = make_entity_manager(platform->permanent_arena);
    if (g_game_state->is_initialized) return;
    g_game_state->is_initialized = true;

    g_game_state->target_ortho_size = 5.f;
    g_game_state->current_ortho_size = g_game_state->target_ortho_size;

    regen_map(&g_game_state->entity_manager, (Random_Seed) { g_platform->cycles() });
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

    const f32 mouse_wheel_delta = (f32)g_platform->input.state.mouse_wheel_delta / 50.f;
    g_game_state->target_ortho_size -= mouse_wheel_delta;
    g_game_state->target_ortho_size = CLAMP(g_game_state->target_ortho_size, 5.f, 50.f);

    const Vector2 old_mouse_pos_in_world = mouse_pos_in_world_space();

    const f32 old_ortho_size = g_game_state->current_ortho_size;
    g_game_state->current_ortho_size = lerpf(g_game_state->current_ortho_size, g_game_state->target_ortho_size, dt * 5.f);
    const f32 delta_ortho_size = g_game_state->current_ortho_size - old_ortho_size;

    const Vector2 delta_mouse_pos_in_world = v2_sub(old_mouse_pos_in_world, mouse_pos_in_world_space());
    if (delta_ortho_size != 0.f) g_game_state->cam_pos = v2_add(g_game_state->cam_pos, delta_mouse_pos_in_world);

    const f32 ratio = (g_game_state->current_ortho_size * 2.f) / (f32)g_platform->window_height;

    if (g_platform->input.state.mouse_buttons_down[MOUSE_MIDDLE]) {
        const Vector2 mouse_delta = v2((f32)g_platform->input.state.mouse_dx, (f32)g_platform->input.state.mouse_dy);
        const f32 speed = ratio;

        g_game_state->cam_pos = v2_add(g_game_state->cam_pos, v2_mul(v2_inverse(mouse_delta), v2s(speed)));
    }

    const f64 before_draw = g_platform->time_in_seconds();
    draw_game(g_game_state);
    const f64 draw_duration = g_platform->time_in_seconds() - before_draw;

    {
        const Rect viewport = { v2z(), v2((f32)g_platform->window_width, (f32)g_platform->window_height) };
        set_shader(find_shader(from_cstr("assets/shaders/font")));
        draw_right_handed(viewport);

        Font_Collection* const fc = find_font_collection(from_cstr("assets/fonts/Menlo-Regular"));
        Font* const font = font_at_size(fc, 48);
        set_uniform_texture("atlas", font->atlas);

        const f64 precise_dt = g_platform->current_frame_time - g_platform->last_frame_time;

        char buffer[512];
        sprintf(
            buffer, 
            "Frame Time: %ims\nDraw Time: %ims\nDraw Calls: %i", 
            (int)(precise_dt * 1000.0),
            (int)(draw_duration * 1000.0),
            draw_state->num_draw_calls
        );

        imm_begin();
        imm_string(from_cstr(buffer), font, 100000.f, v2(0.f, viewport.max.y - 48.f), -4.f, v4s(1.f));
        imm_flush();
    }

    swap_gl_buffers(g_platform);
}

DLL_EXPORT void shutdown_game(void) {

}