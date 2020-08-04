#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include "language_layer.h"

typedef enum Wall_Type {
    WT_Steel = 0,
} Wall_Type;

typedef enum Wall_Visual {
    WV_North,
    WV_South,
    WV_East,
    WV_West,
    WV_South_East,
    WV_South_West,
    WV_East_West,
    WV_Cross,

    WV_Count,
} Wall_Visual;

typedef struct Wall {
    Wall_Type type;
    Wall_Visual visual;
} Wall;

typedef enum Tile_Type {
    TT_Open = 0,
    TT_Steel,
} Tile_Type;

static const char* tile_type_names[] = {
    "Open",
    "Steel",
};

typedef enum Tile_Content {
    TC_None = 0,
    TC_Wall
} Tile_Content;

typedef struct Tile_Ref {
    int x, y, z;
} Tile_Ref;

inline Tile_Ref tile_ref_from_location(Vector2 a) { return (Tile_Ref) { (int)a.x, (int)a.y}; }
inline b32 tile_ref_eq(Tile_Ref a, Tile_Ref b) { 
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

typedef struct Tile {
    Tile_Type type;
    Tile_Content content;
    union {
        Wall wall;
    };
} Tile;

typedef struct Chunk_Ref {
    int x, y;
} Chunk_Ref;

#define CHUNK_SIZE 16
typedef struct Chunk {
    int x, y, z;
    Tile* tiles; // Allocated elsewhere because we scan chunks for their pos in a list
} Chunk;

typedef u32 Entity_Id; // Invalid Entity_Id is 0

typedef enum Entity_Type {
    ET_Controller,
    ET_Pawn,
} Entity_Type;

#define ENTITY_STRUCT(entry) \
entry(Entity_Id, id) \
entry(Entity_Type, type) \
entry(Vector2, location) \
entry(f32, rotation) \
entry(Rect, bounds) \
entry(void*, derived)

#define DEFINE_ENTITY_STRUCT(t, n) t n;

typedef struct Entity {
    ENTITY_STRUCT(DEFINE_ENTITY_STRUCT)
} Entity;

#define DEFINE_CHILD_ENTITY union { \
    struct { \
        ENTITY_STRUCT(DEFINE_ENTITY_STRUCT) \
    }; \
    Entity base; \
}

#define CHUNK_CAP 256
#define ENTITY_CAP (CHUNK_SIZE * CHUNK_SIZE * CHUNK_CAP)
typedef struct Entity_Manager {
    Hash_Table chunks;
    Allocator tile_memory;

    int entity_count;
    Entity* entities[ENTITY_CAP];
    Entity_Id last_entity_id;

    Entity_Id controller_id;

    Allocator entity_memory;
} Entity_Manager;

typedef struct Entity_Iterator {
    Entity_Manager* manager;
    int found_entity_count;
    int index;
} Entity_Iterator;

Entity_Iterator make_entity_iterator(Entity_Manager* manager);
b32 can_step_entity_iterator(Entity_Iterator iter);
void step_entity_iterator(Entity_Iterator* iter);

#define entity_iterator(em) Entity_Iterator iter = make_entity_iterator(em); can_step_entity_iterator(iter); step_entity_iterator(&iter)

inline Entity* entity_from_iterator(Entity_Iterator iter) { return iter.manager->entities[iter.index]; }

void* find_entity_by_id(Entity_Manager* em, Entity_Id id);
Tile* find_tile_at(Entity_Manager* em, int x, int y, int z);
inline Tile* find_tile_by_ref(Entity_Manager* em, Tile_Ref ref) { return find_tile_at(em, ref.x, ref.y, ref.z); }
Entity_Manager* make_entity_manager(Allocator allocator);

void* _make_entity(Entity_Manager* em, int size, Entity_Type type);
#define make_entity(em, type) _make_entity(em, sizeof(type), ET_ ## type)

#define ENTITY_FUNCTIONS(entry) \
entry(ET_Controller, tick_controller, draw_null) \
entry(ET_Pawn, tick_pawn, draw_pawn) 

void tick_null(Entity_Manager* em, Entity* entity, f32 dt) { }
void draw_null(Entity_Manager* em, Entity* entity) { }

typedef struct Path_Tile {
    Tile_Ref parent;
    f32 f, g, h;
#if DEBUG_BUILD
    int times_touched;
    b32 closed;
#endif
} Path_Tile;


typedef struct Path {
    Tile_Ref* refs;
    int ref_count;

#if DEBUG_BUILD
    Hash_Table came_from;
#endif
} Path;

/**
 * A* Pathfinding
 */
b32 pathfind(Entity_Manager* em, Tile_Ref source, Tile_Ref dest, Path* path);

#if DEBUG_BUILD

void draw_pathfind_debug(Entity_Manager* em, Path path);

#endif

#endif /* ENTITY_MANAGER_H */