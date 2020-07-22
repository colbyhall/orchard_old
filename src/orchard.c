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

typedef enum Tile_Type {
    TT_Air = 0,
    TT_Grass,
    TT_Dirt,
    TT_Gravel,
    TT_Stone,
    TT_Sand,
    TT_Water,

    TT_Count,
} Tile_Type;

typedef struct Tile {
    Tile_Type type;
} Tile;

#define CHUNK_SIZE 16
typedef struct Chunk {
    int x, y, z;
    Tile* tiles; // Allocated elsewhere because we scan chunks for their pos in a list
} Chunk;

typedef u32 Entity_Id; // Invalid Entity_Id is 0

typedef enum Entity_Type {
    ET_Camera,
    ET_Pawn,
} Entity_Type;

#define ENTITY_STRUCT(entry) \
entry(Entity_Id, id) \
entry(Entity_Type, type) \
entry(Vector2, location) \
entry(f32, rotation) \
entry(Rect, bounds) \
entry(void*, derived)

#define DEFINE_ENTITY_STRUCT(t, n) t n;

typedef struct Entity {
    ENTITY_STRUCT(DEFINE_ENTITY_STRUCT)
} Entity;

#define DEFINE_CHILD_ENTITY union { \
    struct { \
        ENTITY_STRUCT(DEFINE_ENTITY_STRUCT) \
    }; \
    Entity base; \
}

#define CHUNK_CAP 256
#define ENTITY_CAP (CHUNK_SIZE * CHUNK_SIZE * CHUNK_CAP)
typedef struct Entity_Manager {
    int chunk_count;
    Chunk chunks[CHUNK_CAP];
    Allocator tile_memory;

    int entity_count;
    Entity* entities[ENTITY_CAP];
    Entity_Id last_entity_id;

    Entity_Id camera_id;

    Allocator entity_memory;
} Entity_Manager;

typedef struct Entity_Iterator {
    Entity_Manager* manager;
    int found_entity_count;
    int index;
} Entity_Iterator;

static Entity_Iterator make_entity_iterator(Entity_Manager* manager) {
    for (int i = 0; i < ENTITY_CAP; ++i) {
        Entity* const e = manager->entities[i];

        if (!e) continue;

        return (Entity_Iterator) { manager, 0, i };
    }

    return (Entity_Iterator) { 0 };
}

static b32 can_step_entity_iterator(Entity_Iterator iter) {
    return (
        iter.manager != 0 && 
        iter.index < iter.manager->entity_count && 
        iter.found_entity_count < iter.manager->entity_count
    );
}

static void step_entity_iterator(Entity_Iterator* iter) {
    iter->found_entity_count++;
    if (iter->found_entity_count == iter->manager->entity_count) return;

    for (int i = iter->index + 1; i < ENTITY_CAP; ++i) {
        Entity* const e = iter->manager->entities[i];

        if (!e) continue;

        iter->index = i;
        break;
    }
}

#define entity_iterator(em) Entity_Iterator iter = make_entity_iterator(em); can_step_entity_iterator(iter); step_entity_iterator(&iter)

static Entity* entity_from_iterator(Entity_Iterator iter) {
    return iter.manager->entities[iter.index];
}

static void* find_entity(Entity_Manager* em, Entity_Id id) {
    for (entity_iterator(em)) {
        Entity* const entity = entity_from_iterator(iter);

        if (entity->id == id) return entity->derived;
    }

    return 0;
}

static Entity_Manager* make_entity_manager(Allocator allocator) {
    Entity_Manager* const result = mem_alloc_struct(allocator, Entity_Manager);

    result->tile_memory = allocator;
    result->chunk_count = CHUNK_CAP;

    for (int i = 0; i < result->chunk_count; ++i) {
        Chunk* const chunk = &result->chunks[i];

        chunk->tiles = mem_alloc_array(allocator, Tile, CHUNK_SIZE * CHUNK_SIZE);
    }

    result->entity_memory = pool_allocator(allocator, ENTITY_CAP + ENTITY_CAP / 2, 256);

    return result;
}

static void* _make_entity(Entity_Manager* em, int size, Entity_Type type) {
    assert(sizeof(Entity) < size);
    assert(em->entity_count < ENTITY_CAP);

    Entity* result = mem_alloc(em->entity_memory, size);
    *result = (Entity) { 0 };

    result->derived = result;
    result->id = ++em->last_entity_id;
    result->type = type;

    for (int i = 0; i < ENTITY_CAP; ++i) {
        Entity** const entity = &em->entities[i];
        if (!(*entity)) {
            *entity = result;
            em->entity_count++;
            break;
        }
    }

    return result;
}
#define make_entity(em, type) _make_entity(em, sizeof(type), ET_ ## type)

