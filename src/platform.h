#ifndef PLATFORM_H
#define PLATFORM_H

#include "language_layer.h"

enum {
    FF_Read     = (1 << 0),
    FF_Write    = (1 << 1),
    FF_Create   = (1 << 2),
    FF_ReadOnly = (1 << 3),
};

typedef struct File_Handle {
    void* os_handle;
    int flags;
    int size; // @TODO(colby): Investigate if this is worth having here
} File_Handle; 

#define PLATFORM_OPEN_FILE(name) b32 name(const char* path, int flags, File_Handle* handle)
typedef PLATFORM_OPEN_FILE(Platform_Open_File);

#define PLATFORM_CLOSE_FILE(name) b32 name(File_Handle* handle)
typedef PLATFORM_CLOSE_FILE(Platform_Close_File);

#define PLATFORM_READ_FILE(name) b32 name(File_Handle handle, void* dest, usize size)
typedef PLATFORM_READ_FILE(Platform_Read_File);

#define PLATFORM_WRITE_FILE(name) b32 name(File_Handle handle, u8* ptr, usize size);
typedef PLATFORM_WRITE_FILE(Platform_Write_File);

typedef struct Platform {
    Allocator permanent_arena;

    // Platform api
    Platform_Open_File*     open_file;
    Platform_Close_File*    close_file;
    Platform_Read_File*     read_file;
    Platform_Write_File*    write_file;

    void* window_handle;
    int window_width;
    int window_height;

    f64 current_frame_time;
    f64 last_frame_time;
} Platform;

extern Platform* g_platform;

inline b32 read_file_into_string(const char* path, String* string, Allocator allocator) {
    File_Handle handle;
    if (!g_platform->open_file(path, FF_Read, &handle)) return false;

    u8* the_memory = mem_alloc(allocator, handle.size + 1);
    if (!g_platform->read_file(handle, the_memory, handle.size)) return false;

    the_memory[handle.size] = 0;
    *string = (String) { the_memory, handle.size, allocator };

    return true;
}

#endif /* PLATFORM_H */