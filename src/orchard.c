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
#include "entity_manager.c"
#include "controller.c"
#include "pawn.c"
#include "gui.c"

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
    mem_set(ht->bucket_layout, 0, sizeof(Hash_Bucket*) * ht->pair_cap);

    for (int i = 0; i < ht->pair_count; ++i) {
        Hash_Bucket* bucket = ht->buckets + i;
        bucket->hash = ht->func((u8*)ht->keys + ht->key_size * i, 0, ht->key_size);
        bucket->index = i;
        bucket->next = 0;

        int index = bucket->hash % ht->pair_cap;
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
        
        if (value) mem_copy((u8*)ht->values + value_size * ht->pair_count, value, value_size);
        else mem_set((u8*)ht->values + value_size * ht->pair_count, 0, value_size);
        
        ht->pair_count += 1;
        rebuild_hash_table(ht);
        return (u8*)ht->values + value_size * (ht->pair_count - 1);
    }

    mem_copy((u8*)ht->keys + key_size * ht->pair_count, key, key_size);
    if (value) mem_copy((u8*)ht->values + value_size * ht->pair_count, value, value_size);
    else mem_set((u8*)ht->values + value_size * ht->pair_count, 0, value_size);

    Hash_Bucket* bucket = ht->buckets + ht->pair_count;
    bucket->hash = ht->func((u8*)ht->keys + key_size * ht->pair_count, 0, key_size);
    bucket->index = ht->pair_count;
    bucket->next = 0;

    int index = bucket->hash % ht->pair_cap;
    Hash_Bucket** last = 0;
    Hash_Bucket** slot = ht->bucket_layout + index;
    while (*slot) {
        last = slot;
        slot = &(*slot)->next;
    }
    *slot = bucket;
    if (last) (*last)->next = *slot;

    ht->pair_count++;

    return (u8*)ht->values + value_size * (ht->pair_count - 1);
}

void reserve_hash_table(Hash_Table* ht, int reserve_amount) {
    assert(reserve_amount > 0);

    int new_cap = ht->pair_cap + reserve_amount;
    while (ht->pair_cap < new_cap) {
        ht->pair_cap += ht->pair_cap >> 1;
        ht->pair_cap++;
    }

    ht->keys          = mem_realloc(ht->allocator, ht->keys, ht->key_size * ht->pair_cap);
    ht->values        = mem_realloc(ht->allocator, ht->values, ht->value_size * ht->pair_cap);
    ht->buckets       = mem_realloc(ht->allocator, ht->buckets, sizeof(Hash_Bucket) * ht->pair_cap);
    ht->bucket_layout = mem_realloc(ht->allocator, ht->bucket_layout, sizeof(Hash_Bucket*) * ht->pair_cap);
    mem_set(ht->bucket_layout, 0, sizeof(Hash_Bucket*) * ht->pair_cap);
}

int _index_hash_table(Hash_Table* ht, void* key, int key_size) {
    assert(key_size == ht->key_size);

    if (!ht->pair_count) return -1;

    u64 hash = ht->func(key, 0, key_size);
    int index = hash % ht->pair_cap;

    Hash_Bucket* found = ht->bucket_layout[index];
    while (found) {
        u64 my_hash = found->hash;
        if (my_hash == hash) { // @TEMP } && ht->func(key, (u8*)ht->keys + found->index * key_size, key_size)) {
            return found->index;
        }
        found = found->next;
    }    

    return -1;
}

void* _find_hash_table(Hash_Table* ht, void* key, int key_size) {
    assert(key_size == ht->key_size);

    if (!ht->pair_count) return 0;

    int found_index = _index_hash_table(ht, key, key_size);
    if (found_index == -1) return 0;

    return (u8*)ht->values + ht->value_size * found_index;
}

b32 _remove_hash_table(Hash_Table* ht, void* key, int key_size) {
    assert(key_size == ht->key_size);

    int found_index = _index_hash_table(ht, key, key_size);
    if (found_index == -1) return false;

    ht->pair_count -= 1;

    if (found_index != ht->pair_count) {
        mem_move(
            (u8*)ht->keys + ht->key_size * found_index, 
            (u8*)ht->keys + ht->key_size * (found_index + 1), 
            (ht->pair_count - found_index) * ht->key_size
        );
        mem_move(
            (u8*)ht->values + ht->value_size * found_index, 
            (u8*)ht->values + ht->value_size * (found_index + 1), 
            (ht->pair_count - found_index) * ht->value_size
        );
    }

    rebuild_hash_table(ht);

    return true;
}