typedef struct Camera {
    DEFINE_CHILD_ENTITY;
    f32 current_ortho_size;
    f32 target_ortho_size;
} Camera;

static Vector2 get_mouse_pos_in_world_space(Camera* camera) {
    const f32 ratio = (camera->current_ortho_size * 2.f) / (f32)g_platform->window_height;
    const int adjusted_x = g_platform->input.state.mouse_x - g_platform->window_width / 2;
    const int adjusted_y = g_platform->input.state.mouse_y - g_platform->window_height / 2;
    return v2_add(v2_mul(v2((f32)adjusted_x, (f32)adjusted_y), v2s(ratio)), camera->location);
}

static void set_camera(Entity_Manager* em, Camera* camera) {
    if (camera) {
        em->camera_id = camera->id;
        return;
    }
    em->camera_id = 0;
}

static Rect get_viewport_in_world_space(Camera* camera) {
    if (!camera) return rect_from_raw(0.f, 0.f, 0.f, 0.f);
    
    const f32 ratio = (camera->current_ortho_size * 2.f) / (f32)g_platform->window_height;
    const f32 adjusted_width = (f32)g_platform->window_width * ratio;
    const f32 adjusted_height = (f32)g_platform->window_height * ratio;

    return rect_from_pos(camera->location, v2(adjusted_width, adjusted_height));
}

static void tick_camera(Entity_Manager* em, Entity* entity, f32 dt) {
    assert(entity->type == ET_Camera);
    Camera* const camera = entity->derived;

    const f32 mouse_wheel_delta = (f32)g_platform->input.state.mouse_wheel_delta / 50.f;
    camera->target_ortho_size -= mouse_wheel_delta;
    camera->target_ortho_size = CLAMP(camera->target_ortho_size, 5.f, 50.f);

    const Vector2 old_mouse_pos_in_world = get_mouse_pos_in_world_space(camera);

    const f32 old_ortho_size = camera->current_ortho_size;
    camera->current_ortho_size = lerpf(camera->current_ortho_size, camera->target_ortho_size, dt * 5.f);
    const f32 delta_ortho_size = camera->current_ortho_size - old_ortho_size;

    const Vector2 delta_mouse_pos_in_world = v2_sub(old_mouse_pos_in_world, get_mouse_pos_in_world_space(camera));
    if (delta_ortho_size != 0.f) camera->location = v2_add(camera->location, delta_mouse_pos_in_world);

    const f32 ratio = (camera->current_ortho_size * 2.f) / (f32)g_platform->window_height;

    if (g_platform->input.state.mouse_buttons_down[MOUSE_MIDDLE]) {
        const Vector2 mouse_delta = v2((f32)g_platform->input.state.mouse_dx, (f32)g_platform->input.state.mouse_dy);
        const f32 speed = ratio;

        camera->location = v2_add(camera->location, v2_mul(v2_inverse(mouse_delta), v2s(speed)));
    }
}

typedef struct Pawn {
    DEFINE_CHILD_ENTITY;
    Vector2 target_location;
    f32 idle_time;
} Pawn;

static void tick_pawn(Entity_Manager* em, Entity* entity, f32 dt) {
    assert(entity->type == ET_Pawn);

    const f32 max_speed = 3.f;
    Pawn* const pawn = entity->derived;

    Random_Seed seed = { g_platform->cycles() };

    const Vector2 to_point = v2_sub(pawn->target_location, pawn->location);
    const f32 to_point_len = v2_len(to_point);
    if (to_point_len < 0.1f) {
        pawn->idle_time += dt;
        if (pawn->idle_time >= 3.f) {
            const f32 x = get_random_f32(seed, 0.f, 500.f);
            iterate_seed(&seed);
            const f32 y = get_random_f32(seed, 0.f, 500.f);
            pawn->target_location = v2(x, y);
            pawn->idle_time = 0.f;
        }
    } else {
        const Vector2 to_point_norm = v2_div(to_point, v2s(to_point_len));
        pawn->location = v2_add(pawn->location, v2_mul(to_point_norm, v2s(max_speed * dt)));
    }
}

static void draw_pawn(Entity_Manager* em, Entity* entity) {
    set_shader(find_shader(from_cstr("assets/shaders/basic2d")));

    Camera* const camera = find_entity(em, em->camera_id);

    const Rect viewport_in_world_space = get_viewport_in_world_space(camera);
    const Rect draw_rect = move_rect(entity->bounds, entity->location);

    if (!rect_overlaps_rect(draw_rect, viewport_in_world_space, 0)) return;

    imm_begin();
    imm_rect(draw_rect, -4.f, v4(1.f, 0.f, 0.2f, 1.f));
    imm_flush();
}

#define ENTITY_FUNCTIONS(entry) \
entry(ET_Camera, tick_camera, draw_null) \
entry(ET_Pawn, tick_pawn, draw_pawn) 

