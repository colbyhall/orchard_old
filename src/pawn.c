#include "entity_manager.h"

Pawn* make_pawn(Entity_Manager* em, Vector2 location) {
    Pawn* result = make_entity(em, Pawn);
    result->bounds   = (Rect) { v2(-0.5f, 0.f), v2(0.5f, 2.f) };
    result->location = location;
    result->target_location = location;
    return result;
}

static void tick_pawn(Entity_Manager* em, Entity* entity, f32 dt) {
    assert(entity->type == ET_Pawn);

    f32 max_speed = 3.f;
    Pawn* pawn = entity->derived;

    Vector2 to_point = v2_sub(pawn->target_location, pawn->location);
    f32 to_point_len = v2_len(to_point);
    if (to_point_len > 0.01f) {
        to_point = v2_div(to_point, v2s(to_point_len));
        pawn->location = v2_add(pawn->location, v2_mul(to_point, v2s(max_speed * dt)));
    }
}

static void draw_pawn(Entity_Manager* em, Entity* entity) {
    set_shader(find_shader(from_cstr("assets/shaders/basic2d")));

    Pawn* pawn = entity->derived;
    Rect draw_rect = move_rect(entity->bounds, entity->location);

    imm_begin();
    imm_line(pawn->location, pawn->target_location, -4.f, 0.1f, v4(1.f, 1.f, 1.f, 0.5f));
    imm_rect(draw_rect, -4.f, v4(1.f, 0.f, 0.2f, 1.f));
    imm_flush();
}
