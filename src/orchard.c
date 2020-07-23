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

typedef enum Wall_Type {
    WT_Steel = 0,
} Wall_Type;

typedef enum Wall_Visual {
    WV_North,
    WV_South,
    WV_East,
    WV_West,
    WV_South_East,
    WV_South_West,
    WV_East_West,
    WV_Cross,

    WV_Count,
} Wall_Visual;

typedef struct Wall {
    Wall_Type type;
    Wall_Visual visual;
} Wall;

typedef enum Tile_Type {
    TT_Open = 0,
    TT_Steel,
} Tile_Type;

static const char* tile_type_names[] = {
    "Open",
    "Steel",
};

typedef enum Tile_Content {
    TC_None = 0,
    TC_Wall
} Tile_Content;

typedef struct Tile {
    Tile_Type type;
    Tile_Content content;
    union {
        Wall wall;
    };
} Tile;

#define CHUNK_SIZE 16
typedef struct Chunk {
    int x, y, z;
    Tile* tiles; // Allocated elsewhere because we scan chunks for their pos in a list
} Chunk;

typedef u32 Entity_Id; // Invalid Entity_Id is 0

typedef enum Entity_Type {
    ET_Controller,
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

    Entity_Id controller_id;

    Allocator entity_memory;
} Entity_Manager;

typedef struct Entity_Iterator {
    Entity_Manager* manager;
    int found_entity_count;
    int index;
} Entity_Iterator;

static Entity_Iterator make_entity_iterator(Entity_Manager* manager) {
    for (int i = 0; i < ENTITY_CAP; ++i) {
        Entity* e = manager->entities[i];

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
        Entity* e = iter->manager->entities[i];

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
        Entity* entity = entity_from_iterator(iter);

        if (entity->id == id) return entity->derived;
    }

    return 0;
}

static Tile* find_tile_at(Entity_Manager* em, int x, int y, int z) {
    int chunk_x = x / CHUNK_SIZE;
    int chunk_y = y / CHUNK_SIZE;

    if (x < 0 ||  y < 0) return 0;

    for (int i = 0; i < em->chunk_count; ++i) {
        Chunk chunk = em->chunks[i];
        if (chunk.x == chunk_x && chunk.y == chunk_y && chunk.z == z) {
            int local_x = x - chunk_x * CHUNK_SIZE;
            int local_y = y - chunk_y * CHUNK_SIZE;
            assert(local_x <= CHUNK_SIZE && local_y <= CHUNK_SIZE);
            return &chunk.tiles[local_x + local_y * CHUNK_SIZE];
        }
    }

    return 0;
}

static Entity_Manager* make_entity_manager(Allocator allocator) {
    Entity_Manager* result = mem_alloc_struct(allocator, Entity_Manager);

    result->tile_memory = allocator;
    result->chunk_count = CHUNK_CAP;

    for (int i = 0; i < result->chunk_count; ++i) {
        Chunk* chunk = &result->chunks[i];

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
        Entity** entity = &em->entities[i];
        if (!(*entity)) {
            *entity = result;
            em->entity_count++;
            break;
        }
    }

    return result;
}
#define make_entity(em, type) _make_entity(em, sizeof(type), ET_ ## type)

