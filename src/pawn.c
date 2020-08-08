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

    if (pawn->path.ref_count) {
        Tile_Ref path_tile = pawn->path.refs[0];
        Vector2 target_location = v2_add(v2((f32)path_tile.x, (f32)path_tile.y), v2s(0.5f));
        Vector2 to_point = v2_sub(target_location, pawn->location);
   
        f32 to_point_len = v2_len(to_point);
        if (to_point_len > 0.1f) {
            to_point = v2_div(to_point, v2s(to_point_len));
            pawn->location = v2_add(pawn->location, v2_mul(to_point, v2s(max_speed * dt)));
        } else {
            to_point = v2_div(to_point, v2s(to_point_len));
            pawn->location = v2_add(pawn->location, v2_mul(to_point, v2s(max_speed * dt)));

            if (pawn->path.ref_count > 0) {
                pawn->path.ref_count--;
                pawn->path.refs++;
            }
        }
    }

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

    imm_rect(draw_rect, -4.f, v4(1.f, 0.f, 0.2f, 1.f));
    imm_flush();
}