u64 hash_string(void* a, void* b, int size) {
    assert(size == sizeof(String));

    String* s_a = a;
    String* s_b = b;

    if (b) return string_equal(*s_a, *s_b);

    return fnv1_hash(s_a->data, s_a->len);
}

Builder make_builder(Allocator allocator, int reserve) {
    Builder result = { .allocator = allocator };
    reserve_builder(&result, reserve);
    return result;
}

void reserve_builder(Builder* builder, int amount) {
    int new_cap = builder->cap + amount;
    while (builder->cap < new_cap) {
        builder->cap += builder->cap >> 1;
        builder->cap++;
    }

    builder->data = mem_realloc(builder->allocator, builder->data, builder->cap);
}

int printf_builder(Builder* builder, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = vprintf_builder(builder, fmt, args);
    va_end(args);
    return result;
}

int vprintf_builder(Builder* builder, const char* fmt, va_list args) {
    va_list copy_args = 0;
    va_copy(copy_args, args);
    int count = vsnprintf(0, 0, fmt, copy_args);
    va_end(copy_args);

    int needs = (builder->count + count) - builder->cap;
    if (needs) reserve_builder(builder, needs);

    int result = vsprintf((char*)builder->data + builder->count, fmt, args);
    builder->count += result;
    return result;
}

void bytes_to_string_builder(Builder* builder, usize num_bytes) {
    // This is bad but who cares
    if (num_bytes / 1024 > 0) {
        usize num_kb = num_bytes / 1024;

        if (num_kb / 1024 > 0) {
            usize num_mb = num_kb / 1024;

            if (num_mb / 1024 > 0) {
                usize num_gb = num_mb / 1024;
                usize mb_left = num_mb - num_gb * 1024;

                printf_builder(builder, "%.3fgb", mb_left / 1024.0 + num_gb);
            } else {
                usize kb_left = num_kb - num_mb * 1024;

                printf_builder(builder, "%.3fmb", kb_left / 1024.0 + num_mb);
            }
        } else {
            usize bytes_left = num_bytes - num_kb * 1024;

            printf_builder(builder, "%.3fkb", bytes_left / 1024.0 + num_kb);
        }
    } else {
        printf_builder(builder, "%llub", num_bytes);
    }
}

Float_Heap make_float_heap(Allocator allocator, int reserve) {
    Float_Heap result = { .allocator = allocator };
    reserve_float_heap(&result, reserve + 1);
    result.buckets[0].value = 0.f;
    return result;
}

void reserve_float_heap(Float_Heap* heap, int amount) {
    assert(amount > 0);

    int new_cap = heap->cap + amount;
    while (heap->cap < new_cap) {
        heap->cap += heap->cap >> 1;
        heap->cap++;
    }

    heap->buckets = mem_realloc(heap->allocator, heap->buckets, sizeof(Float_Heap_Bucket) * heap->cap);
}

static void float_heap_minify(Float_Heap* heap, int index) {
    if (!heap_is_leaf(index, heap->count)) {
        if (heap->buckets[index].value > heap->buckets[heap_left_child(index)].value || heap->buckets[index].value > heap->buckets[heap_right_child(index)].value) {

            if (heap->buckets[heap_left_child(index)].value < heap->buckets[heap_right_child(index)].value) {
                swap(heap->buckets[index], heap->buckets[heap_left_child(index)], Float_Heap_Bucket);
                float_heap_minify(heap, heap_left_child(index));
            } else {
                swap(heap->buckets[index], heap->buckets[heap_right_child(index)], Float_Heap_Bucket);
                float_heap_minify(heap, heap_right_child(index));
            }
        }
    }
}

void push_min_float_heap(Float_Heap* heap, f32 value, int index) {
    if (heap->count + 1 >= heap->cap) reserve_float_heap(heap, 1);

    heap->buckets[++heap->count] = (Float_Heap_Bucket) { value, index };

    index = heap->count; // this is kind of ew but oh well

    while (heap->buckets[index].value < heap->buckets[heap_parent(index)].value && heap_is_leaf(index, heap->count)) {
        swap(heap->buckets[index], heap->buckets[heap_parent(index)], Float_Heap_Bucket);
        index = heap_parent(index);
    }

    for (int i = heap->count / 2; i >= 1; --i) {
        float_heap_minify(heap, i);
    }
}

