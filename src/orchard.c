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

typedef int Entity_Id;

typedef enum Entity_Type {
    ET_Character,
    ET_Static_Object,
    ET_Rope,
} Entity_Type;

enum Entity_Flags {
    EF_Active = (1 << 0),
};

typedef struct Entity {
    Entity_Id   id;
    String      name;
    Entity_Type type;
    int         flags;

    Vector3 position;
    Vector2 bounds;

    union {
        // ET_Character
        struct {
            Vector2 velocity;
            b32 pressed_jump;
            b32 flipped;
        };
        // ET_Static_Object
        struct {
            Vector3 color;
        };
        // ET_Rope 
        struct {
            Vector2 end_pos;
        };
    };
} Entity;

#define ENTITY_CAP 256
typedef struct Entity_Manager {
    Entity entities[ENTITY_CAP];
    int entity_count;

    Entity_Id last_id;
} Entity_Manager;

typedef struct Entity_Iterator {
    Entity_Manager* manager;
    int found_entity_count;
    int index;
} Entity_Iterator;

static Entity_Iterator make_entity_iterator(Entity_Manager* manager) {
    for (int i = 0; i < ENTITY_CAP; ++i) {
        Entity* const e = &manager->entities[i];

        if ((e->flags & EF_Active) == 0) continue;

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
        Entity* const e = &iter->manager->entities[i];

        if ((e->flags & EF_Active) == 0) continue;

        iter->index = i;
        break;
    }
}

#define entity_iterator(em) Entity_Iterator iter = make_entity_iterator(em); can_step_entity_iterator(iter); step_entity_iterator(&iter)

static Entity* get_entity_from_iterator(Entity_Iterator iter) {
    return &iter.manager->entities[iter.index];
}

static Entity* push_entity(Entity_Manager* manager, Entity_Type type) {
    if (manager->entity_count == ENTITY_CAP) return 0;

    for (int i = 0; i < ENTITY_CAP; ++i) {
        Entity* const e = &manager->entities[i];
        if ((e->flags & EF_Active) != 0) continue;

        *e = (Entity) { 0 };
        e->id = ++manager->last_id;
        e->type = type;
        e->flags = EF_Active;
        manager->entity_count++;
        return e;
    }

    return 0;
}

static Entity* find_entity(Entity_Manager* manager, Entity_Id id) {
    for (entity_iterator(manager)) {
        Entity* const e = get_entity_from_iterator(iter);
        if (e->id == id) return e;
    }

    return 0;
}

static b32 pop_entity(Entity_Manager* manager, Entity_Id id) {
    Entity* const e = find_entity(manager, id);
    if (e == 0) return false;
    *e = (Entity) { 0 };
    manager->entity_count--;
    return true;
}

#define ENTITY_TICK(macro) \
macro(ET_Character, tick_character)

#define ENTITY_DRAW(macro) \
macro(ET_Character, draw_character) \
macro(ET_Static_Object, draw_static_object) \
macro(ET_Rope, draw_rope)

#define METER 32.f
#define GRAVITY_CONSTANT (-9.8f * METER)

typedef struct Game_State {
    Entity_Manager entity_manager;

    Vector2 current_cam_pos;
    Vector2 target_cam_pos;

    f32 plane_rot;

    b32 is_initialized;
} Game_State;

static Game_State* g_game_state;

