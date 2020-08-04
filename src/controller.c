#include "controller.h"
#include "pawn.h"

Controller* make_controller(Entity_Manager* em, Vector2 location, f32 ortho_size) {
    Controller* result = make_entity(em, Controller);
    result->location = location;
    result->current_ortho_size = ortho_size;
    result->target_ortho_size  = ortho_size;
    return result;
}

Vector2 get_mouse_pos_in_world_space(Controller* controller) {
    f32 ratio = (controller->current_ortho_size * 2.f) / (f32)g_platform->window_height;
    int adjusted_x = g_platform->input.state.mouse_x - g_platform->window_width / 2;
    int adjusted_y = g_platform->input.state.mouse_y - g_platform->window_height / 2;
    return v2_add(v2_mul(v2((f32)adjusted_x, (f32)adjusted_y), v2s(ratio)), controller->location);
}

void set_controller(Entity_Manager* em, Controller* controller) {
    if (controller) {
        em->controller_id = controller->id;
        return;
    }
    em->controller_id = 0;
}

Rect get_viewport_in_world_space(Controller* controller) {
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
        else controller->mode = CM_Set_Tile;
    }

    // Wall Mode
    if (was_key_pressed(KEY_F2)) {
        if (controller->mode == CM_Set_Wall) controller->mode = CM_Normal;
        else controller->mode = CM_Set_Wall;
    }

    controller->selection.valid = is_mouse_button_pressed(MOUSE_LEFT);
    if (controller->selection.valid) {
        if (was_mouse_button_pressed(MOUSE_LEFT)) controller->selection.start = mouse_pos_in_world;
        controller->selection.current = mouse_pos_in_world;
    }

    if (controller->mode == CM_Normal) {
        Vector2 target_location = v2_add(v2_floor(mouse_pos_in_world), v2s(0.5f));
        if (was_mouse_button_pressed(MOUSE_LEFT)) {
            make_pawn(em, target_location);
        }

        if (is_mouse_button_pressed(MOUSE_RIGHT)) {
            for (entity_iterator(em)) {
                Entity* e = entity_from_iterator(iter);
                if (e->type == ET_Pawn) {
                    Pawn* pawn = e->derived;

                    f64 start = g_platform->time_in_seconds();
                    b32 can_pathfind = pathfind(
                        em, 
                        tile_ref_from_location(e->location), 
                        tile_ref_from_location(mouse_pos_in_world), 
                        &pawn->path
                    );
                    f64 duration = g_platform->time_in_seconds() - start;
                    o_log_error("Took %.2fms to do pathfinding", duration * 1000.f);

                    break;
                }
            }
        }
    }

    if (was_mouse_button_released(MOUSE_LEFT)) {
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

    if (controller->mode == CM_Set_Wall && was_mouse_button_pressed(MOUSE_RIGHT)) {
        Tile* tile = find_tile_at(em, (int)mouse_pos_in_world.x, (int)mouse_pos_in_world.y, 0);
        if (tile && tile->content == TC_Wall) {
            tile->content = TC_None;
            refresh_wall_visual(em, (int)mouse_pos_in_world.x, (int)mouse_pos_in_world.y, 0, true);
        }
    }
}