void push_max_float_heap(Float_Heap* heap, f32 value, int index) {
    if (heap->count + 1 >= heap->cap) reserve_float_heap(heap, 1);

    heap->buckets[++heap->count] = (Float_Heap_Bucket) { value, index };
    
    float_heap_minify(heap, 1);

    index = heap->count; // this is kind of ew but oh well

    while (heap->buckets[index].value > heap->buckets[heap_parent(index)].value) {
        swap(heap->buckets[index], heap->buckets[heap_parent(index)], Float_Heap_Bucket);
        index = heap_parent(index);
    }
}

int pop_min_float_heap(Float_Heap* heap) {
    int result = heap->buckets[1].index;
    heap->buckets[1] = heap->buckets[heap->count--];

    for (int i = heap->count / 2; i >= 1; --i) {
        float_heap_minify(heap, i);
    }

    return result;
}

static void float_heap_maxify(Float_Heap* heap, int index) {
    if (!heap_is_leaf(index, heap->count)) {
        if (heap->buckets[index].value < heap->buckets[heap_left_child(index)].value || heap->buckets[index].value < heap->buckets[heap_right_child(index)].value) {

            if (heap->buckets[heap_left_child(index)].value > heap->buckets[heap_right_child(index)].value) {
                swap(heap->buckets[index], heap->buckets[heap_left_child(index)], Float_Heap_Bucket);
                float_heap_minify(heap, heap_left_child(index));
            } else {
                swap(heap->buckets[index], heap->buckets[heap_right_child(index)], Float_Heap_Bucket);
                float_heap_minify(heap, heap_right_child(index));
            }
        }
    }
}

int pop_max_float_heap(Float_Heap* heap) {
    int result = heap->buckets[1].index;
    heap->buckets[1] = heap->buckets[heap->count--];

    for (int i = heap->count / 2; i >= 1; --i) {
        float_heap_maxify(heap, i);
    }

    return result;
}

#define PIXELS_PER_METER 32

typedef struct Game_State {
    Entity_Manager* entity_manager;

    int fps;
    int frame_count;
    f32 frame_accum;

    b32 is_initialized;
} Game_State;

static Game_State* game_state;

DLL_EXPORT void init_game(Platform* platform) {
    g_platform = platform;

    init_logger(platform);
    init_opengl(platform);
    init_asset_manager(platform);
    init_draw(platform);
    init_gui(platform);
    init_debug(platform);

    // Init Game State
    game_state = mem_alloc_struct(platform->permanent_arena, Game_State);
    game_state->entity_manager = make_entity_manager(platform->permanent_arena);
    if (game_state->is_initialized) {
        o_log("[Game] Found new build and reloaded code");
        return;
    }
    game_state->is_initialized = true;

    Controller* controller = make_controller(game_state->entity_manager, v2s((WORLD_SIZE * CHUNK_SIZE) / 2.f), MAX_CAMERA_ORTHO_SIZE);
    set_controller(game_state->entity_manager, controller);
}

