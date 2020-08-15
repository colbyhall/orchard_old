#ifndef PAWN_H
#define PAWN_H

#include "entity_manager.h"

typedef enum Instruction_Type {
    IT_Wait,
    IT_Move_To,
    IT_Interact_With,
    IT_Pick_Up,
    IT_Put_Down,
} Instruction_Type;

typedef struct Instruction {
    Instruction_Type type;

    union {
        // IT_Wait
        f32 duration;
        
        // IT_Move_To
        Vector2 target_location;
    };
} Instruction;

typedef enum Task_Type {
    TT_Idle,
    TT_Sleep,
    TT_Work,
    TT_Recreation,
} Task_Type;

#define INSTRUCTION_CAP 8
typedef struct Task {
    String name;
    Task_Type type;
    f32 duration;

    int instruction_count;
    int current_instruction;
    Instruction instructions[INSTRUCTION_CAP];
} Task;

#define TASK_CAP 8
typedef struct Pawn {
    DEFINE_CHILD_ENTITY;

    int task_count;
    Task task_queue[TASK_CAP];

    Path path;
} Pawn;

Pawn* make_pawn(Entity_Manager* em, Vector2 location);

#endif /* PAWN_H */