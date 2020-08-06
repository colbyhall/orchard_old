#include "draw.h"

typedef union GUI_Id {
    struct { int item, index; };
    u64 whole;
} GUI_Id;

GUI_Id gui_id_from_ptr(void* ptr) { return (GUI_Id) { .whole = (u64)ptr, }; }
GUI_Id gui_id_from_ptr_index(void* ptr, int index) { 
    GUI_Id id;
    id.whole = (u64)ptr;
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

void end_gui(f32 dt) {
    set_shader(find_shader(from_cstr("assets/shaders/font")));
    
    Rect viewport = { v2z(), v2((f32)g_platform->window_width, (f32)g_platform->window_height) };

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
            // @TODO(colby): Alignment
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

#define do_gui(dt) defer_loop(begin_gui, end_gui(dt))
#define push_widget(widget) assert(gui_state->widget_count < GUI_WIDGET_CAP); gui_state->widgets[gui_state->widget_count++] = widget
#define push_layout(layout) assert(gui_state->layout_count < GUI_WIDGET_CAP); gui_state->layouts[gui_state->layout_count++] = layout

b32 gui_pop_layout(GUI_Layout* out) {
    if (!gui_state->layout_count) return 0;

    gui_state->layout_count -= 1;
    if (out) *out = gui_state->layouts[gui_state->layout_count];
    return 1;
}

void gui_push_row_layout_rect(GUI_Id id, Rect rect, b32 going_down) {
    f32 current = going_down ? rect.max.y : rect.min.y;

    GUI_Layout the_layout = {
        .id       = id,
        .bounds   = rect,
        .current  = current,
        .is_col   = false,
        .reversed = !going_down,
    };
    push_layout(the_layout);
}

#define gui_row_layout_rect(id, rect, going_down) defer_loop(gui_push_row_layout_rect(id, rect, going_down), gui_pop_layout(0))

static GUI_Layout* last_layout(void) {
    assert(gui_state->layout_count);
    return &gui_state->layouts[gui_state->layout_count - 1];
}

static Rect rect_from_layout(GUI_Layout* layout, f32 size) {
    if (layout->is_col) {
        invalid_code_path; // @TODO(colby): Implement this
    } else {
        if (layout->reversed) {
            Vector2 bot_left  = v2(layout->bounds.min.x, layout->current);
            Vector2 top_right = v2_add(bot_left, v2(rect_width(layout->bounds), size));
            layout->current  += size;
            return (Rect) { bot_left, top_right };
        } else {
            Vector2 top_right = v2(layout->bounds.max.x, layout->current);
            Vector2 bot_left  = v2_sub(top_right, v2(rect_width(layout->bounds), size));
            layout->current  -= size;
            return (Rect) { bot_left, top_right };
        }
    }

    return (Rect) { 0 };
}

static f32 layout_space_left(GUI_Layout* layout) {
    if (layout->is_col) {
        invalid_code_path; // @TODO(colby): Implement this
    } else {
        if (layout->reversed) return layout->bounds.max.y - layout->current;
        else return layout->current - layout->bounds.min.y;
    }

    return 0.f;
}

void gui_label_rect(Rect rect, GUI_Text_Alignment alignment, String label) {
    GUI_Widget the_widget = {
        .bounds    = rect,
        .type      = GWT_Label,
        .label     = copy_string(label, g_platform->frame_arena),
        .alignment = alignment,
        .color     = gui_state->label_color,
    };

    push_widget(the_widget);
}

void printf_gui_label_rect(Rect rect, GUI_Text_Alignment alignment, const char* fmt, ...) {
    Builder builder = make_builder(g_platform->frame_arena, 0);
    
    va_list args;
    va_start(args, fmt);
    vprintf_builder(&builder, fmt, args);
    va_end(args);

    String label = builder_to_string(builder);

    GUI_Widget the_widget = {
        .bounds    = rect,
        .type      = GWT_Label,
        .label     = label,
        .alignment = alignment,
        .color     = gui_state->label_color,
    };

    push_widget(the_widget);
}

void gui_label(GUI_Text_Alignment alignment, String label) {
    GUI_Layout* layout = last_layout();

    Rect rect = rect_from_layout(layout, 56.f);
    gui_label_rect(rect, alignment, label);
}

void printf_gui_label(GUI_Text_Alignment alignment, const char* fmt, ...) {
    Builder builder = make_builder(g_platform->frame_arena, 0);
    
    va_list args;
    va_start(args, fmt);
    vprintf_builder(&builder, fmt, args);
    va_end(args);

    String label = builder_to_string(builder);
    gui_label(alignment, label); // @HACK: This does a copy
}