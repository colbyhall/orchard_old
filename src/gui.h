#ifndef GUI_H
#define GUI_H

#include "language_layer.h"

typedef union GUI_Id {
    struct { int item, index; };
    u64 whole;
} GUI_Id;

inline GUI_Id gui_id_from_ptr(void* ptr) { return (GUI_Id) { .whole = (u64)ptr, }; }
inline GUI_Id gui_id_from_ptr_index(void* ptr, int index) { 
    GUI_Id id;
    id.item = (int)((u64)ptr);
    id.index = index;
    return id;
}
#define procedural_gui_id (GUI_Id) { .item = __LINE__, .index = __COUNTER__ }

const GUI_Id null_gui_id = { 0 };

typedef struct GUI_Layout {
    GUI_Id id;
    Rect bounds;
    f32 current;
    b32 is_col;
    b32 reversed;
} GUI_Layout;

b32 gui_pop_layout(GUI_Layout* out);
void gui_push_row_layout_rect(GUI_Id id, Rect rect, b32 going_down);
#define gui_row_layout_rect(rect, going_down) defer_loop(gui_push_row_layout_rect(null_gui_id, rect, going_down), gui_pop_layout(0))
void gui_push_col_layout_rect(GUI_Id id, Rect rect, b32 going_right);
#define gui_col_layout_rect(rect, going_right) defer_loop(gui_push_col_layout_rect(null_gui_id, rect, going_right), gui_pop_layout(0))
void gui_push_col_layout_size(GUI_Id id, f32 size, b32 going_right);
#define gui_col_layout_size(size, going_right) defer_loop(gui_push_col_layout_size(null_gui_id, size, going_right), gui_pop_layout(0))

void gui_label_rect(Rect rect, String label) ;
void gui_label(String label);
void gui_label_printf_rect(Rect rect, const char* fmt, ...);
void gui_label_printf(const char* fmt, ...);

b32 gui_checkbox_rect(GUI_Id id, Rect rect, b32* value);
b32 gui_checkbox(GUI_Id id, b32* value);

void gui_panel_rect(Rect rect);

void init_gui(Platform* platform);

void begin_gui(void);
void end_gui(f32 dt);
#define do_gui(dt) defer_loop(begin_gui(), end_gui(dt))

b32 is_hovering_widget(void);

#endif /* GUI_H */