static void refresh_wall_visual(Entity_Manager* em, int x, int y, int z, b32 first) {
    Tile* tile = find_tile_at(em, x, y, z);
    if (!tile) return;
    if (tile->content != TC_Wall && !first) return;

    b32 has_north = false;
    Tile* north = find_tile_at(em, x, y + 1, z);
    if (north && north->content == TC_Wall) {
        if (first) refresh_wall_visual(em, x, y + 1, z, false);
        has_north = true;
    }

    b32 has_south = false;
    Tile* south = find_tile_at(em, x, y - 1, z);
    if (south && south->content == TC_Wall) {
        if (first) refresh_wall_visual(em, x, y - 1, z, false);
        has_south = true;
    }

    b32 has_east = false;
    Tile* east = find_tile_at(em, x + 1, y, z);
    if (east && east->content == TC_Wall) {
        if (first) refresh_wall_visual(em, x + 1, y, z, false);
        has_east = true;
    }

    b32 has_west = false;
    Tile* west = find_tile_at(em, x - 1, y, z);
    if (west && west->content == TC_Wall) {
        if (first) refresh_wall_visual(em, x - 1, y, z, false);
        has_west = true;
    }

    if (tile->content != TC_Wall && first) return;

    int num_surrounding = has_north + has_south + has_east + has_west;
    switch (num_surrounding) {
    case 0: 
        tile->wall.visual = WV_South;
        break;
    case 1: {
        Wall_Visual visual    = WV_South;
        if (has_south) visual = WV_North;
        if (has_east) visual  = WV_West;
        if (has_west) visual  = WV_East;
        tile->wall.visual = visual;
    } break;
    case 2: {
        Wall_Visual visual = WV_South;
        if (has_south && has_north) visual = WV_North;
        if (has_east && has_west)   visual = WV_East_West;
        if (has_east && has_north)  visual = WV_West;
        if (has_west && has_north)  visual = WV_East;
        if (has_east && has_south)  visual = WV_South_East;
        if (has_west && has_south)  visual = WV_South_West;
        tile->wall.visual = visual;
    } break;
    case 3: {
        Wall_Visual visual = WV_Cross;
        if (has_north && !has_south) visual = WV_East_West;
        if (has_north && has_south && has_east) visual = WV_South_East;
        if (has_north && has_south && has_west) visual = WV_South_West;
        tile->wall.visual = visual;
    } break;
    case 4:
        tile->wall.visual = WV_Cross;
        break;
    }
}

typedef enum Controller_Mode {
    CM_Normal,
    CM_Set_Tile,
    CM_Set_Wall,
} Controller_Mode;

typedef struct Controller_Selection {
    b32 valid;
    Vector2 start;
    Vector2 current;
} Controller_Selection;

#define MAX_CAMERA_ORTHO_SIZE 40.f
#define MIN_CAMERA_ORTHO_SIZE 5.f
typedef struct Controller {
    DEFINE_CHILD_ENTITY;
    f32 current_ortho_size;
    f32 target_ortho_size;

    Controller_Mode mode;
    union {
        Controller_Selection selection;
    };
} Controller;

static Vector2 get_mouse_pos_in_world_space(Controller* controller) {
    f32 ratio = (controller->current_ortho_size * 2.f) / (f32)g_platform->window_height;
    int adjusted_x = g_platform->input.state.mouse_x - g_platform->window_width / 2;
    int adjusted_y = g_platform->input.state.mouse_y - g_platform->window_height / 2;
    return v2_add(v2_mul(v2((f32)adjusted_x, (f32)adjusted_y), v2s(ratio)), controller->location);
}

static void set_controller(Entity_Manager* em, Controller* controller) {
    if (controller) {
        em->controller_id = controller->id;
        return;
    }
    em->controller_id = 0;
}

static Rect get_viewport_in_world_space(Controller* controller) {
    if (!controller) return rect_from_raw(0.f, 0.f, 0.f, 0.f);
    
    f32 ratio = (controller->current_ortho_size * 2.f) / (f32)g_platform->window_height;
    f32 adjusted_width = (f32)g_platform->window_width * ratio;
    f32 adjusted_height = (f32)g_platform->window_height * ratio;

    return rect_from_pos(controller->location, v2(adjusted_width, adjusted_height));
}

