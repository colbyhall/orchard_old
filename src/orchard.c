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
#include "math.c"
#include "opengl.c"
#include "draw.c"

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
} Entity;

#define ENTITIES_CAP 256
typedef struct Entity_Manager {
    Entity entities[ENTITIES_CAP];
    int num_entities;

    Entity_Id last_id;
} Entity_Manager;

typedef struct Entity_Iterator {
    Entity_Manager* manager;
    int found_entities;
    int index;
} Entity_Iterator;

static Entity_Iterator make_entity_iterator(Entity_Manager* manager) {
    for (int i = 0; i < ENTITIES_CAP; ++i) {
        Entity* const e = &manager->entities[i];

        if ((e->flags & EF_Active) == 0) continue;

        return (Entity_Iterator) { manager, 0, i };
    }

    return (Entity_Iterator) { 0 };
}

static b32 can_step_entity_iterator(Entity_Iterator iter) {
    return (
        iter.manager != 0 && 
        iter.index < iter.manager->num_entities && 
        iter.found_entities < iter.manager->num_entities
    );
}

static void step_entity_iterator(Entity_Iterator* iter) {
    iter->found_entities++;
    if (iter->found_entities == iter->manager->num_entities) return;

    for (int i = iter->index + 1; i < ENTITIES_CAP; ++i) {
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
    if (manager->num_entities == ENTITIES_CAP) return 0;

    for (int i = 0; i < ENTITIES_CAP; ++i) {
        Entity* const e = &manager->entities[i];
        if ((e->flags & EF_Active) != 0) continue;

        *e = (Entity) { 0 };
        e->id = ++manager->last_id;
        e->type = type;
        e->flags = EF_Active;
        manager->num_entities++;
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
    manager->num_entities--;
    return true;
}

#define ENTITY_TICK(macro) \
macro(ET_Character, tick_character)

#define ENTITY_DRAW(macro) \
macro(ET_Character, draw_character) \
macro(ET_Static_Object, draw_static_object)

static void tick_character(Entity* e, f32 dt) {

}

static void draw_character(Entity* e) {
    imm_begin();
    const Rect rect = rect_from_pos(e->position.xy, e->bounds);
    imm_rect(rect, -5.f, v4(1.f, 0.f, 0.2f, 1.f));
    imm_flush();
}

static void draw_static_object(Entity* e) {
    imm_begin();
    const Rect rect = rect_from_pos(e->position.xy, e->bounds);
    imm_rect(rect, -5.f, v4(0.f, 0.7f, 0.2f, 1.f));
    imm_flush();
}

typedef struct Game_State {
    Entity_Manager entity_manager;

    b32 is_initialized;
} Game_State;

static Game_State* g_game_state;

DLL_EXPORT void init_game(Platform* platform) {
    g_platform = platform;

    init_opengl(platform);
    init_draw(platform->permanent_arena);
    g_game_state = mem_alloc_struct(platform->permanent_arena, Game_State);
    if (!g_game_state->is_initialized) {
        Entity_Manager* const em = &g_game_state->entity_manager;
        
        Entity* const ground = push_entity(em, ET_Static_Object);
        ground->bounds = v2(100000.f, 300.f);
        ground->position.y = -300.f;

        Entity* const player = push_entity(em, ET_Character);
        player->bounds = v2(100.f, 180.f);

        g_game_state->is_initialized = true;
    }
}

DLL_EXPORT void tick_game(f32 dt) {
    Entity_Manager* const em = &g_game_state->entity_manager;
    for (entity_iterator(em)) {
        Entity* const e = get_entity_from_iterator(iter);

        switch (e->type) {
#define TICK_ENTITIES(t, f) case t: f(e, dt); break;
            ENTITY_TICK(TICK_ENTITIES);
#undef TICK_ENTITIES
        };
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);

    const Rect viewport = { v2z(), v2((f32)g_platform->window_width, (f32)g_platform->window_height) };
    imm_render_ortho(v3z(), viewport.max.width / viewport.max.height, viewport.max.height / 2.f);

    for (entity_iterator(em)) {
        Entity* const e = get_entity_from_iterator(iter);

        switch (e->type) {
#define DRAW_ENTITIES(t, f) case t: f(e); break;
            ENTITY_DRAW(DRAW_ENTITIES);
#undef DRAW_ENTITIES
        };
    }

    swap_gl_buffers(g_platform);
}

DLL_EXPORT void shutdown_game(void) {

}