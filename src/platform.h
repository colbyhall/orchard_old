#ifndef PLATFORM_H
#define PLATFORM_H

#include "language_layer.h"
#include "input.h"
#include "math.h"

enum File_Flags {
    FF_Read     = (1 << 0),
    FF_Write    = (1 << 1),
    FF_Create   = (1 << 2),
};

typedef struct File_Handle {
    void* os_handle;
    String path;
    int flags;
} File_Handle; 

typedef struct File_Metadata {
    u64 creation_time;
    u64 last_access_time;
    u64 last_write_time;

    int size;
    b32 read_only;
} File_Metadata;

typedef enum Directory_Entry_Type {
    DET_File,
    DET_Directory,
    DET_Unknown,
} Directory_Entry_Type;

typedef struct Directory_Result {
    String path;
    File_Metadata metadata;
    Directory_Entry_Type type;
    struct Directory_Result* next;
} Directory_Result;

#define PLATFORM_OPEN_FILE(name) b32 name(String path, int flags, File_Handle* handle)
typedef PLATFORM_OPEN_FILE(Platform_Open_File);

#define PLATFORM_CLOSE_FILE(name) b32 name(File_Handle* handle)
typedef PLATFORM_CLOSE_FILE(Platform_Close_File);

#define PLATFORM_READ_FILE(name) b32 name(File_Handle handle, void* dest, int size)
typedef PLATFORM_READ_FILE(Platform_Read_File);

#define PLATFORM_WRITE_FILE(name) b32 name(File_Handle handle, u8* ptr, int size)
typedef PLATFORM_WRITE_FILE(Platform_Write_File);

#define PLATFORM_FILE_METADATA(name) b32 name(String path, File_Metadata* metadata)
typedef PLATFORM_FILE_METADATA(Platform_File_Metadata);

#define PLATFORM_FIND_ALL_FILES_IN_DIR(name) Directory_Result* name(String path, b32 do_recursive, Allocator allocator)
typedef PLATFORM_FIND_ALL_FILES_IN_DIR(Platform_Find_All_Files_In_Dir);

#define directory_iterator(path, do_recursive, allocator) Directory_Result* iter = g_platform->find_all_files_in_dir(path, do_recursive, allocator); iter != 0; iter = iter->next

#define PLATFORM_CREATE_DIRECTORY(name) b32 name(String path)
typedef PLATFORM_CREATE_DIRECTORY(Platform_Create_Directory);

typedef struct System_Time {
    u16 year;
    u16 month;
    u16 day_of_week;
    u16 day_of_month;
    u16 hour;
    u16 minute;
    u16 second;
    u16 milli;
} System_Time;

#define PLATFORM_LOCAL_TIME(name) System_Time name(void)
typedef PLATFORM_LOCAL_TIME(Platform_Local_Time);

#define PLATFORM_CYCLES(name) u64 name(void)
typedef PLATFORM_CYCLES(Platform_Cycles);

#define PLATFORM_TIME_IN_SECONDS(name) f64 name(void)
typedef PLATFORM_TIME_IN_SECONDS(Platform_Time_In_Seconds);

typedef enum OS_Event_Type {
    OET_Window_Resized = 0,
    OET_Window_Closed,
    OET_Key_Pressed,
    OET_Key_Released,
    OET_Char,
    OET_Mouse_Button_Pressed,
    OET_Mouse_Button_Released,
    OET_Mouse_Moved,
} OS_Event_Type;

typedef struct OS_Event {
    OS_Event_Type type;
    union {
        struct { int old_width, old_height; };
        u8 key;
        Rune c;
        u8 mouse_button;
        struct { int pos_x, pos_y; };
    };
} OS_Event;

#define OS_EVENT_CAP 256
typedef struct Platform {
    Allocator permanent_arena;
    Allocator frame_arena;
    
    // The whole point of doing this is to build layers. We have a platform layer which contains our platform api
    Platform_Open_File*         open_file;
    Platform_Close_File*        close_file;
    Platform_Read_File*         read_file;
    Platform_Write_File*        write_file;
    Platform_File_Metadata*     file_metadata;

    Platform_Find_All_Files_In_Dir* find_all_files_in_dir;
    Platform_Create_Directory*      create_directory;

    Platform_Local_Time*        local_time;
    Platform_Cycles*            cycles;
    Platform_Time_In_Seconds*   time_in_seconds;

    void* window_handle;
    int window_width;
    int window_height;
    f32 dpi_scale;

    f64 current_frame_time;
    f64 last_frame_time;

    int num_events;
    OS_Event events[OS_EVENT_CAP];

    Input input;
} Platform;

extern Platform* g_platform;

inline b32 read_file_into_string(String path, String* string, Allocator allocator) {
    File_Handle handle;
    if (!g_platform->open_file(path, FF_Read, &handle)) return false;

    File_Metadata metadata;
    if (!g_platform->file_metadata(path, &metadata)) {
        b32 did_close = g_platform->close_file(&handle);
        assert(did_close);

        return false;
    }

    u8* the_memory = mem_alloc(allocator, metadata.size + 1);
    if (!g_platform->read_file(handle, the_memory, metadata.size)) return false;

    the_memory[metadata.size] = 0;
    *string = (String) { the_memory, metadata.size, allocator };

    return true;
}

inline b32 is_key_pressed(u8 key) { return g_platform->input.state.keys_down[key]; }
inline b32 was_key_pressed(u8 key) { return g_platform->input.state.keys_down[key] && !g_platform->input.prev_state.keys_down[key]; }
inline b32 was_key_released(u8 key) { return !g_platform->input.state.keys_down[key] && g_platform->input.prev_state.keys_down[key]; }

inline b32 is_mouse_button_pressed(u8 mouse_button) { return g_platform->input.state.mouse_buttons_down[mouse_button]; }
inline b32 was_mouse_button_pressed(u8 mouse_button) { return g_platform->input.state.mouse_buttons_down[mouse_button] && !g_platform->input.prev_state.mouse_buttons_down[mouse_button]; }
inline b32 was_mouse_button_released(u8 mouse_button) { return !g_platform->input.state.mouse_buttons_down[mouse_button] && g_platform->input.prev_state.mouse_buttons_down[mouse_button]; }

inline Rect viewport_rect(void) { return (Rect) { v2z(), v2((f32)g_platform->window_width, (f32)g_platform->window_height) }; }

#endif /* PLATFORM_H */