DLL_EXPORT void tick_game(f32 dt) {
    game_state->frame_accum += dt;
    if (game_state->frame_accum >= 1.f) {
        game_state->fps = game_state->frame_count;
        game_state->frame_accum = 0.f;
        game_state->frame_count = 0;
    }
    game_state->frame_count += 1;

    // Do input handling
    for (int i = 0; i < g_platform->num_events; ++i) {
        OS_Event event = g_platform->events[i];

        switch (event.type) {
        case OET_Window_Resized:
            resize_draw(g_platform->window_width, g_platform->window_height);
            break;
        }
    }

    Entity_Manager* em = game_state->entity_manager;
    Rect viewport = viewport_rect();

    f64 before_tick = g_platform->time_in_seconds();
    // Tick the game state
    {
        for (entity_iterator(em)) {
            Entity* entity = entity_from_iterator(iter);

            switch (entity->type) {
#define TICK_ENTITIES(type, tick, draw) \
            case type: tick(em, entity, dt); break;
            ENTITY_FUNCTIONS(TICK_ENTITIES);
#undef TICK_ENTITIES
            };
        }
    }
    f64 tick_duration = g_platform->time_in_seconds() - before_tick;

    // Draw the game state
    f64 before_draw = g_platform->time_in_seconds();
    {
        glViewport(0, 0, g_platform->window_width, g_platform->window_height);
        clear_framebuffer(v3s(0.01f));

        draw_state->num_draw_calls = 0;
        draw_state->vertices_drawn = 0;
        draw_state->draw_call_duration = 0.0;

        set_shader(find_shader(from_cstr("assets/shaders/basic2d")));
        draw_right_handed(viewport);
        set_uniform_texture("diffuse", *find_texture2d(from_cstr("assets/textures/background")));
        imm_begin();
        imm_textured_rect(viewport, -10.f, v2z(), v2s(1.f), v4s(1.f));
        imm_flush();

        Controller* controller = find_entity_by_id(em, em->controller_id);
        if (controller) {
            // Draw the tilemap
            set_shader(find_shader(from_cstr("assets/shaders/basic2d")));
            Texture2d* terrain = find_texture2d(from_cstr("assets/sprites/terrain_map"));
            set_uniform_texture("diffuse", *terrain);
            draw_from(controller->location, controller->current_ortho_size);

            Rect viewport_in_world_space = get_viewport_in_world_space(controller);
            
            // Draw tile in a single batch
            for (int i = 0; i < WORLD_SIZE * WORLD_SIZE; ++i) {
                Chunk* chunk = &em->chunks[i];
                
                Vector2 min = v2((f32)chunk->x * CHUNK_SIZE, (f32)chunk->y * CHUNK_SIZE);
                Vector2 max = v2_add(min, v2(CHUNK_SIZE, CHUNK_SIZE));
                Rect chunk_rect = { min, max };

                if (!rect_overlaps_rect(viewport_in_world_space, chunk_rect, 0)) continue;

                imm_begin();
                Vector2 pos = v2((f32)(chunk->x * CHUNK_SIZE), (f32)(chunk->y * CHUNK_SIZE));
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int y = 0; y < CHUNK_SIZE; ++y) {
                        Tile* tile = &chunk->tiles[x + y * CHUNK_SIZE];

                        Vector2 tmin = v2_add(pos, v2((f32)x, (f32)y));
                        Vector2 tmax = v2_add(tmin, v2s(1.f));
                        Rect trect = { tmin, tmax };
                        f32 tile_z = -5.f;

                        // if (rect_overlaps_rect(viewport_in_world_space, trect)) continue;
                        if (tile->content == TC_Wall) continue;

                        int sprites_per_row = terrain->width / PIXELS_PER_METER;
                        int sprite_y = tile->type / sprites_per_row;
                        int sprite_x = tile->type - sprite_y * sprites_per_row;

                        f32 map_width = (f32)terrain->width;
                        f32 texel_size = 1.f / map_width;
                        Vector2 uv_offset = v2s(texel_size / 4.f);

                        Vector2 uv0 = v2_add(v2(sprite_x * PIXELS_PER_METER / map_width, sprite_y * PIXELS_PER_METER / map_width), uv_offset);
                        Vector2 uv1 = v2_sub(v2_add(uv0, v2s(texel_size * PIXELS_PER_METER)), uv_offset);


                        switch (controller->mode) {
                        case CM_Normal: 
                        case CM_Set_Wall:
                            if (tile->type != TT_Open) imm_textured_rect(trect, tile_z, uv0, uv1, v4s(1.f));
                            break;
                        case CM_Set_Tile:
                            imm_textured_rect(trect, tile_z, uv0, uv1, v4s(1.f)); 
                            break;
                        default: invalid_code_path;
                        }

                    }
                }
                imm_flush();
            }

            Texture2d* walls = find_texture2d(from_cstr("assets/sprites/walls"));
            set_uniform_texture("diffuse", *walls);
            for (int i = 0; i < WORLD_SIZE * WORLD_SIZE; ++i) {
                Chunk* chunk = &em->chunks[i];
                
                Vector2 min = v2((f32)chunk->x * CHUNK_SIZE, (f32)chunk->y * CHUNK_SIZE);
                Vector2 max = v2_add(min, v2(CHUNK_SIZE, CHUNK_SIZE));
                Rect chunk_rect = { min, max };

                if (!rect_overlaps_rect(viewport_in_world_space, chunk_rect, 0)) continue;

                imm_begin();
                Vector2 pos = v2((f32)(chunk->x * CHUNK_SIZE), (f32)(chunk->y * CHUNK_SIZE));
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int y = 0; y < CHUNK_SIZE; ++y) {
                        Tile* tile = &chunk->tiles[x + y * CHUNK_SIZE];

                        Vector2 tmin = v2_add(pos, v2((f32)x, (f32)y));
                        Vector2 tmax = v2_add(tmin, v2s(1.f));
                        Rect trect = { tmin, tmax };
                        f32 wall_z = -4.f;

                        if (tile->content != TC_Wall) continue;
                        // if (!rect_overlaps_rect(viewport_in_world_space, trect, 0)) continue;

                        int sprite_y = 0;
                        int sprite_x = tile->wall.visual;

                        f32 map_width = (f32)walls->width;
                        f32 texel_size = 1.f / map_width;
                        Vector2 uv_offset = v2s(texel_size / 4.f);

                        Vector2 uv0 = v2_add(v2(sprite_x * PIXELS_PER_METER / map_width, sprite_y * PIXELS_PER_METER / map_width), uv_offset);
                        Vector2 uv1 = v2_sub(v2_add(uv0, v2s(texel_size * PIXELS_PER_METER)), uv_offset);

                        imm_textured_rect(trect, wall_z, uv0, uv1, v4s(1.f)); 
                    }
                }
                imm_flush();
            }

            if (controller->selection.valid) {
                Rect selection = rect_from_points(controller->selection.start, controller->selection.current);
                selection.min = v2_floor(selection.min);
                selection.max = v2_add(v2_floor(selection.max), v2s(1.f));
                Vector4 selection_color = color_from_hex(0x5ecf4466);
                f32 selection_z = -2.f;

                switch (controller->mode) {
                case CM_Set_Tile: {
                    imm_begin();
                    imm_rect(selection, selection_z, selection_color);
                    imm_flush();
                } break;
                case CM_Set_Wall: {
                    imm_begin();
                    imm_border_rect(selection, selection_z, 1.f, selection_color);
                    imm_flush();
                } break;
                }
            }

            for (entity_iterator(em)) {
                Entity* entity = entity_from_iterator(iter);

                switch (entity->type) {
#define DRAW_ENTITIES(type, tick, draw) \
                case type: draw(em, entity); break;
                ENTITY_FUNCTIONS(DRAW_ENTITIES);
#undef DRAW_ENTITIES
                };
            }
        }
    }
    f64 draw_duration = g_platform->time_in_seconds() - before_draw;

    do_gui(dt) {
        gui_row_layout_rect(viewport, true) {
            gui_label_printf("FPS: %i", game_state->fps);

            gui_label_printf(" ");

            f64 precise_dt = g_platform->current_frame_time - g_platform->last_frame_time;
            gui_label_printf("Frame Time: %.3fms", precise_dt * 1000.0);
            gui_label_printf("    Tick Time: %.3fms", tick_duration * 1000.0);
            gui_label_printf("    Draw Time: %.3fms", draw_duration * 1000.0);
            gui_label_printf("        Draw Calls: %i", draw_state->num_draw_calls);
            gui_label_printf("        Vertices Drawn: %i", draw_state->vertices_drawn);
            gui_label_printf("        GPU Time: %.3fms", draw_state->draw_call_duration * 1000.0);
            gui_label_printf("    GUI Time: %.3fms", gui_state->last_duration * 1000.0);

            gui_label_printf(" ");

            Memory_Arena* permanent_arena = g_platform->permanent_arena.data;
            Builder builder = make_builder(g_platform->frame_arena, 512);
            printf_builder(&builder, "Permanent Arena: ");
            bytes_to_string_builder(&builder, permanent_arena->used);
            printf_builder(&builder, "/");
            bytes_to_string_builder(&builder, permanent_arena->total);
            gui_label(builder_to_string(builder));

            gui_label_printf(" ");

            gui_label_printf("Build: %s", DEBUG_BUILD ? "Debug" : "Release");

            gui_label_printf(" ");

            do_debug_ui();
        }
    }

    swap_gl_buffers(g_platform);
}

DLL_EXPORT void shutdown_game(void) {

}