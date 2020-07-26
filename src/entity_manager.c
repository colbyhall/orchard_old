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
