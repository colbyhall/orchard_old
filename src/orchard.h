#ifndef ORCHARD_H
#define ORCHARD_H

#include "math.h"

typedef enum Tile_Type {
    TT_Air = 0,
    TT_Grass,
    TT_Dirt,
    TT_Water,
    TT_Count,
} Tile_Type;

typedef struct Tile {
    Tile_Type type;
} Tile;

enum Chunk_Flags {
    CF_NeedsTick = (1 << 0),
};

typedef u32 Chunk_Id;
#define CHUNK_SIZE 16
typedef struct Chunk {
    Tile tiles[CHUNK_SIZE * CHUNK_SIZE];
    int x, y, z;
    int flags;
    Chunk_Id id;
} Chunk;

#define CHUNK_CAP 64
typedef struct Entity_Manager {
    int chunk_count;
    Chunk chunks[CHUNK_CAP];
    Chunk_Id last_chunk_id;
} Entity_Manager;

typedef struct Game_State {
    Entity_Manager entity_manager;

    Vector2 cam_pos;
    f32 current_ortho_size;
    f32 target_ortho_size;

    b32 is_initialized;
} Game_State;

Vector2 mouse_pos_in_world_space(void);

#endif /* ORCHARD_H */