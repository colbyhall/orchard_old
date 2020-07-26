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
        mem_copy((u8*)ht->values + value_size * ht->pair_count, value, value_size);
        ht->pair_count += 1;
        rebuild_hash_table(ht);
        return 0;
    }

    mem_copy((u8*)ht->keys + key_size * ht->pair_count, key, key_size);
    mem_copy((u8*)ht->values + value_size * ht->pair_count, value, value_size);

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

    u64 hash = ht->func(key, 0, key_size);
    int index = hash % ht->pair_cap;

    Hash_Bucket* found = ht->bucket_layout[index];
    while (found) {
        u64 my_hash = found->hash;
        if (my_hash == hash) { // @TEMP } && ht->func(key, (u8*)ht->keys + found->index * key_size, key_size)) {
            return (u8*)ht->values + ht->value_size * found->index;
        }
        found = found->next;
    }

    return 0;
}

u64 hash_string(void* a, void* b, int size) {
    assert(size == sizeof(String));

    String* s_a = a;
    String* s_b = b;

    if (b) return string_equal(*s_a, *s_b);

    return fnv1_hash(s_a->data, s_a->len);
}

static void regen_map(Entity_Manager* em, Random_Seed seed) {
    o_log("[Game] Generating map with seed %llu", seed.seed);

    int chunk_cap_sq = (int)sqrt(CHUNK_CAP);
    for (int x = 0; x < chunk_cap_sq; ++x) {
        for (int y = 0; y < chunk_cap_sq; ++y) {
            Chunk* chunk = &em->chunks[x + y * chunk_cap_sq];
            chunk->x = x;
            chunk->y = y;
            chunk->z = 0;

            for (int i = 0; i < CHUNK_SIZE * CHUNK_SIZE; ++i) chunk->tiles[i].type = TT_Open;
        }
    }

    Controller* controller = make_controller(em, v2s((f32)chunk_cap_sq * CHUNK_SIZE / 2.f), MAX_CAMERA_ORTHO_SIZE);
    set_controller(em, controller);
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
    // init_editor(platform);

    // Init Game State
    game_state = mem_alloc_struct(platform->permanent_arena, Game_State);
    game_state->entity_manager = make_entity_manager(platform->permanent_arena);
    if (game_state->is_initialized) {
        o_log("[Game] Found new build and reloaded code");
        return;
    }
    game_state->is_initialized = true;

    regen_map(
        game_state->entity_manager, 
        init_seed((int)g_platform->time_in_seconds())
    );
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
    Rect viewport = { v2z(), v2((f32)g_platform->window_width, (f32)g_platform->window_height) };

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
            imm_begin();
            for (int i = 0; i < em->chunk_count; ++i) {
                Chunk* chunk = &em->chunks[i];
                
                Vector2 min = v2((f32)chunk->x * CHUNK_SIZE, (f32)chunk->y * CHUNK_SIZE);
                Vector2 max = v2_add(min, v2(CHUNK_SIZE, CHUNK_SIZE));
                Rect chunk_rect = { min, max };

                if (!rect_overlaps_rect(viewport_in_world_space, chunk_rect, 0)) continue;

                Vector2 pos = v2((f32)(chunk->x * CHUNK_SIZE), (f32)(chunk->y * CHUNK_SIZE));
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int y = 0; y < CHUNK_SIZE; ++y) {
                        Tile* tile = &chunk->tiles[x + y * CHUNK_SIZE];

                        Vector2 tmin = v2_add(pos, v2((f32)x, (f32)y));
                        Vector2 tmax = v2_add(tmin, v2s(1.f));
                        Rect trect = { tmin, tmax };
                        f32 tile_z = -5.f;

                        if (tile->content == TC_Wall) continue;
                        if (!rect_overlaps_rect(viewport_in_world_space, trect, 0)) continue;

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
            }
            imm_flush();

            Texture2d* walls = find_texture2d(from_cstr("assets/sprites/walls"));
            set_uniform_texture("diffuse", *walls);
            imm_begin();
            for (int i = 0; i < em->chunk_count; ++i) {
                Chunk* chunk = &em->chunks[i];
                
                Vector2 min = v2((f32)chunk->x * CHUNK_SIZE, (f32)chunk->y * CHUNK_SIZE);
                Vector2 max = v2_add(min, v2(CHUNK_SIZE, CHUNK_SIZE));
                Rect chunk_rect = { min, max };

                if (!rect_overlaps_rect(viewport_in_world_space, chunk_rect, 0)) continue;

                Vector2 pos = v2((f32)(chunk->x * CHUNK_SIZE), (f32)(chunk->y * CHUNK_SIZE));
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int y = 0; y < CHUNK_SIZE; ++y) {
                        Tile* tile = &chunk->tiles[x + y * CHUNK_SIZE];

                        Vector2 tmin = v2_add(pos, v2((f32)x, (f32)y));
                        Vector2 tmax = v2_add(tmin, v2s(1.f));
                        Rect trect = { tmin, tmax };
                        f32 wall_z = -4.f;

                        if (tile->content != TC_Wall) continue;
                        if (!rect_overlaps_rect(viewport_in_world_space, trect, 0)) continue;

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
            }
            imm_flush();

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

    {
        set_shader(find_shader(from_cstr("assets/shaders/font")));
        draw_right_handed(viewport);

        Font_Collection* fc = find_font_collection(from_cstr("assets/fonts/Menlo-Regular"));
        Font* font = font_at_size(fc, (int)(25.f * g_platform->dpi_scale));
        set_uniform_texture("atlas", font->atlas);

        char controller_debug_string[256];
        Controller* controller = find_entity_by_id(em, em->controller_id);
        if (controller) {
            Vector2 mouse_location = get_mouse_pos_in_world_space(controller);
            Tile* tile_under_mouse = find_tile_at(em, (int)mouse_location.x, (int)mouse_location.y, 0);

            if (tile_under_mouse) {
                const char* type_string = tile_type_names[tile_under_mouse->type];
                sprintf(
                    controller_debug_string, 
                    "%s (%i, %i)", 
                    type_string, 
                    (int)mouse_location.x, 
                    (int)mouse_location.y
                );
            }
            else sprintf(controller_debug_string, "None");
        }

        f64 precise_dt = g_platform->current_frame_time - g_platform->last_frame_time;

        char buffer[512];
        sprintf(
            buffer, 
            "FPS: %i\nFrame Time: %ims\n  Tick Time: %ims\n  Draw Time: %ims\n  Draw Calls: %i\nVertices Drawn: %i\nTile: %s", 
            game_state->fps,
            (int)(precise_dt * 1000.0),
            (int)(tick_duration * 1000.0),
            (int)(draw_duration * 1000.0),
            draw_state->num_draw_calls,
            draw_state->vertices_drawn,
            controller != 0 ? controller_debug_string : "None"
        );

        imm_begin();
        imm_string(from_cstr(buffer), font, 100000.f, v2(0.f, viewport.max.y - 48.f), -4.f, v4s(1.f));
        imm_flush();
    }

    swap_gl_buffers(g_platform);
}

DLL_EXPORT void shutdown_game(void) {

}