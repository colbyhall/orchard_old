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

typedef int Entity_Id;

typedef enum Entity_Type {
    ET_Character,
    ET_Static_Object
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
    };
} Entity;

#define entity_count_CAP 256
typedef struct Entity_Manager {
    Entity entity_count[entity_count_CAP];
    int num_entity_count;

    Entity_Id last_id;
} Entity_Manager;

typedef struct Entity_Iterator {
    Entity_Manager* manager;
    int found_entity_count;
    int index;
} Entity_Iterator;

static Entity_Iterator make_entity_iterator(Entity_Manager* manager) {
    for (int i = 0; i < entity_count_CAP; ++i) {
        Entity* const e = &manager->entity_count[i];

        if ((e->flags & EF_Active) == 0) continue;

        return (Entity_Iterator) { manager, 0, i };
    }

    return (Entity_Iterator) { 0 };
}

static b32 can_step_entity_iterator(Entity_Iterator iter) {
    return (
        iter.manager != 0 && 
        iter.index < iter.manager->num_entity_count && 
        iter.found_entity_count < iter.manager->num_entity_count
    );
}

static void step_entity_iterator(Entity_Iterator* iter) {
    iter->found_entity_count++;
    if (iter->found_entity_count == iter->manager->num_entity_count) return;

    for (int i = iter->index + 1; i < entity_count_CAP; ++i) {
        Entity* const e = &iter->manager->entity_count[i];

        if ((e->flags & EF_Active) == 0) continue;

        iter->index = i;
        break;
    }
}

#define entity_iterator(em) Entity_Iterator iter = make_entity_iterator(em); can_step_entity_iterator(iter); step_entity_iterator(&iter)

static Entity* get_entity_from_iterator(Entity_Iterator iter) {
    return &iter.manager->entity_count[iter.index];
}

static Entity* push_entity(Entity_Manager* manager, Entity_Type type) {
    if (manager->num_entity_count == entity_count_CAP) return 0;

    for (int i = 0; i < entity_count_CAP; ++i) {
        Entity* const e = &manager->entity_count[i];
        if ((e->flags & EF_Active) != 0) continue;

        *e = (Entity) { 0 };
        e->id = ++manager->last_id;
        e->type = type;
        e->flags = EF_Active;
        manager->num_entity_count++;
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
    manager->num_entity_count--;
    return true;
}

#define ENTITY_TICK(macro) \
macro(ET_Character, tick_character)

#define ENTITY_DRAW(macro) \
macro(ET_Character, draw_character) \
macro(ET_Static_Object, draw_static_object)

#define METER 32.f
#define GRAVITY_CONSTANT (-9.8f * METER)

typedef struct Game_State {
    Entity_Manager entity_manager;

    Vector2 current_cam_pos;
    Vector2 target_cam_pos;

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

    g_game_state->target_cam_pos = v2_add(e->position.xy, v2(0.f, METER));
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
        ground->position.y = -8.f * METER;
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
    Entity_Manager* const em = &g_game_state->entity_manager;
    for (entity_iterator(em)) {
        Entity* const e = get_entity_from_iterator(iter);

        switch (e->type) {
#define TICK_entity_count(t, f) case t: f(e, dt); break;
            ENTITY_TICK(TICK_entity_count);
#undef TICK_entity_count
        };
    }

    g_game_state->current_cam_pos = v2_lerp(
        g_game_state->current_cam_pos, 
        g_game_state->target_cam_pos,
        dt * 2.f
    );

    begin_draw();
    {
        imm_render_from(v3xy(g_game_state->current_cam_pos, 0.f));

        for (entity_iterator(em)) {
            Entity* const e = get_entity_from_iterator(iter);

            switch (e->type) {
#define DRAW_entity_count(t, f) case t: f(e); break;
                ENTITY_DRAW(DRAW_entity_count);
#undef DRAW_entity_count
            };
        }
    }
    end_draw();

    // Draw UI
    {
        set_shader(g_font_shader);
        const Rect viewport = { v2z(), v2((f32)g_platform->window_width, (f32)g_platform->window_height) };
        imm_render_right_handed(viewport);

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