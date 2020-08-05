#include "draw.h"

typedef union GUI_Id {
    struct { int item, index; };
    u64 whole;
} GUI_Id;

static GUI_Id gui_id_from_ptr(void* ptr) { return (GUI_Id) { .whole = (u64)ptr, }; }
static GUI_Id gui_id_from_ptr_index(void* ptr, int index) { 
    GUI_Id id;
    id.item = (int)((u64)ptr);
    id.index = index;
    return id;
}

const GUI_Id null_gui = { 0 };

static u64 hash_gui_id(void* a, void* b, int size) {
    assert(sizeof(GUI_Id) == size);

    const GUI_Id* const a_id = a;
    if (b) {
        const GUI_Id* const b_id = b;
        return a_id->whole == b_id->whole;
    }

    return fnv1_hash(a, size);
}

typedef enum GUI_Widget_Type {
    GWT_Panel,
    GWT_Label,
    GWT_Button,
    GWT_Text_Input,
    GWT_Scroll,
} GUI_Widget_Type;

enum GUI_Widget_Flags {
    GWF_Has_Clip    = (1 << 0),
};

typedef enum GUI_Text_Alignment {
    GTA_Left,
    GTA_Center,
    GTA_Right,
    // GTA 5 go brrrr
} GUI_Text_Alignment;

typedef struct GUI_Widget {
    GUI_Id id;
    int flags;

    Rect bounds;
    Rect clip;

    Vector4 color;

    GUI_Widget_Type type;
    union {
        // GWT_Label
        struct {
            String label;
            GUI_Text_Alignment alignment;
        };
    };
} GUI_Widget;

typedef struct GUI_Widget_State {
    GUI_Widget_Type type;
    int frame_updated;
    union {
        int test;
    };
} GUI_Widget_State;

typedef struct GUI_Layout {
    GUI_Id id;
    Rect bounds;
    f32 current;
    b32 is_col;
    b32 reversed;
} GUI_Layout;

#define GUI_WIDGET_CAP 512
typedef struct GUI_State {
    int widget_count;
    GUI_Widget widgets[GUI_WIDGET_CAP];
    Hash_Table widget_state; // Key GUI_Id, Value E_Widget_State

    int layout_count;
    GUI_Layout layouts[GUI_WIDGET_CAP];

    int clip_count;
    Rect clips[GUI_WIDGET_CAP];

    GUI_Id hovered;
    GUI_Id focused;
    b32 focus_was_set;

    Vector4 label_color;

    int current_frame;
    b32 is_initialized;
} GUI_State;

static GUI_State* gui_state;

void init_gui(Platform* platform) {
    gui_state = mem_alloc_struct(platform->permanent_arena, GUI_State);

    if (gui_state->is_initialized) return;
    gui_state->is_initialized = true;
    gui_state->label_color = v4s(1.f);

    gui_state->widget_state = make_hash_table(GUI_Id, GUI_Widget_State, hash_gui_id, platform->permanent_arena);
    reserve_hash_table(&gui_state->widget_state, GUI_WIDGET_CAP);
}

void begin_gui(void) {
    gui_state->widget_count = 0;
    gui_state->layout_count = 0;
    gui_state->clip_count   = 0;

    gui_state->current_frame += 1;
    gui_state->focus_was_set = false;
}

void end_gui(f32 dt, Rect viewport) {
    set_shader(find_shader(from_cstr("assets/shaders/font")));
    draw_right_handed(viewport);

    Font_Collection* fc = find_font_collection(from_cstr("assets/fonts/Menlo-Regular"));
    Font* font = font_at_size(fc, 48);
    set_uniform_texture("atlas", font->atlas);

    f32 gui_z = -1.f;

    imm_begin();
    for (int i = 0; i < gui_state->widget_count; ++i) {
        GUI_Widget widget = gui_state->widgets[i];

        switch (widget.type) {
        case GWT_Label: 
            imm_string(
                widget.label, 
                font, (f32)font->size, 
                widget.bounds.max.x - widget.bounds.min.y, 
                v2(widget.bounds.min.x, widget.bounds.max.y - (f32)font->size), 
                gui_z, 
                widget.color
            );
            break;
        default: invalid_code_path;
        };
    }
    imm_flush();

    gui_state->widget_count = 0;
}

#define do_gui(dt, viewport) defer_loop(begin_gui, end_gui(dt, viewport))

void gui_label_rect(Rect rect, String label, GUI_Text_Alignment alignment) {
    GUI_Widget the_widget = {
        .bounds    = rect,
        .type      = GWT_Label,
        .label     = copy_string(label, g_platform->frame_arena),
        .alignment = alignment,
        .color     = gui_state->label_color,
    };

    gui_state->widgets[gui_state->widget_count++] = the_widget;
}