static void tick_character(Entity* e, f32 dt) {
    e->velocity.y += GRAVITY_CONSTANT * dt;

    e->velocity.x = 0.f;
    if (g_platform->input.keys_down[KEY_D]) {
        e->velocity.x = 6.f * METER;
        e->flipped = false;
    }
    if (g_platform->input.keys_down[KEY_A]) {
        e->velocity.x = -6.f * METER;
        e->flipped = true;
    }

    if (!e->pressed_jump && g_platform->input.keys_down[KEY_SPACE]) {
        e->pressed_jump = true;
        e->velocity.y = 200.f;
    } 

    // Vertical movement
    {
        const Vector2 old_position      = e->position.xy;
        const Vector2 target_position   = v2_add(old_position, v2_mul(v2(0.f, e->velocity.y), v2s(dt)));
        const Vector2 target_move       = v2_sub(target_position, old_position);
        const float target_move_len     = v2_len(target_move);

        float distance = 1.f;
        Vector2 impact_normal = v2z();
        for (entity_iterator(&g_game_state->entity_manager)) {
            const Entity* other_e = get_entity_from_iterator(iter);
            if (other_e->type != ET_Static_Object) continue;

            const Rect other_e_bounds = rect_from_pos(other_e->position.xy, other_e->bounds);

            Rect_Intersect_Result result;
            if (rect_sweep_rect(old_position, target_position, e->bounds, other_e_bounds, &result)) {
                const Vector2 actual_move = v2_sub(result.intersection, old_position);
                const f32 actual_move_len = v2_len(actual_move);
                
                const f32 new_distance = actual_move_len / target_move_len;
                if (new_distance < distance) {
                    distance = new_distance;
                    impact_normal = result.normal;
                } else if (new_distance == distance) impact_normal = v2_add(impact_normal, result.normal);
            }
        }

        if (isnan(distance)) distance = 0.f; // @CRT

        if (distance != 1.f) {
            if (impact_normal.x != 0.f) e->velocity.x = 0.f;
            if (impact_normal.y != 0.f) e->velocity.y = 0.f;

            e->pressed_jump = false;
        }

        const Vector2 new_position = v2_add(old_position, v2_mul(target_move, v2s(distance)));
        e->position.xy = new_position;
    }

    // Horizontal movement
    {
        const Vector2 old_position      = e->position.xy;
        const Vector2 target_position   = v2_add(old_position, v2_mul(v2(e->velocity.x, 0.f), v2s(dt)));
        const Vector2 target_move       = v2_sub(target_position, old_position);
        const float target_move_len     = v2_len(target_move);

        float distance = 1.f;
        Vector2 impact_normal = v2z();
        for (entity_iterator(&g_game_state->entity_manager)) {
            const Entity* other_e = get_entity_from_iterator(iter);
            if (other_e->type != ET_Static_Object) continue;

            const Rect other_e_bounds = rect_from_pos(other_e->position.xy, other_e->bounds);

            Rect_Intersect_Result result;
            if (rect_sweep_rect(old_position, target_position, e->bounds, other_e_bounds, &result)) {
                const Vector2 actual_move = v2_sub(result.intersection, old_position);
                const f32 actual_move_len = v2_len(actual_move);
                
                const f32 new_distance = actual_move_len / target_move_len;
                if (new_distance < distance) {
                    distance = new_distance;
                    impact_normal = result.normal;
                } else if (new_distance == distance) impact_normal = v2_add(impact_normal, result.normal);
            }
        }

        if (isnan(distance)) distance = 0.f; // @CRT

        if (distance != 1.f) {
            if (impact_normal.x != 0.f) e->velocity.x = 0.f;
            if (impact_normal.y != 0.f) e->velocity.y = 0.f;
        }

        const Vector2 new_position = v2_add(old_position, v2_mul(target_move, v2s(distance)));
        e->position.xy = new_position;
    }

    // g_game_state->target_cam_pos = v2_add(e->position.xy, v2(0.f, METER));
}

static Texture2d* g_character_texture;

static void draw_character(Entity* e) {
    const Rect rect = rect_from_pos(v2_round(e->position.xy), e->bounds);
    set_uniform_texture("diffuse_tex", *g_character_texture);
    imm_begin();

    if (e->flipped) imm_textured_rect(rect, -5.f, v2(1.f, 0.f), v2(0.f, 1.f), v4s(1.f));
    else imm_textured_rect(rect, -5.f, v2z(), v2s(1.f), v4s(1.f));
    imm_flush();
}

