#include "entity_manager.h"
#include "controller.h"

Entity_Iterator make_entity_iterator(Entity_Manager* manager) {
    for (int i = 0; i < ENTITY_CAP; ++i) {
        Entity* e = manager->entities[i];

        if (!e) continue;

        return (Entity_Iterator) { manager, 0, i };
    }

    return (Entity_Iterator) { 0 };
}

b32 can_step_entity_iterator(Entity_Iterator iter) {
    return (
        iter.manager != 0 && 
        iter.index < iter.manager->entity_count && 
        iter.found_entity_count < iter.manager->entity_count
    );
}

void step_entity_iterator(Entity_Iterator* iter) {
    iter->found_entity_count++;
    if (iter->found_entity_count == iter->manager->entity_count) return;

    for (int i = iter->index + 1; i < ENTITY_CAP; ++i) {
        Entity* e = iter->manager->entities[i];

        if (!e) continue;

        iter->index = i;
        break;
    }
}

void* find_entity_by_id(Entity_Manager* em, Entity_Id id) {
    for (entity_iterator(em)) {
        Entity* entity = entity_from_iterator(iter);

        if (entity->id == id) return entity->derived;
    }

    return 0;
}

Tile* find_tile_at(Entity_Manager* em, int x, int y, int z) {
    int chunk_x = x / CHUNK_SIZE;
    int chunk_y = y / CHUNK_SIZE;

    Chunk_Ref ref = { chunk_x, chunk_y };

    Chunk* chunk = find_hash_table(&em->chunks, ref);
    if (!chunk) return 0;

    int local_x = x - chunk_x * CHUNK_SIZE;
    int local_y = y - chunk_y * CHUNK_SIZE;
    assert(local_x <= CHUNK_SIZE && local_y <= CHUNK_SIZE);
    return &chunk->tiles[local_x + local_y * CHUNK_SIZE];
}

static u64 hash_generic(void* a, void* b, int size) {
    if (b) {
        for (int i = 0; i < size; ++i) {
            if (((u8*)a)[i] != ((u8*)b)[i]) return false;
        }
        return true;
    }

    return fnv1_hash(a, size);
}

Entity_Manager* make_entity_manager(Allocator allocator) {
    Entity_Manager* result = mem_alloc_struct(allocator, Entity_Manager);

    result->tile_memory = allocator;
    result->chunks = make_hash_table(Chunk_Ref, Chunk, hash_generic, allocator);
    reserve_hash_table(&result->chunks, 256);

    result->entity_memory = pool_allocator(allocator, ENTITY_CAP + ENTITY_CAP / 2, 256);

    return result;
}

