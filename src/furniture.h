#ifndef FURNITURE_H
#define FURNITURE_H 1

#include "entity_manager.h"

typedef enum Furniture_Direction {
    FD_North,
    FD_South,
    FD_East,
    FD_West,
} Furniture_Direction;

enum Furniture_Definition_Flags {
    FDF_North_Sprite_Is_South    = (1 << 0),
    FDF_East_Sprite_Is_West      = (1 << 1),
    FDF_All_Directions_Use_North = (FDF_North_Sprite_Is_South | FDF_East_Sprite_Is_West),
};

typedef struct Furniture_Defintion {
    int flags;
    int size_x, size_y;

    Rect north_sprite_uv, south_sprite_uv;
    Rect east_sprite_uv, west_sprite_uv;
} Furniture_Defintion;

static Furniture_Defintion furniture_definitions[] = {
    {
        0, 
        3, 
        2
    },
};

typedef struct Furniture {
    DEFINE_CHILD_ENTITY;

    Furniture_Defintion* definition;
    Furniture_Direction direction;
} Furniture;

Furniture* make_furniture(Entity_Manager* em, Furniture_Defintion* definition, Cell_Ref location, Furniture_Direction direction);

#endif /* FURNITURE_H */