#include "gui.h"
#include "draw.h"

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
    GWT_Label,
    GWT_Checkbox,
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
        // GWT_Checkbox
        b32 is_checked;
    };
} GUI_Widget;

typedef struct GUI_Widget_State {
    GUI_Widget_Type type;
    int frame_updated;
    union {
        int test;
    };
} GUI_Widget_State;

#define GUI_WIDGET_CAP 1024
typedef struct GUI_State {
    int widget_count;
    GUI_Widget widgets[GUI_WIDGET_CAP];
    Hash_Table widget_state; // Key GUI_Id, Value E_Widget_State

    int layout_count;
    GUI_Layout layouts[GUI_WIDGET_CAP];

    int clip_count;
    Rect clips[GUI_WIDGET_CAP];

    Vector4 label_color;
    Font* label_font;

    GUI_Id hovered;
    GUI_Id focused;
    b32 focus_was_set;

    f32 scale;

    int current_frame;
    f64 start_time;
    f64 last_duration;

    b32 is_initialized;
} GUI_State;

static GUI_State* gui_state;

#define push_widget(widget) assert(gui_state->widget_count < GUI_WIDGET_CAP); gui_state->widgets[gui_state->widget_count++] = widget
#define push_layout(layout) assert(gui_state->layout_count < GUI_WIDGET_CAP); gui_state->layouts[gui_state->layout_count++] = layout

static void set_focus(GUI_Id id) {
    gui_state->focused = id;
    gui_state->focus_was_set = true;
}

static GUI_Layout* last_layout(void) {
    assert(gui_state->layout_count);
    return &gui_state->layouts[gui_state->layout_count - 1];
}

static Rect rect_from_layout(GUI_Layout* layout, f32 size) {
    if (layout->is_col) {
        if (layout->reversed) {
            Vector2 top_right  = v2(layout->current, layout->bounds.max.y);
            Vector2 bot_left = v2_sub(top_right, v2(size, rect_height(layout->bounds)));
            layout->current  -= size;
            return (Rect) { bot_left, top_right };
        } else {
            Vector2 bot_left  = v2(layout->current, layout->bounds.min.y);
            Vector2 top_right = v2_add(bot_left, v2(size, rect_height(layout->bounds)));
            layout->current  += size;
            return (Rect) { bot_left, top_right };
        }
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
}

static f32 layout_space_left(GUI_Layout* layout) {
    if (layout->is_col) {
        if (layout->reversed) return layout->bounds.max.x - layout->current;
        return layout->current - layout->bounds.min.x;
    } else {
        if (layout->reversed) return layout->bounds.max.y - layout->current;
        return layout->current - layout->bounds.min.y;
    }
}

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

void gui_push_col_layout_rect(GUI_Id id, Rect rect, b32 going_right) {
    f32 current = going_right ? rect.min.x : rect.max.x;

    GUI_Layout the_layout = {
        .id       = id,
        .bounds   = rect,
        .current  = current,
        .is_col   = true,
        .reversed = !going_right,
    };
    push_layout(the_layout);
}

void gui_push_col_layout_size(GUI_Id id, f32 size, b32 going_right) {
    GUI_Layout* layout = last_layout();

    Rect rect = rect_from_layout(layout, size);
    gui_push_col_layout_rect(id, rect, going_right);
}

void gui_label_rect(Rect rect, String label) {
    GUI_Widget the_widget = {
        .bounds    = rect,
        .type      = GWT_Label,
        .label     = copy_string(label, g_platform->frame_arena),
        .alignment = GTA_Left,
        .color     = gui_state->label_color,
    };

    push_widget(the_widget);
}

void gui_label_printf_rect(Rect rect, const char* fmt, ...) {
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
        .alignment = GTA_Left,
        .color     = gui_state->label_color,
    };

    push_widget(the_widget);
}

void gui_label(String label) {
    GUI_Layout* layout = last_layout();

    Rect string_rect = font_string_rect(label, gui_state->label_font, rect_width(layout->bounds));
    f32 padding = 2.f * gui_state->scale;

    Rect rect = rect_from_layout(layout, (layout->is_col ? string_rect.max.x : string_rect.max.y) + padding);
    gui_label_rect(rect, label);
}

void gui_label_printf(const char* fmt, ...) {
    Builder builder = make_builder(g_platform->frame_arena, 0);
    
    va_list args;
    va_start(args, fmt);
    vprintf_builder(&builder, fmt, args);
    va_end(args);

    String label = builder_to_string(builder);
    gui_label(label); // @HACK: This does a copy
}