void* _make_entity(Entity_Manager* em, int size, Entity_Type type) {
    assert(sizeof(Entity) <= size);
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

static u64 hash_path_tile(void* a, void* b, int size) {
    assert(sizeof(Path_Tile) == size);

    Path_Tile* a_tile = a;

    if (b) {
        Path_Tile* b_tile = b;
        return tile_ref_eq(a_tile->parent, b_tile->parent);
    }

    return fnv1_hash(a, size);
}

static u64 hash_tile_ref(void* a, void* b, int size) {
    assert(size == sizeof(Tile_Ref));

    Tile_Ref* a_t = a;
    
    if (b) {
        Tile_Ref* b_t = b;

        return tile_ref_eq(*a_t, *b_t);
    }

    return fnv1_hash(a, size);
}

static f32 distance_between_tiles(Tile_Ref a, Tile_Ref b) {
    Vector2 a_xy = v2((f32)a.x, (f32)a.y);
    Vector2 b_xy = v2((f32)b.x, (f32)b.y);
    return v2_len(v2_sub(a_xy, b_xy));
}

static b32 is_tile_traversable(Tile* tile) {
    if (!tile) return false;

    if (tile->type == TT_Open) return false;
    if (tile->content == TC_Wall) return false;

    return true;
}

b32 pathfind(Entity_Manager* em, Tile_Ref source, Tile_Ref dest, Path* path) {
    if (tile_ref_eq(source, dest)) return true;

    Hash_Table came_from = make_hash_table(
        Tile_Ref, 
        Path_Tile, 
        hash_tile_ref, 
        g_platform->permanent_arena
    ); // Key is the tile. Value is the details
    reserve_hash_table(&came_from, 32);
    push_hash_table(&came_from, source, (Path_Tile) { .parent = source });

    Float_Heap open = make_float_heap(g_platform->frame_arena, 32);
    push_min_float_heap(&open, 0.f, came_from.pair_count - 1);

    Hash_Set closed = make_hash_set(Tile_Ref, hash_tile_ref, g_platform->frame_arena);
    reserve_hash_set(&closed, 32);

    f64 time_doing_min = 0.0;
    f64 time_doing_hash = 0.0;
    f64 time_doing_math = 0.0;
    f64 time_doing_entity_manager = 0.0;
    f64 time_wasted = 0.0;

    while (open.count) {
        // Find path tile with lowest f
        f64 min_start = g_platform->time_in_seconds();
        int current_index = pop_min_float_heap(&open);
        Tile_Ref current_ref = (*(Tile_Ref*)key_at_hash_table(&came_from, current_index));
        Path_Tile current_tile = (*(Path_Tile*)find_hash_table(&came_from, current_ref));
        time_doing_min += g_platform->time_in_seconds() - min_start;

        f64 pre_closed_start = g_platform->time_in_seconds();
        push_hash_set(&closed, current_ref);
        time_doing_hash += pre_closed_start - g_platform->time_in_seconds();

        // Check to see if we're at out destination
        if (tile_ref_eq(dest, current_ref)) {
            // If so build our bath by walking up the graph
            int ref_count = 1;
            Tile_Ref final_ref = current_ref;
            Path_Tile final_tile = current_tile;

            while(!tile_ref_eq(final_tile.parent, final_ref)) {
                ref_count += 1;
                final_ref = final_tile.parent;
                final_tile = (*(Path_Tile*)find_hash_table(&came_from, final_ref));
            }

            path->refs = mem_alloc_array(g_platform->permanent_arena, Tile_Ref, ref_count);
            path->ref_count = ref_count;

            final_ref = current_ref;
            final_tile = current_tile;
            path->refs[ref_count - 1] = dest;
            path->came_from = came_from;

            while(!tile_ref_eq(final_tile.parent, final_ref)) {
                ref_count -= 1;
                path->refs[ref_count - 1] = final_ref;
                final_ref = final_tile.parent;
                final_tile = (*(Path_Tile*)find_hash_table(&came_from, final_ref));
            }

            // o_log("spent %.2fms doing hash. spent %.2fms doing min lookup. spent %.2fms doing math. spent %.2fms doing entity manager. wasted %.2fms", time_doing_hash * 1000.f, time_doing_min * 1000.f, time_doing_math * 1000.f, time_doing_entity_manager * 1000.f, time_wasted * 1000.f);

            return true;
        }

        for (int i = 0; i < 9; ++i) {
            f64 n_start = g_platform->time_in_seconds();

            int y = i / 3;
            int x = i - y * 3;
            y--;
            x--;

            if (x == 0 && y == 0) {
                time_wasted += g_platform->time_in_seconds() - n_start;
                continue;
            }

            Tile_Ref neighbor_ref = { current_ref.x + x, current_ref.y + y, current_ref.z };

            f64 em_time = g_platform->time_in_seconds();
            Tile* found_tile = find_tile_by_ref(em, neighbor_ref);
            if (!is_tile_traversable(found_tile)) {
                time_wasted += g_platform->time_in_seconds() - n_start;
                time_doing_entity_manager += g_platform->time_in_seconds() - em_time;
                continue;
            }
            time_doing_entity_manager += g_platform->time_in_seconds() - em_time;

            f64 check_hash_start = g_platform->time_in_seconds();
            if (find_hash_set(&closed, neighbor_ref)) {
                time_wasted += g_platform->time_in_seconds() - n_start;
                time_doing_hash += g_platform->time_in_seconds() - check_hash_start;
                continue;
            }
            time_doing_hash += g_platform->time_in_seconds() - check_hash_start;

            f64 math_start = g_platform->time_in_seconds();

            b32 is_diagonal = abs(x) + abs(y) == 2;

            f32 g = current_tile.g + (is_diagonal ? 1.4f : 1.f);
            f32 h = distance_between_tiles(neighbor_ref, dest);
            f32 f = g + h;
            time_doing_math += g_platform->time_in_seconds() - math_start;

            f64 final_hash = g_platform->time_in_seconds();
            Path_Tile* found_path_tile = find_hash_table(&came_from, neighbor_ref);
            if (!found_path_tile || (found_path_tile && found_path_tile->f > f)) {
                if (!found_path_tile) found_path_tile = push_hash_table(&came_from, neighbor_ref, (Path_Tile) { 0 });

                found_path_tile->f = f;
                found_path_tile->h = h;
                found_path_tile->g = g;
#if DEBUG_BUILD
                found_path_tile->times_touched += 1;
#endif
                found_path_tile->parent = current_ref;

                push_min_float_heap(&open, f, index_hash_table(&came_from, neighbor_ref));
            }
            time_doing_hash += g_platform->time_in_seconds() - final_hash;
        }
    }

    return false;
}

#if DEBUG_BUILD

void draw_pathfind_debug(Entity_Manager* em, Path path) {
    Controller* controller = find_entity_by_id(em, em->controller_id);

    set_shader(find_shader(from_cstr("assets/shaders/font")));
    draw_from(controller->location, controller->current_ortho_size);

    Font_Collection* fc = find_font_collection(from_cstr("assets/fonts/Menlo-Regular"));
    Font* font = font_at_size(fc, 48);
    set_uniform_texture("atlas", font->atlas);

    imm_begin();
    for (int i = 0; i < path.came_from.pair_count; ++i) {
        Tile_Ref* tile_ref = key_at_hash_table(&path.came_from, i);
        Path_Tile* tile = value_at_hash_table(&path.came_from, i);

        Vector2 draw_min = v2((f32)tile_ref->x, (f32)tile_ref->y);
        Rect draw_rect = { draw_min, v2_add(draw_min, v2s(1.f)) };

        f32 r = fmodf(tile->g, 25.f) / 25.f;
        f32 g = 1.f - r;

        imm_rect(draw_rect, -4.f, v4(r, g, 0.f, 0.5f));


        if (controller->current_ortho_size <= 10.f) {
            char buffer[64];
            sprintf(buffer, "F: %.2f\nG: %.2f\nH: %.2f\nTT: %i", tile->f, tile->g, tile->h, tile->times_touched);

            imm_string(from_cstr(buffer), font, 0.2f, 1000.f, v2(draw_min.x, draw_min.y + 1.f - 0.2f), -4.f, v4s(1.f));
        }
    }
    imm_flush();
}

#endif