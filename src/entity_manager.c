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

b32 can_step_cell_rect_iterator(Cell_Rect_Iterator iter) {
    int dist_x = iter.p1.x - iter.p0.x + 1;
    int dist_y = iter.p1.y - iter.p0.y + 1;

    return iter.index < dist_x * dist_y;
}

Cell_Ref ref_from_rect_iterator(Cell_Rect_Iterator iter) {
    int dist_x = iter.p1.x - iter.p0.x + 1;
    int dist_y = iter.p1.y - iter.p0.y + 1;

    int y = iter.index / dist_x;
    int x = iter.index - y * dist_x;

    return (Cell_Ref) { iter.p0.x + x, iter.p0.y + y };
}

void* find_entity_by_id(Entity_Manager* em, Entity_Id id) {
    for (entity_iterator(em)) {
        Entity* entity = entity_from_iterator(iter);

        if (entity->id == id) return entity->derived;
    }

    return 0;
}

Cell* find_cell_at(Entity_Manager* em, int x, int y) {
    if (x > CHUNK_SIZE * WORLD_SIZE || x < 0) return 0;
    if (y > CHUNK_SIZE * WORLD_SIZE || y < 0) return 0;

    int chunk_x = x / CHUNK_SIZE;
    int chunk_y = y / CHUNK_SIZE;

    Chunk* chunk = &em->chunks[chunk_x + chunk_y * WORLD_SIZE];

    int local_x = x - chunk_x * CHUNK_SIZE;
    int local_y = y - chunk_y * CHUNK_SIZE;
    assert(local_x <= CHUNK_SIZE && local_y <= CHUNK_SIZE);
    return &chunk->cells[local_x + local_y * CHUNK_SIZE];
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

static void refresh_wall_visual(Entity_Manager* em, int x, int y, b32 first) {
    Cell* cell = find_cell_at(em, x, y);
    if (!cell) return;
    if (cell->content != CC_Wall && !first) return;

    b32 has_north = false;
    Cell* north = find_cell_at(em, x, y + 1);
    if (north && north->content == CC_Wall) {
        if (first) refresh_wall_visual(em, x, y + 1, false);
        has_north = true;
    }

    b32 has_south = false;
    Cell* south = find_cell_at(em, x, y - 1);
    if (south && south->content == CC_Wall) {
        if (first) refresh_wall_visual(em, x, y - 1, false);
        has_south = true;
    }

    b32 has_east = false;
    Cell* east = find_cell_at(em, x + 1, y);
    if (east && east->content == CC_Wall) {
        if (first) refresh_wall_visual(em, x + 1, y, false);
        has_east = true;
    }

    b32 has_west = false;
    Cell* west = find_cell_at(em, x - 1, y);
    if (west && west->content == CC_Wall) {
        if (first) refresh_wall_visual(em, x - 1, y, false);
        has_west = true;
    }

    if (cell->content != CC_Wall && first) return;

    int num_surrounding = has_north + has_south + has_east + has_west;
    switch (num_surrounding) {
    case 0: 
        cell->wall.visual = WV_South;
        break;
    case 1: {
        Wall_Visual visual    = WV_South;
        if (has_south) visual = WV_North;
        if (has_east) visual  = WV_West;
        if (has_west) visual  = WV_East;
        cell->wall.visual = visual;
    } break;
    case 2: {
        Wall_Visual visual = WV_South;
        if (has_south && has_north) visual = WV_North;
        if (has_east && has_west)   visual = WV_East_West;
        if (has_east && has_north)  visual = WV_West;
        if (has_west && has_north)  visual = WV_East;
        if (has_east && has_south)  visual = WV_South_East;
        if (has_west && has_south)  visual = WV_South_West;
        cell->wall.visual = visual;
    } break;
    case 3: {
        Wall_Visual visual = WV_Cross;
        if (has_north && !has_south) visual = WV_East_West;
        if (has_north && has_south && has_east) visual = WV_South_East;
        if (has_north && has_south && has_west) visual = WV_South_West;
        cell->wall.visual = visual;
    } break;
    case 4:
        cell->wall.visual = WV_Cross;
        break;
    }
}

static u64 hash_path_cell(void* a, void* b, int size) {
    assert(sizeof(Path_Cell) == size);

    Path_Cell* a_cell = a;

    if (b) {
        Path_Cell* b_cell = b;
        return cell_ref_equals(a_cell->parent, b_cell->parent);
    }

    return fnv1_hash(a, size);
}

static u64 hash_cell_ref(void* a, void* b, int size) {
    assert(size == sizeof(Cell_Ref));
 
    Cell_Ref* a_t = a;
    
    if (b) {
        Cell_Ref* b_t = b;

        return cell_ref_equals(*a_t, *b_t);
    }

    return fnv1_hash(a, size);
}

static f32 pathfind_heuristic(Cell_Ref a, Cell_Ref b) {
    int dx = abs(b.x - a.x);
    int dy = abs(b.y - a.y);

    return (f32)(dx + dy);
}

static f32 distance_between_cells(Cell_Ref a, Cell_Ref b) {
    Vector2 a_xy = v2((f32)a.x, (f32)a.y);
    Vector2 b_xy = v2((f32)b.x, (f32)b.y);
    return v2_len(v2_sub(a_xy, b_xy));
}

static b32 is_cell_traversable(Cell* cell) {
    if (!cell) return false;
    if (cell->content != CFT_None) return false;

    return true;
}

static Path_Cell* find_path_cell(Path_Map* map, Cell_Ref ref) { 
    Path_Cell* cell = &map->cells[ref.x + ref.y * CHUNK_SIZE * WORLD_SIZE];
    if (!cell->is_initialized) return 0;
    
#if DEBUG_BUILD
    cell->times_touched += 1;
#endif

    return cell; 
}

static Path_Cell* set_path_cell(Path_Map* map, Cell_Ref ref, b32 is_passable) {
    map->num_discovered += 1;

    Path_Cell* cell = &map->cells[ref.x + ref.y * CHUNK_SIZE * WORLD_SIZE];    
    cell->is_initialized = true;
    cell->parent = ref;
    cell->is_passable = is_passable;
    cell->f = 1000000000.f;

#if DEBUG_BUILD
    cell->times_touched += 1;
#endif

    return cell;    
}

inline int cell_ref_to_index(Cell_Ref ref) { return ref.x + ref.y * CHUNK_SIZE * WORLD_SIZE; }
inline Cell_Ref cell_ref_from_index(int index) { 
    int y = index / (CHUNK_SIZE * WORLD_SIZE);
    int x = index - y * CHUNK_SIZE * WORLD_SIZE;
    return (Cell_Ref) { x, y };
}

// @TODO: There are still a number of optimizations we can make here
//  1. Path map should use some iteration counter to keep track of what path cell has been visited
//  2. Jump point search needs to be implemented
//  3. Looser heuristic function that sacrifices smallest path for speed
b32 pathfind(Entity_Manager* em, Cell_Ref source, Cell_Ref dest, Path* path) {
    if (cell_ref_equals(source, dest)) return true;

    Cell* source_cell = find_cell_by_ref(em, source);
    if (!source_cell || !is_cell_traversable(source_cell)) return false;

    Cell* dest_cell = find_cell_by_ref(em, dest);
    if (!dest_cell || !is_cell_traversable(dest_cell)) return false;

    int cell_count = CHUNK_SIZE * CHUNK_SIZE * WORLD_SIZE * WORLD_SIZE;
    Path_Map path_map = { .cells = mem_alloc_array(g_platform->permanent_arena, Path_Cell, cell_count), };
    // mem_set(path_map.cells, 0, sizeof(Path_Cell) * cell_count); This is super expensive

    set_path_cell(&path_map, source, false);

    Float_Heap open = make_float_heap(g_platform->frame_arena, 32);
    push_min_float_heap(&open, 0.f, cell_ref_to_index(source));

    b32* closed = mem_alloc_array(g_platform->frame_arena, b32, cell_count);
    mem_set(closed, 0, sizeof(b32) * cell_count);

    static int neighbor_map[] = {
         -1,  0,
          1,  0,
          0, -1,
          0,  1,

         -1, -1,
         -1,  1,
          1, -1,
          1,  1,
    };

    while (open.count) {
        // Find path cell with lowest f
        int current_index      = pop_min_float_heap(&open);
        Path_Cell current_cell = path_map.cells[current_index];
        Cell_Ref current_ref   = cell_ref_from_index(current_index);

        closed[current_index] = true;

        // Check to see if we're at out destination
        if (cell_ref_equals(dest, current_ref)) {
            // If so build our bath by walking up the graph
            int point_count = 0;
            Cell_Ref final_ref = current_ref;
            Path_Cell final_cell = current_cell;

            while(!cell_ref_equals(final_cell.parent, final_ref)) {
                point_count += 1;
                final_ref = final_cell.parent;
                final_cell = *find_path_cell(&path_map, final_ref);
            }

            path->points = mem_alloc_array(g_platform->permanent_arena, Cell_Ref, point_count);
            path->point_count = point_count;

            final_ref = current_ref;
            final_cell = current_cell;
            path->points[point_count - 1] = dest;

            while(!cell_ref_equals(final_cell.parent, final_ref)) {
                point_count -= 1;
                path->points[point_count] = final_ref;
                final_ref = final_cell.parent;
                final_cell = *find_path_cell(&path_map, final_ref);
            }

            return true;
        }

        for (int i = 0; i < array_count(neighbor_map) / 2; ++i) {
            int x = neighbor_map[i * 2];
            int y = neighbor_map[i * 2 + 1];

            Cell_Ref neighbor_ref = { current_ref.x + x, current_ref.y + y };

            // If we haven't initialized our path_cell proxy then do so
            Path_Cell* path_cell = find_path_cell(&path_map, neighbor_ref);
            if (!path_cell) {
                Cell* cell = find_cell_by_ref(em, neighbor_ref);
                if (!cell) continue;

                path_cell = set_path_cell(&path_map, neighbor_ref, is_cell_traversable(cell));
            }

            int neighbor_index = cell_ref_to_index(neighbor_ref);

            b32 is_diagonal = i >= array_count(neighbor_map) / 4;
            b32 is_passable = path_cell->is_passable;
            if (is_diagonal && is_passable) {
                Cell_Ref a_ref = { current_ref.x + x, current_ref.y };
                Cell_Ref b_ref = { current_ref.x, current_ref.y + y};
                
                Path_Cell* a_cell = find_path_cell(&path_map, a_ref);
                Path_Cell* b_cell = find_path_cell(&path_map, b_ref);

                is_passable = a_cell->is_passable && b_cell->is_passable;
            }

            // If we're not passable or we're on the closed list try another neighbor
            if (!is_passable || closed[neighbor_index]) continue;

            // Do the math!
            f32 g = current_cell.g + (is_diagonal ? 1.41f : 1.f);
            f32 h = pathfind_heuristic(neighbor_ref, dest);
            f32 f = g + h;

            if (path_cell->f > f) {
                path_cell->f = f;
                path_cell->h = h;
                path_cell->g = g;
                path_cell->parent = current_ref;

                push_min_float_heap(&open, f, neighbor_index);
            }
        }
    }

    return false;
}

#if 0

void draw_pathfind_debug(Entity_Manager* em, Path path) {
    if (!path.path_map.cells) return;

    Controller* controller = find_entity_by_id(em, em->controller_id);

    Vector2 mouse_pos_in_world = get_mouse_pos_in_world_space(controller);
    Cell_Ref mouse_cell = cell_ref_from_location(mouse_pos_in_world);

    set_shader(find_shader(from_cstr("assets/shaders/font")));
    draw_from(controller->location, controller->current_ortho_size);

    Font_Collection* fc = find_font_collection(from_cstr("assets/fonts/Menlo-Regular"));
    Font* font = font_at_size(fc, 48);
    set_uniform_texture("atlas", font->atlas);

    imm_begin();
    for (int i = 0; i < CHUNK_SIZE * CHUNK_SIZE * WORLD_SIZE * WORLD_SIZE; ++i) {
        Path_Cell cell = path.path_map.cells[i];
        if (!cell.is_initialized) continue;

        Cell_Ref ref = cell_ref_from_index(i);

        Vector2 draw_min = v2((f32)ref.x, (f32)ref.y);
        Rect draw_rect = { draw_min, v2_add(draw_min, v2s(1.f)) };

        f32 r = (cell.times_touched) / 20.f;
        f32 g = cell.g / cell.f;
        f32 b = cell.h / cell.f;

        imm_rect(draw_rect, -4.f, v4(r, g, b, 0.5f));

        f32 dist_between_cells = distance_between_cells(mouse_cell, ref);
        if (dist_between_cells < 5.f && controller->current_ortho_size <= 15.f) {
            char buffer[64];
            sprintf(buffer, "F: %.2f\nG: %.2f\nH: %.2f\nTT: %i", cell.f, cell.g, cell.h, cell.times_touched);

            imm_string(from_cstr(buffer), font, 0.2f, 1000.f, v2(draw_min.x, draw_min.y + 1.f - 0.2f), -4.f, v4s(1.f));

        }
    }
    imm_flush();
}

#endif