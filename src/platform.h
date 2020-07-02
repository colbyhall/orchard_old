#ifndef PLATFORM_H
#define PLATFORM_H

#include "language_layer.h"

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

typedef enum Directory_Entry_Type {
    DET_File,
    DET_Director,
    DET_Unknown,
} Directory_Entry_Type;

typedef struct File_Metadata {
    u64 creation_time;
    u64 last_access_time;
    u64 last_write_time;

    int size;
    b32 read_only;
} File_Metadata;

#define PLATFORM_OPEN_FILE(name) b32 name(const char* path, int flags, File_Handle* handle)
typedef PLATFORM_OPEN_FILE(Platform_Open_File);

#define PLATFORM_CLOSE_FILE(name) b32 name(File_Handle* handle)
typedef PLATFORM_CLOSE_FILE(Platform_Close_File);

#define PLATFORM_READ_FILE(name) b32 name(File_Handle handle, void* dest, int size)
typedef PLATFORM_READ_FILE(Platform_Read_File);

#define PLATFORM_WRITE_FILE(name) b32 name(File_Handle handle, u8* ptr, int size)
typedef PLATFORM_WRITE_FILE(Platform_Write_File);

#define PLATFORM_FILE_METADATA(name) b32 name(const char* path, File_Metadata* metadata)
typedef PLATFORM_FILE_METADATA(Platform_File_Metadata);

typedef struct Platform {
    Allocator permanent_arena;

    // The whole point of doing this is to build layers. We have a platform layer which contains our platform api
    Platform_Open_File*         open_file;
    Platform_Close_File*        close_file;
    Platform_Read_File*         read_file;
    Platform_Write_File*        write_file;
    Platform_File_Metadata*     file_metadata;

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

    File_Metadata metadata;
    if (!g_platform->file_metadata(path, &metadata)) {
        const b32 did_close = g_platform->close_file(&handle);
        assert(did_close);

        return false;
    }

    u8* the_memory = mem_alloc(allocator, metadata.size + 1);
    if (!g_platform->read_file(handle, the_memory, metadata.size)) return false;

    the_memory[metadata.size] = 0;
    *string = (String) { the_memory, metadata.size, allocator };

    return true;
}

#endif /* PLATFORM_H */