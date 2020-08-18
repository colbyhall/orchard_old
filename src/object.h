#ifndef OBJECT_H
#define OBJECT_H

#include "entity_manager.h"

typedef struct Object_Definition {
    int size_x, size_y;
    int max_durability;
} Object_Definition;

typedef struct Object {
    DEFINE_CHILD_ENTITY;

    Object_Definition* definition;

    

} Object;

#endif /* OBJECT_H */