b32 gui_checkbox_rect(GUI_Id id, Rect rect, b32* value) {
    Vector2 mouse_pos = v2((f32)g_platform->input.state.mouse_x, (f32)g_platform->input.state.mouse_y);
    b32 mouse_over = rect_overlaps_point(rect, mouse_pos);

    b32 triggered = false;
    if (mouse_over) gui_state->hovered = id;
    else if (gui_state->hovered.whole == id.whole) gui_state->hovered = null_gui_id;

    if (gui_state->focused.whole == id.whole && was_mouse_button_released(MOUSE_LEFT)) {
        if (gui_state->hovered.whole == id.whole) triggered = true;
        gui_state->focused = null_gui_id;
    } else if (gui_state->hovered.whole == id.whole && was_mouse_button_pressed(MOUSE_LEFT)) set_focus(id);

    if (triggered) *value = !(*value);

    GUI_Widget the_widget = {
        .bounds     = rect,
        .id         = id,
        .type       = GWT_Checkbox,
        .is_checked = *value,
    };

    push_widget(the_widget);
    return triggered;
}

b32 gui_checkbox(GUI_Id id, b32* value) {
    GUI_Layout* layout = last_layout();

    Rect rect = rect_from_layout(layout, 60.f); // @TODO(colby): Use the actual text rect
    rect.max = v2_add(rect.min, v2s(50.f));
    return gui_checkbox_rect(id, rect, value);
}

b32 is_hovering_widget(void) { return gui_state->hovered.whole != 0; }

void init_gui(Platform* platform) {
    gui_state = mem_alloc_struct(platform->permanent_arena, GUI_State);

    if (gui_state->is_initialized) return;
    gui_state->is_initialized = true;
    gui_state->label_color = v4s(1.f);
    gui_state->scale = platform->dpi_scale;

    Font_Collection* fc = find_font_collection(from_cstr("assets/fonts/Menlo-Regular"));
    gui_state->label_font = font_at_size(fc, (int)(24.f * gui_state->scale));

    gui_state->widget_state = make_hash_table(GUI_Id, GUI_Widget_State, hash_gui_id, platform->permanent_arena);
    reserve_hash_table(&gui_state->widget_state, GUI_WIDGET_CAP);
}

void begin_gui(void) {
    gui_state->widget_count = 0;
    gui_state->layout_count = 0;
    gui_state->clip_count   = 0;

    gui_state->current_frame += 1;
    gui_state->focus_was_set = false;

    gui_state->start_time = g_platform->time_in_seconds();
}

void end_gui(f32 dt) {
    set_shader(find_shader(from_cstr("assets/shaders/font")));
    
    Rect viewport = { v2z(), v2((f32)g_platform->window_width, (f32)g_platform->window_height) };

    draw_right_handed(viewport);
    set_uniform_texture("atlas", gui_state->label_font->atlas);

    f32 gui_z = -1.f;

    imm_begin();
    for (int i = 0; i < gui_state->widget_count; ++i) {
        GUI_Widget widget = gui_state->widgets[i];

        switch (widget.type) {
        case GWT_Label: 
            // @TODO(colby): Alignment
            f32 max_width = widget.bounds.max.x - widget.bounds.min.x;
            Vector2 xy = v2(widget.bounds.min.x, widget.bounds.max.y - (f32)gui_state->label_font->size);
            imm_string_2d(widget.label, gui_state->label_font, max_width, xy, gui_z, widget.color);
            break;
        case GWT_Checkbox:
            Vector4 background_color = v4(1.f, 1.f, 1.f, 0.5f);
            if (gui_state->hovered.whole == widget.id.whole) background_color.a = 0.7f;
            if (gui_state->focused.whole == widget.id.whole) background_color.a = 1.f;
            imm_rect(widget.bounds, gui_z, background_color);

            if (widget.is_checked) {
                Vector4 foreground_color = background_color;
                foreground_color.xyz = v3s(0.f);
                foreground_color.a += 0.2f;
                imm_rect((Rect) { v2_add(widget.bounds.min, v2s(10.f)), v2_sub(widget.bounds.max, v2s(10.f)) }, gui_z, foreground_color);
            }
            break;
        default: invalid_code_path;
        };
    }
    imm_flush();

    gui_state->widget_count = 0;
    gui_state->last_duration = g_platform->time_in_seconds() - gui_state->start_time;
}