static Pawn* make_pawn(Entity_Manager* em, Vector2 location, Vector2 target_location) {
    Pawn* const result = make_entity(em, Pawn);
    result->bounds   = (Rect) { v2(-0.5f, 0.f), v2(0.5f, 2.f) };
    result->location = location;
    result->target_location = target_location;
    return result;
}

static Camera* make_camera(Entity_Manager* em, Vector2 location, f32 ortho_size) {
    Camera* const result = make_entity(em, Camera);
    result->location = location;
    result->current_ortho_size = ortho_size;
    result->target_ortho_size  = ortho_size;
    return result;
}

static void tick_null(Entity_Manager* em, Entity* entity, f32 dt) { }
static void draw_null(Entity_Manager* em, Entity* entity) { }

static void regen_map(Entity_Manager* em, Random_Seed seed) {
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

    Camera* const camera = make_camera(em, v2z(), 5.f);
    set_camera(em, camera);

    for (int i = 0; i < 1; ++i) {
        const f32 xy = get_random_f32(seed, 0.f, 500.f);
        iterate_seed(&seed);
        make_pawn(em, v2s(100.f), v2s(xy));
    }
}

typedef struct Game_State {
    Entity_Manager* entity_manager;

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
    if (game_state->is_initialized) return;
    game_state->is_initialized = true;

    regen_map(game_state->entity_manager, (Random_Seed) { g_platform->cycles() });
}

DLL_EXPORT void tick_game(f32 dt) {
    // Do input handling
    for (int i = 0; i < g_platform->num_events; ++i) {
        const OS_Event event = g_platform->events[i];

        switch (event.type) {
        case OET_Window_Resized:
            resize_draw(g_platform->window_width, g_platform->window_height);
            break;
        }
    }

    Entity_Manager* const em = game_state->entity_manager;

    // Tick the game state
    {
        for (entity_iterator(em)) {
            Entity* const entity = entity_from_iterator(iter);

            switch (entity->type) {
#define TICK_ENTITIES(type, tick, draw) \
            case type: tick(em, entity, dt); break;
            ENTITY_FUNCTIONS(TICK_ENTITIES);
#undef TICK_ENTITIES
            };
        }
    }

    // Draw the game state
    const f64 before_draw = g_platform->time_in_seconds();
    {
        glViewport(0, 0, g_platform->window_width, g_platform->window_height);
        clear_framebuffer(v3s(0.01f));

        draw_state->num_draw_calls = 0;

        Camera* const camera = find_entity(em, em->camera_id);
        if (camera) {
            // Draw the tilemap
            set_shader(find_shader(from_cstr("assets/shaders/basic2d")));
            set_uniform_texture("diffuse", *find_texture2d(from_cstr("assets/sprites/terrain_map")));
            draw_from(camera->location, camera->current_ortho_size);

            const Rect viewport_in_world_space = get_viewport_in_world_space(camera);
            
            imm_begin();
            for (int i = 0; i < em->chunk_count; ++i) {
                Chunk* const chunk = &em->chunks[i];
                
                const Vector2 min = v2((f32)chunk->x * CHUNK_SIZE, (f32)chunk->y * CHUNK_SIZE);
                const Vector2 max = v2_add(min, v2(CHUNK_SIZE, CHUNK_SIZE));
                const Rect chunk_rect = { min, max };

                if (!rect_overlaps_rect(viewport_in_world_space, chunk_rect, 0)) continue;

                const Vector2 pos = v2((f32)(chunk->x * CHUNK_SIZE), (f32)(chunk->y * CHUNK_SIZE));
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int y = 0; y < CHUNK_SIZE; ++y) {
                        Tile* const tile = &chunk->tiles[x + y * CHUNK_SIZE];

                        const Vector2 tmin = v2_add(pos, v2((f32)x, (f32)y));
                        const Vector2 tmax = v2_add(tmin, v2s(1.f));

                        const f32 tile_size = 32;
                        const Vector2 uv0 = tile->type == TT_Grass ? v2z() : v2(tile_size / 512.f, 0.f);
                        const Vector2 uv1 = tile->type == TT_Grass ? v2s(tile_size / 512.f) : v2((tile_size / 512.f) * 2.f, tile_size / 512.f);

                        const Rect rect = { tmin, tmax };
                        imm_textured_rect(rect, -5.f - (f32)chunk->z, uv0, uv1, v4s(1.f));
                    }
                }
            }
            imm_flush();

            for (entity_iterator(em)) {
                Entity* const entity = entity_from_iterator(iter);

                switch (entity->type) {
#define DRAW_ENTITIES(type, tick, draw) \
                case type: draw(em, entity);
                ENTITY_FUNCTIONS(DRAW_ENTITIES);
#undef DRAW_ENTITIES
                };
            }
        }
    }
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