static void draw_static_object(Entity* e) {
    imm_begin();
    const Rect rect = rect_from_pos(e->position.xy, e->bounds);
    imm_rect(rect, -5.f, v4(e->color.x, e->color.y, e->color.z, 1.f));
    imm_flush();
}

static void draw_rope(Entity* e) {
    imm_begin();
    const Vector2 a1 = e->position.xy;
    const Vector2 a2 = e->end_pos;
    imm_line(a1, a2, -5.f, 1.f, v4s(1.f));
    imm_flush();
}

DLL_EXPORT void init_game(Platform* platform) {
    g_platform = platform;

    init_logger(platform);
    init_opengl(platform);
    init_asset_manager(platform);
    init_draw(platform->permanent_arena);

    // Init Game State
    g_game_state = mem_alloc_struct(platform->permanent_arena, Game_State);
    g_character_texture = find_texture2d(string_from_raw("assets/sprites/test_character"));
    if (!g_game_state->is_initialized) {
        Entity_Manager* const em = &g_game_state->entity_manager;
        
        Entity* const ground = push_entity(em, ET_Static_Object);
        ground->bounds = v2(100000.f, METER);
        ground->position.y = -4.f * METER;
        ground->color = v3(0.f, 0.7f, 0.2f);

        Entity* const box = push_entity(em, ET_Static_Object);
        box->bounds = v2s(METER);
        box->position.xy = v2(5.f * METER, -8.f * METER + METER);
        box->color = v3(0.2f, 0.7f, 0.7f);

        Entity* const player = push_entity(em, ET_Character);
        player->bounds = v2(METER, 2 * METER);

        g_game_state->is_initialized = true;
    }
}

DLL_EXPORT void tick_game(f32 dt) {
    g_game_state->current_cam_pos = v2_lerp(
        g_game_state->current_cam_pos, 
        g_game_state->target_cam_pos,
        dt * 2.f
    );

    Entity_Manager* const em = &g_game_state->entity_manager;
    for (entity_iterator(em)) {
        Entity* const e = get_entity_from_iterator(iter);

        switch (e->type) {
#define TICK_entity_count(t, f) case t: f(e, dt); break;
            ENTITY_TICK(TICK_entity_count);
#undef TICK_entity_count
        };
    }

    begin_draw();
    {
        imm_draw_from(v3xy(g_game_state->current_cam_pos, 0.f));

        for (entity_iterator(em)) {
            Entity* const e = get_entity_from_iterator(iter);

            switch (e->type) {
#define DRAW_entity_count(t, f) case t: f(e); break;
                ENTITY_DRAW(DRAW_entity_count);
#undef DRAW_entity_count
            };
        }

        g_game_state->plane_rot += dt;

        const Quaternion rot = quat_from_axis_angle(v3(0.f, 1.f, 0.f), g_game_state->plane_rot);

        Mesh* const mesh = find_mesh(string_from_raw("assets/meshes/sphere"));
        Shader* const shader = find_shader(string_from_raw("assets/shaders/solid_color_geometry"));
        set_shader(shader);

        imm_draw_persp(v3z());
        draw_mesh(mesh, v3(0.f, 0.f, -10.f), rot, v3s(1.f));
    }
    end_draw();

    // Draw UI
    {
        set_shader(g_font_shader);
        const Rect viewport = { v2z(), v2((f32)g_platform->window_width, (f32)g_platform->window_height) };
        imm_draw_right_handed(viewport);

        Font* const the_font = font_at_size(g_font_collection, (int)(32.f * g_platform->dpi_scale));
        set_uniform_texture("atlas", the_font->atlas);

        imm_begin();
        imm_string(string_from_raw("Hello World!"), the_font, 1000.f, v2z(), -5.f, v4(1.f, 1.f, 1.f, 1.f));
        imm_flush();
    }

    swap_gl_buffers(g_platform);
}

DLL_EXPORT void shutdown_game(void) {

}