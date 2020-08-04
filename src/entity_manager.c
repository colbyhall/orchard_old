#include "entity_manager.h"

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

    if (x < 0 ||  y < 0) return 0;

    for (int i = 0; i < em->chunk_count; ++i) {
        Chunk chunk = em->chunks[i];
        if (chunk.x == chunk_x && chunk.y == chunk_y && chunk.z == z) {
            int local_x = x - chunk_x * CHUNK_SIZE;
            int local_y = y - chunk_y * CHUNK_SIZE;
            assert(local_x <= CHUNK_SIZE && local_y <= CHUNK_SIZE);
            return &chunk.tiles[local_x + local_y * CHUNK_SIZE];
        }
    }

    return 0;
}

Entity_Manager* make_entity_manager(Allocator allocator) {
    Entity_Manager* result = mem_alloc_struct(allocator, Entity_Manager);

    result->tile_memory = allocator;
    result->chunk_count = CHUNK_CAP;

    for (int i = 0; i < result->chunk_count; ++i) {
        Chunk* chunk = &result->chunks[i];

        chunk->tiles = mem_alloc_array(allocator, Tile, CHUNK_SIZE * CHUNK_SIZE);
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

typedef struct Path_Tile {
    Tile_Ref parent;
    f32 f, g, h;
} Path_Tile;

static u64 hash_path_tile(void* a, void* b, int size) {
    assert(sizeof(Path_Tile) == size);

    Path_Tile* a_tile = a;

    if (b) {
        Path_Tile* b_tile = b;
        return tile_ref_eq(a_tile->parent, b_tile->parent);
    }

    return fnv1_hash(a, size);
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

static f32 distance_between_tiles(Tile_Ref a, Tile_Ref b) {
    return (f32)(abs(b.x - a.x) + abs(b.y - a.y));

    // Vector2 a_xy = v2((f32)a.x, (f32)a.y);
    // Vector2 b_xy = v2((f32)b.x, (f32)b.y);
    // return v2_len(v2_sub(a_xy, b_xy));
}

static b32 is_tile_walkable(Tile* tile) {
    if (!tile) return false;

    if (tile->type == TT_Open) return false;
    if (tile->content == TC_Wall) return false;

    return true;
}

b32 pathfind(Entity_Manager* em, Tile_Ref source, Tile_Ref dest, Path* path) {
    if (tile_ref_eq(source, dest)) return true;

    Hash_Set open = make_hash_set(
        Tile_Ref,
        hash_generic, 
        g_platform->frame_arena
    );
    reserve_hash_set(&open, 4096);

    Hash_Set closed = make_hash_set(
        Tile_Ref, 
        hash_generic, 
        g_platform->frame_arena
    );
    reserve_hash_set(&closed, 4096);

    Hash_Table came_from = make_hash_table(
        Tile_Ref, 
        Path_Tile, 
        hash_generic, 
        g_platform->frame_arena
    ); // Key is the tile. Value is the details
    reserve_hash_table(&came_from, 4096);

    push_hash_set(&open, source);
    push_hash_table(&came_from, source, (Path_Tile) { .parent = source });

    while (open.pair_count) {
        // Find path tile with lowest f

        Tile_Ref current_ref = (*(Tile_Ref*)key_at_hash_table(&open, 0));
        Path_Tile* current_tile = find_hash_table(&came_from, current_ref);
        for (int i = 1; i < open.pair_count; ++i) {
            Tile_Ref found_ref = (*(Tile_Ref*)key_at_hash_table(&open, i));
            Path_Tile* found_tile = find_hash_table(&came_from, found_ref);

            if (current_tile->f > found_tile->f) {
                current_tile = found_tile;
                current_ref = found_ref;
            }
        }

        push_hash_set(&closed, current_ref);
        b32 ok = remove_hash_set(&open, current_ref);
        assert(ok);

        for (int i = 0; i < 9; ++i) {
            int y = i / 3;
            int x = i - y * 3;
            y--;
            x--;

            if (x == 0 && y == 0) continue;

            Tile_Ref neighbor_ref = { current_ref.x + x, current_ref.y + y, current_ref.z };

            Tile* found_tile = find_tile_by_ref(em, neighbor_ref);
            if (!found_tile) continue;

            if (tile_ref_eq(dest, neighbor_ref)) {
                // Do the path tracing
                int ref_count = 1;
                Tile_Ref final_ref = current_ref;
                Path_Tile* final_tile = current_tile;

                while(!tile_ref_eq(final_tile->parent, final_ref)) {
                    ref_count += 1;
                    final_ref = final_tile->parent;
                    final_tile = find_hash_table(&came_from, final_ref);
                }

                path->refs = mem_alloc_array(g_platform->permanent_arena, Tile_Ref, ref_count);
                path->ref_count = ref_count;

                final_ref = current_ref;
                final_tile = current_tile;
                path->refs[ref_count - 1] = dest;

                while(!tile_ref_eq(final_tile->parent, final_ref)) {
                    ref_count -= 1;
                    path->refs[ref_count - 1] = final_ref;
                    final_ref = final_tile->parent;
                    final_tile = find_hash_table(&came_from, final_ref);
                }

                return true;
            }

            if (find_hash_set(&closed, neighbor_ref) == 0 && is_tile_walkable(found_tile)) {
                b32 is_diagonal = (x + y) % 2 == 0;

                if (is_diagonal) {
                    Tile* tile_a = find_tile_at(em, x, 0, 0);
                    Tile* tile_b = find_tile_at(em, 0, y, 0);
                    if (!is_tile_walkable(tile_a) && !is_tile_walkable(tile_b)) continue;
                }

                f32 g = current_tile->g + (is_diagonal ? 1.44f : 1.f);
                f32 h = distance_between_tiles(neighbor_ref, dest);
                f32 f = g + h;

                Path_Tile* found_path_tile = find_hash_table(&came_from, neighbor_ref);
                if (!found_path_tile || (found_path_tile && found_path_tile->f > f)) {
                    if (!found_path_tile) found_path_tile = push_hash_table(&came_from, neighbor_ref, (Path_Tile) {0});

                    found_path_tile->f = f;
                    found_path_tile->h = h;
                    found_path_tile->g = g;
                    found_path_tile->parent = current_ref;

                    push_hash_set(&open, neighbor_ref);
                }
            }
        }
    }

    return false;
}
