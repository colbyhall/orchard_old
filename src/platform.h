#ifndef PLATFORM_H
#define PLATFORM_H

#include "memory.h"

enum {
    FF_Read     = (1 << 0),
    FF_Write    = (1 << 1),
    FF_Create   = (1 << 2),
    FF_ReadOnly = (1 << 3),
};

typedef struct File_Handle {
    void* os_handle;
    int flags;
    usize size; // @TODO(colby): Investigate if this is worth having here
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
} Platform;

extern Platform* g_platform;

#endif /* PLATFORM_H */