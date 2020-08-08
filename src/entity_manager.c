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
    if (x > CHUNK_SIZE * WORLD_SIZE || x < 0) return 0;
    if (y > CHUNK_SIZE * WORLD_SIZE || y < 0) return 0;

    int chunk_x = x / CHUNK_SIZE;
    int chunk_y = y / CHUNK_SIZE;

    Chunk_Ref ref = { chunk_x, chunk_y };

    Chunk chunk = em->chunks[chunk_x + chunk_y * WORLD_SIZE];

    int local_x = x - chunk_x * CHUNK_SIZE;
    int local_y = y - chunk_y * CHUNK_SIZE;
    assert(local_x <= CHUNK_SIZE && local_y <= CHUNK_SIZE);
    return &chunk.tiles[local_x + local_y * CHUNK_SIZE];
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

static u64 hash_chunk_ref(void* a, void* b, int size) {
    assert(sizeof(Chunk_Ref) == size);

    Chunk_Ref* a_ref = a;
    if (b) {
        Chunk_Ref* b_ref = b;
            
        return a_ref->x == b_ref->x && a_ref->y == b_ref->y;
    }

    return fnv1_hash(a, size);
}

Entity_Manager* make_entity_manager(Allocator allocator) {
    Entity_Manager* result = mem_alloc_struct(allocator, Entity_Manager);

    result->tile_memory = allocator;
    for (int x = 0; x < WORLD_SIZE; ++x) {
        for (int y = 0; y < WORLD_SIZE; ++y) {
            Chunk chunk = {
                x, y,
                mem_alloc_array(result->tile_memory, Tile, CHUNK_SIZE * CHUNK_SIZE)
            };
            result->chunks[x + y * WORLD_SIZE] = chunk;
        }
    }

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

static f32 pathfind_heuristic(Tile_Ref a, Tile_Ref b) {
    int dx = abs(b.x - a.x);
    int dy = abs(b.y - a.y);

    return (f32)(dx + dy);
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

static Path_Tile* find_path_tile(Path_Map* map, Tile_Ref ref) { 
    Path_Tile* tile = &map->tiles[ref.x + ref.y * CHUNK_SIZE * WORLD_SIZE];
    if (!tile->is_initialized) return 0;
    
#if DEBUG_BUILD
    tile->times_touched += 1;
#endif

    return tile; 
}

static Path_Tile* set_path_tile(Path_Map* map, Tile_Ref ref, b32 is_passable) {
    map->num_discovered += 1;

    Path_Tile* tile = &map->tiles[ref.x + ref.y * CHUNK_SIZE * WORLD_SIZE];    
    tile->is_initialized = true;
    tile->parent = ref;
    tile->is_passable = is_passable;
    tile->f = 1000000000000.f;

#if DEBUG_BUILD
    tile->times_touched += 1;
#endif

    return tile;    
}

inline int tile_ref_to_index(Tile_Ref ref) { return ref.x + ref.y * CHUNK_SIZE * WORLD_SIZE; }
inline Tile_Ref tile_ref_from_index(int index) { 
    int y = index / (CHUNK_SIZE * WORLD_SIZE);
    int x = index - y * CHUNK_SIZE * WORLD_SIZE;
    return (Tile_Ref) { x, y };
}

// There are some optimizations i want to do here.
//
// 1. Store Path_Tile similar to how we store tiles in the entity manager. We'll have chunks that we can easily look into 
// 2. Path_Tile needs to store if the tile is traversable. The goal is to get down the number of times we check tiles from the entity manager as that can be very expensive
// 3. Looser heuristic function that sacrifices smallest path for speed
b32 pathfind(Entity_Manager* em, Tile_Ref source, Tile_Ref dest, Path* path) {
    if (tile_ref_eq(source, dest)) return true;

    Tile* source_tile = find_tile_by_ref(em, source);
    if (!source_tile || !is_tile_traversable(source_tile)) return false;

    Tile* dest_tile = find_tile_by_ref(em, dest);
    if (!dest_tile || !is_tile_traversable(dest_tile)) return false;

    int tile_count = CHUNK_SIZE * CHUNK_SIZE * WORLD_SIZE * WORLD_SIZE;
    Path_Map path_map = { .tiles = mem_alloc_array(g_platform->permanent_arena, Path_Tile, tile_count), };
    // mem_set(path_map.tiles, 0, sizeof(Path_Tile) * tile_count);

    set_path_tile(&path_map, source, false);

    Float_Heap open = make_float_heap(g_platform->frame_arena, 32);
    push_min_float_heap(&open, 0.f, tile_ref_to_index(source));

    b32* closed = mem_alloc_array(g_platform->frame_arena, b32, tile_count);
    mem_set(closed, 0, sizeof(b32) * tile_count);

    f64 time_doing_min = 0.0;
    f64 time_wasted = 0.0;
    f64 time_doing_neighbor = 0.0;
    int tiles_checked = 0;

    int neighbor_map[] = {
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
        tiles_checked += 1;
        f64 min_start = g_platform->time_in_seconds();

        // Find path tile with lowest f
        int current_index      = pop_min_float_heap(&open);
        Path_Tile current_tile = path_map.tiles[current_index];
        Tile_Ref current_ref   = tile_ref_from_index(current_index);
        time_doing_min += g_platform->time_in_seconds() - min_start;

        closed[current_index] = true;

        // Check to see if we're at out destination
        if (tile_ref_eq(dest, current_ref)) {
            // If so build our bath by walking up the graph
            int ref_count = 0;
            Tile_Ref final_ref = current_ref;
            Path_Tile final_tile = current_tile;

            while(!tile_ref_eq(final_tile.parent, final_ref)) {
                ref_count += 1;
                final_ref = final_tile.parent;
                final_tile = *find_path_tile(&path_map, final_ref);
            }

            path->refs = mem_alloc_array(g_platform->permanent_arena, Tile_Ref, ref_count);
            path->ref_count = ref_count;

            final_ref = current_ref;
            final_tile = current_tile;
            path->refs[ref_count - 1] = dest;
#if DEBUG_BUILD
            path->path_map = path_map;
#endif

            while(!tile_ref_eq(final_tile.parent, final_ref)) {
                ref_count -= 1;
                path->refs[ref_count] = final_ref;
                final_ref = final_tile.parent;
                final_tile = *find_path_tile(&path_map, final_ref);
            }

            o_log(
                "[Pathfinding] spent %.3fms doing min lookup. wasted %.3fms. checked %i tiles, time doing neighbor %.3fms", 
                time_doing_min * 1000.0, 
                time_wasted * 1000.0, 
                tiles_checked, 
                time_doing_neighbor * 1000.0
            );

            return true;
        }

        for (int i = 0; i < array_count(neighbor_map) / 2; ++i) {
            f64 n_start = g_platform->time_in_seconds();

            int x = neighbor_map[i * 2];
            int y = neighbor_map[i * 2 + 1];

            tiles_checked += 1;

            Tile_Ref neighbor_ref = { current_ref.x + x, current_ref.y + y };

            // If we haven't initialized our path_tile proxy then do so
            Path_Tile* path_tile = find_path_tile(&path_map, neighbor_ref);
            if (!path_tile) {
                Tile* tile = find_tile_by_ref(em, neighbor_ref);
                if (!tile) {
                    time_wasted += g_platform->time_in_seconds() - n_start;
                    time_doing_neighbor += g_platform->time_in_seconds() - n_start;
                    continue;
                }

                path_tile = set_path_tile(&path_map, neighbor_ref, is_tile_traversable(tile));
            }

            int neighbor_index = tile_ref_to_index(neighbor_ref);

            b32 is_diagonal = i > array_count(neighbor_map) / 4;
            b32 is_passable = path_tile->is_passable;
            if (is_diagonal && is_passable) {
                Tile_Ref a_ref = { current_ref.x + x, current_ref.y };
                Tile_Ref b_ref = { current_ref.x, current_ref.y + y};
                
                Path_Tile* a_tile = find_path_tile(&path_map, a_ref);
                Path_Tile* b_tile = find_path_tile(&path_map, b_ref);

                is_passable = a_tile->is_passable && b_tile->is_passable;
            }

            // If we're not passable or we're on the closed list try another neighbor
            if (!is_passable || closed[neighbor_index]) {
                time_wasted += g_platform->time_in_seconds() - n_start;
                time_doing_neighbor += g_platform->time_in_seconds() - n_start;
                continue;
            }

            // Do the math!
            f32 g = current_tile.g + (is_diagonal ? 1.4f : 1.f);
            f32 h = pathfind_heuristic(neighbor_ref, dest);
            f32 f = g + h;

            if (path_tile->f > f) {
                path_tile->f = f;
                path_tile->h = h;
                path_tile->g = g;
                path_tile->parent = current_ref;

                f64 pre_push_min_heap = g_platform->time_in_seconds();
                push_min_float_heap(&open, f, neighbor_index);
                time_doing_min += g_platform->time_in_seconds() - pre_push_min_heap;
            }
            time_doing_neighbor += g_platform->time_in_seconds() - n_start;
        }
    }

    return false;
}

#if DEBUG_BUILD

void draw_pathfind_debug(Entity_Manager* em, Path path) {
    if (!path.path_map.tiles) return;

    Controller* controller = find_entity_by_id(em, em->controller_id);

    Vector2 mouse_pos_in_world = get_mouse_pos_in_world_space(controller);
    Tile_Ref mouse_tile = tile_ref_from_location(mouse_pos_in_world);

    set_shader(find_shader(from_cstr("assets/shaders/font")));
    draw_from(controller->location, controller->current_ortho_size);

    Font_Collection* fc = find_font_collection(from_cstr("assets/fonts/Menlo-Regular"));
    Font* font = font_at_size(fc, 48);
    set_uniform_texture("atlas", font->atlas);

    imm_begin();
    for (int i = 0; i < CHUNK_SIZE * CHUNK_SIZE * WORLD_SIZE * WORLD_SIZE; ++i) {
        Path_Tile tile = path.path_map.tiles[i];
        if (!tile.is_initialized) continue;

        Tile_Ref ref = tile_ref_from_index(i);

        Vector2 draw_min = v2((f32)ref.x, (f32)ref.y);
        Rect draw_rect = { draw_min, v2_add(draw_min, v2s(1.f)) };

        f32 r = (tile.times_touched) / 20.f;
        f32 g = tile.g / tile.f;
        f32 b = tile.h / tile.f;

        imm_rect(draw_rect, -4.f, v4(r, g, b, 0.5f));

        f32 dist_between_tiles = distance_between_tiles(mouse_tile, ref);
        if (dist_between_tiles < 5.f && controller->current_ortho_size <= 15.f) {
            char buffer[64];
            sprintf(buffer, "F: %.2f\nG: %.2f\nH: %.2f\nTT: %i", tile.f, tile.g, tile.h, tile.times_touched);

            imm_string(from_cstr(buffer), font, 0.2f, 1000.f, v2(draw_min.x, draw_min.y + 1.f - 0.2f), -4.f, v4s(1.f));

        }
    }
    imm_flush();
}

#endif