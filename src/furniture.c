#include "furniture.h"

Furniture* make_furniture(Entity_Manager* em, Furniture_Defintion* definition, Cell_Ref location, Furniture_Direction direction) {
    Furniture* result = make_entity(em, Furniture);
    result->definition = definition;
    result->location = v2((f32)location.x, (f32)location.y);
    result->direction = direction;
    return result;
}

void tick_furniture(Entity_Manager* em, Entity* entity, f32 dt) {

}

void draw_furniture(Entity_Manager* em, Entity* entity) {
    Furniture* furniture = entity->derived;
    Furniture_Defintion* definition = furniture->definition;

    Cell_Ref tile_at = cell_ref_from_location(furniture->location);

    Cell_Ref p0 = tile_at;
    Cell_Ref p1 = { tile_at.x + definition->size_x - 1, tile_at.y + definition->size_y - 1 };

    imm_begin();
    for (cell_rect_iterator(p0, p1)) {
        Cell_Ref at = ref_from_rect_iterator(iter);
        Rect cell_rect = rect_from_cell(at);

        imm_rect(cell_rect, -4.f, v4(0.f, 0.7f, 0.2f, 0.5f));
    }
    imm_flush();
}