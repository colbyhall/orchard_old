#include "pawn.h"

Pawn* make_pawn(Entity_Manager* em, Vector2 location) {
    Pawn* result = make_entity(em, Pawn);
    result->bounds   = (Rect) { v2(-0.5f, 0.f), v2(0.5f, 2.f) };
    result->location = location;
    return result;
}

static void tick_pawn(Entity_Manager* em, Entity* entity, f32 dt) {
    assert(entity->type == ET_Pawn);

    // Pawn* pawn = entity->derived;
}

static void draw_pawn(Entity_Manager* em, Entity* entity) {
    Pawn* pawn = entity->derived;

    set_shader(find_shader(from_cstr("assets/shaders/basic2d")));
    Rect draw_rect = move_rect(entity->bounds, entity->location);

    imm_begin();
    imm_rect(draw_rect, -3.f, v4(1.f, 0.f, 0.2f, 1.f));
    imm_flush();
}