static void tick_controller(Entity_Manager* em, Entity* entity, f32 dt) {
    assert(entity->type == ET_Controller);
    Controller* controller = entity->derived;

    f32 mouse_wheel_delta = (f32)g_platform->input.state.mouse_wheel_delta / 50.f;
    controller->target_ortho_size -= mouse_wheel_delta;
    controller->target_ortho_size = CLAMP(controller->target_ortho_size, MIN_CAMERA_ORTHO_SIZE, MAX_CAMERA_ORTHO_SIZE);

    Vector2 old_mouse_pos_in_world = get_mouse_pos_in_world_space(controller);

    f32 old_ortho_size = controller->current_ortho_size;
    controller->current_ortho_size = lerpf(controller->current_ortho_size, controller->target_ortho_size, dt * 5.f);
    f32 delta_ortho_size = controller->current_ortho_size - old_ortho_size;

    Vector2 mouse_pos_in_world = get_mouse_pos_in_world_space(controller);
    Vector2 delta_mouse_pos_in_world = v2_sub(old_mouse_pos_in_world, mouse_pos_in_world);
    if (delta_ortho_size != 0.f) controller->location = v2_add(controller->location, delta_mouse_pos_in_world);

    f32 ratio = (controller->current_ortho_size * 2.f) / (f32)g_platform->window_height;

    if (g_platform->input.state.mouse_buttons_down[MOUSE_MIDDLE]) {
        Vector2 mouse_delta = v2((f32)g_platform->input.state.mouse_dx, (f32)g_platform->input.state.mouse_dy);
        f32 speed = ratio;

        controller->location = v2_add(controller->location, v2_mul(v2_inverse(mouse_delta), v2s(speed)));
    }

    // Tile Mode
    if (was_key_pressed(KEY_F1)) {
        if (controller->mode == CM_Set_Tile) controller->mode = CM_Normal;
        controller->mode = CM_Set_Tile;
    }

    // Wall Mode
    if (was_key_pressed(KEY_F2)) {
        if (controller->mode == CM_Set_Wall) controller->mode = CM_Normal;
        controller->mode = CM_Set_Wall;
    }

    u8 selection_mouse_button = MOUSE_LEFT;

    controller->selection.valid = is_mouse_button_pressed(selection_mouse_button);
    if (controller->selection.valid) {
        if (was_mouse_button_pressed(selection_mouse_button)) controller->selection.start = mouse_pos_in_world;
        controller->selection.current = mouse_pos_in_world;
    }

    if (was_mouse_button_released(selection_mouse_button)) {
        Rect selection = rect_from_points(controller->selection.start, controller->selection.current);

        int start_x = (int)selection.min.x;
        int start_y = (int)selection.min.y;
        int end_x = (int)selection.max.x + 1;
        int end_y = (int)selection.max.y + 1;

        switch (controller->mode) {
        case CM_Set_Tile: {
            for (int x = start_x; x < end_x; ++x) {
                for (int y = start_y; y < end_y; ++y) {
                    Tile* tile = find_tile_at(em, x, y, 0);
                    if (tile) tile->type = TT_Steel;
                }
            }
        } break;
        case CM_Set_Wall: {
            for (int x = start_x; x < end_x; ++x) {
                Tile* start_y_tile = find_tile_at(em, x, start_y, 0);
                if (start_y_tile) {
                    start_y_tile->content = TC_Wall;
                    start_y_tile->wall.type = WT_Steel;
                    refresh_wall_visual(em, x, start_y, 0, true);
                }

                Tile* end_y_tile = find_tile_at(em, x, end_y - 1, 0);
                if (end_y_tile) {
                    end_y_tile->content = TC_Wall;
                    end_y_tile->wall.type = WT_Steel;
                    refresh_wall_visual(em, x, end_y - 1, 0, true);
                }
            }

            for (int y = start_y; y < end_y; ++y) {
                Tile* start_x_tile = find_tile_at(em, start_x, y, 0);
                if (start_x_tile) {
                    start_x_tile->content = TC_Wall;
                    start_x_tile->wall.type = WT_Steel;
                    refresh_wall_visual(em, start_x, y, 0, true);
                }

                Tile* end_x_tile = find_tile_at(em, end_x - 1, y, 0);
                if (end_x_tile) {
                    end_x_tile->content = TC_Wall;
                    end_x_tile->wall.type = WT_Steel;
                    refresh_wall_visual(em, end_x - 1, y, 0, true);
                }
            }

        } break;
        }
    }

    if (controller->mode == CM_Set_Wall && was_mouse_button_released(MOUSE_RIGHT)) {
        Tile* tile = find_tile_at(em, (int)mouse_pos_in_world.x, (int)mouse_pos_in_world.y, 0);
        if (tile && tile->content == TC_Wall) {
            tile->content = TC_None;
            refresh_wall_visual(em, (int)mouse_pos_in_world.x, (int)mouse_pos_in_world.y, 0, true);
        }
    }
}

typedef struct Pawn {
    DEFINE_CHILD_ENTITY;
    Vector2 target_location;
    f32 idle_time;
} Pawn;

