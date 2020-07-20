#include "draw.h"

typedef union E_UI_Id {
    struct { int item, index; }
    u64 whole;
} E_UI_Id;

static E_UI_Id e_ui_id_from_ptr(void* ptr) { return (E_UI_Id) { .whole = (u64)ptr, }; }
static E_UI_Id e_ui_id_from_ptr_index(void* ptr, int index) { 
    E_UI_Id id;
    id.item = (u32)ptr;
    id.index = index;
    return id;
}

static u64 hash_e_ui_id(void* a, void* b, int size) {
    assert(sizeof(E_UI_Id) == size);

    const E_UI_Id* const a_id = a;
    if (b) {
        const E_UI_Id* const b_id = b;
        return a_id->whole == b_id->whole;
    }

    return fnv1_hash(a, size);
}

typedef enum E_Widget_Type {
    EWT_Panel,
    EWT_Button,
    EWT_Text_Input,
    EWT_Label,
    EWT_Scroll,
} E_Widget_Type;

enum E_Widget_Flags {
    EWF_Has_Clip    = (1 << 0),
};

typedef struct E_Widget {
    E_UI_Id id;
    int flags;

    Rect bounds;
    Rect clip;

    E_Widget_Type type;
    union {

    };
} E_Widget;

typedef struct E_Widget_State {
    E_Widget_Type type;
    u64 frame_updated;
    union {

    };
} E_Widget_State;

typedef struct E_Layout {
    E_UI_Id id;
    Rect bounds;
    f32 current;
    b32 is_col;
    b32 reversed;
} E_Layout;

#define E_WIDGET_CAP 512
typedef struct E_UI_State {
    int widget_count;
    E_Widget widgets[E_WIDGET_CAP];
    Hash_Table widget_state; // Key E_UI_Id, Value E_Widget_State

    int layout_count;
    E_Layout layouts[E_WIDGET_CAP];

    int clip_count;
    Rect clips[E_WIDGET_CAP];

    E_UI_Id hovered;
    E_UI_Id focused;
    b32 focus_was_set;

    u64 current_frame;
    b32 is_initialized;
} E_UI_State;

static E_UI_State* e_ui_state;

void init_editor_ui(Platform* platform) {
    e_ui_state = mem_alloc_struct(platform->permanent_arena, E_UI_State);

    if (e_ui_state->is_initialized) return;
    e_ui_state->is_initialized = true;

    e_ui_state->widget_state = make_hash_table(E_UI_Id, E_Widget_State, hash_e_ui_id, platform->permanent_arena);
    reserve_hash_table(E_WIDGET_CAP);
}

void begin_editor_ui(void) {
    e_ui_state->widget_count = 0;
    e_ui_state->layout_count = 0;
    e_ui_state->clip_count   = 0;

    e_ui_state->current_frame += 1;
    e_ui_state->focus_was_set = false;
}

void end_editor_ui(f32 dt, Rect viewport) {

}

