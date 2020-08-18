#ifndef PAWN_H
#define PAWN_H

#include "entity_manager.h"

typedef struct Pawn {
    DEFINE_CHILD_ENTITY;

    Path path;
} Pawn;

Pawn* make_pawn(Entity_Manager* em, Vector2 location);

#endif /* PAWN_H */