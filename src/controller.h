#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "entity_manager.h"

typedef enum Controller_Mode {
    CM_Normal,
    CM_Set_Cell,
    CM_Set_Wall,
} Controller_Mode;

typedef struct Controller_Selection {
    b32 valid;
    Vector2 start;
    Vector2 current;
} Controller_Selection;

#define MAX_CAMERA_ORTHO_SIZE 35.f
#define MIN_CAMERA_ORTHO_SIZE 5.f
typedef struct Controller {
    DEFINE_CHILD_ENTITY;
    f32 current_ortho_size;
    f32 target_ortho_size;

    Controller_Mode mode;
    union {
        Controller_Selection selection;
    };
} Controller;

Controller* make_controller(Entity_Manager* em, Vector2 location, f32 ortho_size);
Vector2 get_mouse_pos_in_world_space(Controller* controller);
void set_controller(Entity_Manager* em, Controller* controller);
Rect get_viewport_in_world_space(Controller* controller);

#endif /* CONTROLLER_H */