static void tick_pawn(Entity_Manager* em, Entity* entity, f32 dt) {
    assert(entity->type == ET_Pawn);

    f32 max_speed = 3.f;
    Pawn* pawn = entity->derived;

    Random_Seed seed = init_seed((int)g_platform->time_in_seconds());

    Vector2 to_point = v2_sub(pawn->target_location, pawn->location);
    f32 to_point_len = v2_len(to_point);
    if (to_point_len < 0.1f) {
        pawn->idle_time += dt;
        if (pawn->idle_time >= 3.f) {
            pawn->idle_time = 0.f;

            int chunk_cap_sq = (int)sqrt(CHUNK_CAP);
            f32 max = (f32)(chunk_cap_sq * CHUNK_SIZE);

            f32 x = random_f32_in_range(&seed, 0.f, max);
            f32 y = random_f32_in_range(&seed, 0.f, max);
            pawn->target_location = v2(x, y);

            o_log("[Game] Pawn with id %lu will be moving toward (%f, %f)", pawn->id, pawn->target_location.x, pawn->target_location.y);
        }
    } else {
        Vector2 to_point_norm = v2_div(to_point, v2s(to_point_len));
        pawn->location = v2_add(pawn->location, v2_mul(to_point_norm, v2s(max_speed * dt)));
    }
}

static void draw_pawn(Entity_Manager* em, Entity* entity) {
    set_shader(find_shader(from_cstr("assets/shaders/basic2d")));

    Controller* controller = find_entity(em, em->controller_id);

    Rect viewport_in_world_space = get_viewport_in_world_space(controller);
    Rect draw_rect = move_rect(entity->bounds, entity->location);

    if (!rect_overlaps_rect(draw_rect, viewport_in_world_space, 0)) return;

    imm_begin();
    imm_rect(draw_rect, -4.f, v4(1.f, 0.f, 0.2f, 1.f));
    imm_flush();
}

#define ENTITY_FUNCTIONS(entry) \
entry(ET_Controller, tick_controller, draw_null) \
entry(ET_Pawn, tick_pawn, draw_pawn) 

static Pawn* make_pawn(Entity_Manager* em, Vector2 location, Vector2 target_location) {
    Pawn* result = make_entity(em, Pawn);
    result->bounds   = (Rect) { v2(-0.5f, 0.f), v2(0.5f, 2.f) };
    result->location = location;
    result->target_location = target_location;
    return result;
}

static Controller* make_controller(Entity_Manager* em, Vector2 location, f32 ortho_size) {
    Controller* result = make_entity(em, Controller);
    result->location = location;
    result->current_ortho_size = ortho_size;
    result->target_ortho_size  = ortho_size;
    return result;
}

static void tick_null(Entity_Manager* em, Entity* entity, f32 dt) { }
static void draw_null(Entity_Manager* em, Entity* entity) { }

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

        Controller* controller = find_entity(em, em->controller_id);
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

            Vector2 mouse_in_world = get_mouse_pos_in_world_space(controller);
            Rect selection = { v2_floor(mouse_in_world), v2_add(v2_floor(mouse_in_world), v2s(1.f)) };
            Vector4 selection_color = color_from_hex(0x5ecf4466);
            f32 selection_z = -2.f;

            switch (controller->mode) {
            case CM_Set_Tile: {
                if (controller->selection.valid) {
                    selection = rect_from_points(controller->selection.start, controller->selection.current);
                    selection.min = v2_floor(selection.min);
                    selection.max = v2_add(v2_floor(selection.max), v2s(1.f));
                }
                imm_begin();
                imm_rect(selection, selection_z, selection_color);
                imm_flush();
            } break;
            case CM_Set_Wall: {
                if (controller->selection.valid) {
                    selection = rect_from_points(controller->selection.start, controller->selection.current);
                    selection.min = v2_floor(selection.min);
                    selection.max = v2_add(v2_floor(selection.max), v2s(1.f));
                    imm_begin();
                    imm_border_rect(selection, selection_z, 1.f, selection_color);
                    imm_flush();
                } else {
                    imm_begin();
                    imm_rect(selection, selection_z, selection_color);
                    imm_flush();
                }
            } break;
            }

            for (entity_iterator(em)) {
                Entity* entity = entity_from_iterator(iter);

                switch (entity->type) {
#define DRAW_ENTITIES(type, tick, draw) \
                case type: draw(em, entity);
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
        Font* font = font_at_size(fc, 48);
        set_uniform_texture("atlas", font->atlas);

        char controller_debug_string[256];
        Controller* controller = find_entity(em, em->controller_id);
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