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

typedef enum Cell_Content {
    CC_None,
    CC_Wall,
    CC_Entity,
} Cell_Content;

typedef enum Cell_Floor_Type {
    CFT_None,
    CFT_Steel_Panel,
} Cell_Floor_Type;

struct Entity;

typedef struct Cell {
    b32 has_frame;
    Cell_Floor_Type floor_type;

    Cell_Content content;
    union {
        Wall wall;
        struct Entity* entity;
    };
} Cell;

typedef struct Cell_Ref { 
    int x, y; 
} Cell_Ref;

inline Cell_Ref cell_ref_from_location(Vector2 location) { return (Cell_Ref) { (int)location.x, (int)location.y }; }
inline b32 cell_ref_equals(Cell_Ref a, Cell_Ref b) { return a.x == b.x && a.y == b.y; }
inline Rect rect_from_cell(Cell_Ref a) { return (Rect) { v2((f32)a.x, (f32)a.y), v2((f32)a.x + 1.f, (f32)a.y + 1.f) }; }

#define CHUNK_SIZE 16
#define CELLS_PER_CHUNK (CHUNK_SIZE * CHUNK_SIZE)
typedef struct Chunk {
    Cell cells[CELLS_PER_CHUNK];
} Chunk;

typedef u32 Entity_Id; // Invalid Entity_Id is 0

typedef enum Entity_Type {
    ET_Controller,
    ET_Pawn,
    ET_Furniture,
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

#define WORLD_SIZE 16
#define CHUNK_CAP (WORLD_SIZE * WORLD_SIZE)
#define ENTITY_CAP (CHUNK_SIZE * CHUNK_SIZE * WORLD_SIZE * WORLD_SIZE)
typedef struct Entity_Manager {
    Chunk chunks[CHUNK_CAP];
    Allocator cell_memory;

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

typedef struct Cell_Rect_Iterator {
    Cell_Ref p0, p1;
    int index;
} Cell_Rect_Iterator;

b32 can_step_cell_rect_iterator(Cell_Rect_Iterator iter);
Cell_Ref ref_from_rect_iterator(Cell_Rect_Iterator iter);
#define cell_rect_iterator(p0, p1) Cell_Rect_Iterator iter = { p0, p1, 0 }; can_step_cell_rect_iterator(iter); ++iter.index

void* find_entity_by_id(Entity_Manager* em, Entity_Id id);
Cell* find_cell_at(Entity_Manager* em, int x, int y);
Cell* find_cell_by_ref(Entity_Manager* em, Cell_Ref ref) { return find_cell_at(em, ref.x, ref.y); }
Entity_Manager* make_entity_manager(Allocator allocator);

void* _make_entity(Entity_Manager* em, int size, Entity_Type type);
#define make_entity(em, type) _make_entity(em, sizeof(type), ET_ ## type)

#define ENTITY_FUNCTIONS(entry) \
entry(ET_Controller, tick_controller, draw_null) \
entry(ET_Pawn, tick_pawn, draw_pawn) \
entry(ET_Furniture, tick_furniture, draw_furniture) \

void tick_null(Entity_Manager* em, Entity* entity, f32 dt) { }
void draw_null(Entity_Manager* em, Entity* entity) { }

typedef struct Path_Cell {
    b32 is_initialized;
    Cell_Ref parent;
    f32 f, g, h;
    b32 is_passable;

#if DEBUG_BUILD
    int times_touched;
#endif
} Path_Cell;

typedef struct Path_Map {
    int num_discovered;
    Path_Cell* cells;
} Path_Map;

typedef struct Path {
    Cell_Ref* points;
    int point_count;
} Path;

/**
 * A* Pathfinding
 */
b32 pathfind(Entity_Manager* em, Cell_Ref source, Cell_Ref dest, Path* path);

#if 0

void draw_pathfind_debug(Entity_Manager* em, Path path);

#endif

#endif /* ENTITY_MANAGER_H */