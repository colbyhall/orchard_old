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

    Pawn* pawn = entity->derived;

    if (!pawn->task_count) return;

    Task* current_task = &pawn->task_queue[0];
}

static void draw_pawn(Entity_Manager* em, Entity* entity) {
    Pawn* pawn = entity->derived;

#if DEBUG_BUILD
    if (g_debug_state->draw_pathfinding) draw_pathfind_debug(em, pawn->path);
#endif

    set_shader(find_shader(from_cstr("assets/shaders/basic2d")));
    Rect draw_rect = move_rect(entity->bounds, entity->location);

    imm_begin();

    if (pawn->path.ref_count) {
        Tile_Ref first = pawn->path.refs[0];
        imm_line(entity->location, v2((f32)first.x + 0.5f, (f32)first.y + 0.5f), -4.f, 0.1f, v4(1.f, 1.f, 1.f, 0.5f));        

        for (int i = 0; i < pawn->path.ref_count - 1; ++i) {
            Tile_Ref a = pawn->path.refs[i];
            Tile_Ref b = pawn->path.refs[i + 1];

            imm_line(v2((f32)a.x + 0.5f, (f32)a.y + 0.5f), v2((f32)b.x + 0.5f, (f32)b.y + 0.5f), -4.f, 0.1f, v4(1.f, 1.f, 1.f, 0.5f));        
        }
    }

    imm_rect(draw_rect, -3.f, v4(1.f, 0.f, 0.2f, 1.f));
    imm_flush();
}
