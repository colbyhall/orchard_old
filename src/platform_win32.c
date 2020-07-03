#include "platform.h"
#include "memory.c"
#include "program_options.h"

// Windows include needs to happen before _win32.c includes
#define WIN32_LEAN_AND_MEAN
#define win32_MEAN_AND_LEAN
#include <windows.h>

Platform* g_platform;

static b32 is_running = true;

typedef struct Game_Code {
    HMODULE library;
    u64 last_write_time;
    
    void** functions;
    int function_count;

    char dll_path[1024];
} Game_Code;

#define INIT_GAME(name) void name(Platform* platform)
typedef INIT_GAME(Init_Game);

#define TICK_GAME(name) void name(f32 dt)
typedef TICK_GAME(Tick_Game);

#define SHUTDOWN_GAME(name) void name(void)
typedef SHUTDOWN_GAME(Shutdown_Game);

typedef struct Game_Code_VTable {
    Init_Game*      init_game;
    Tick_Game*      tick_game;
    Shutdown_Game*  shutdown_game;
} Game_Code_VTable;

static const char* game_code_vtable_names[] = {
    "init_game",
    "tick_game",
    "shutdown_game",
};

static const char* dll_path = "bin/game_module.dll";

static void new_dll_path(char* buffer) {
    static int last = 0;
    sprintf(buffer, "bin\\temp_game_module_%i.dll", last);
    last += 1;
}

static b32 try_reload_dll(Game_Code* game_code) {
    File_Metadata metadata;
    const b32 found_metadata = g_platform->file_metadata(dll_path, &metadata);
    if (!found_metadata) return false;

    if (!game_code->library || game_code->last_write_time != metadata.last_write_time) {
        new_dll_path(game_code->dll_path);
        game_code->last_write_time = metadata.last_write_time;

        const BOOL did_copy_file = CopyFileA(dll_path, game_code->dll_path, false);
        if (!did_copy_file) return false;

        if (game_code->library) {
            FreeLibrary(game_code->library);
        }

        game_code->library = LoadLibraryA(game_code->dll_path);
        for (int i = 0; i < game_code->function_count; ++i) {
            game_code->functions[i] = (void*)GetProcAddress(game_code->library, game_code_vtable_names[i]);
        }

        return true;
    }

    return false;
}

static PLATFORM_OPEN_FILE(win32_open_file) {
    const b32 read   = flags & FF_Read;
    const b32 write  = flags & FF_Write;
    const b32 create = flags & FF_Create;

    assert(read || write);

    DWORD desired_access = 0;
    if (read)  desired_access |= GENERIC_READ;
    if (write) desired_access |= GENERIC_WRITE;

    DWORD creation = OPEN_EXISTING;
    if (create) creation = OPEN_ALWAYS;

    // Try to create the file
    void* const os_handle = CreateFileA(path, desired_access, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, creation, FILE_ATTRIBUTE_NORMAL, 0);
    const b32 is_open = os_handle != INVALID_HANDLE_VALUE;

    // If we actually opened the file then fill out the handle data
    if (is_open) {
        const String wrapped_path = { (u8*)path, (int)str_len(path) };
        *handle = (File_Handle) { os_handle, wrapped_path, flags };
        return true;
    }
    
    return false;
}

static PLATFORM_CLOSE_FILE(win32_close_file) {
    if (!handle->os_handle) return false;

    const b32 result = CloseHandle(handle->os_handle) == 1;
    handle->os_handle = 0;
    return result;
}

static PLATFORM_READ_FILE(win32_read_file) {
    assert(handle.os_handle);
    return ReadFile(handle.os_handle, dest, (DWORD)size, 0, 0);
}

static PLATFORM_WRITE_FILE(win32_write_file) {
    return WriteFile(handle.os_handle, ptr, (DWORD)size, 0, 0);
}

static PLATFORM_FILE_METADATA(win32_file_metadata) {
    WIN32_FILE_ATTRIBUTE_DATA file_data;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &file_data)) {
        const FILETIME creation_time = file_data.ftCreationTime;
        metadata->creation_time = (creation_time.dwHighDateTime << 16) | creation_time.dwLowDateTime;

        const FILETIME last_access_time = file_data.ftLastAccessTime;
        metadata->last_access_time = (last_access_time.dwHighDateTime << 16) | last_access_time.dwLowDateTime;

        const FILETIME last_write_time = file_data.ftLastWriteTime;
        metadata->last_write_time = (last_write_time.dwHighDateTime << 16) | last_write_time.dwLowDateTime;

        metadata->size = (int)file_data.nFileSizeLow;
        metadata->read_only = (file_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
        return true;
    }

    return false;
}

int main(int argv, char** argc) {
    Allocator permanent_arena = arena_allocator(
        VirtualAlloc(0, PERMANENT_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE),
        PERMANENT_MEMORY_SIZE
    );

    // Move us to project base dir
    {
        char buffer[512];
        const DWORD len = GetModuleFileNameA(0, buffer, 512);
        for (u32 i = len - 1; i > 0; --i) {
            const char c = buffer[i];
            if (c == '\\' || c == '/') {
                buffer[i] = 0;
                break;
            }
        }
        SetCurrentDirectoryA(buffer);
        SetCurrentDirectoryA("..");
    }

    Platform the_platform = {
        .permanent_arena    = permanent_arena,
        .open_file          = win32_open_file,
        .close_file         = win32_close_file,
        .read_file          = win32_read_file,
        .write_file         = win32_write_file,
        .file_metadata      = win32_file_metadata,
    };

    // Create the window
    {
        HINSTANCE hInstance = GetModuleHandle(0); // @Temp

        const WNDCLASSA window_class = {
            .lpfnWndProc    = DefWindowProcA,
            .hInstance      = hInstance,
            .hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1),
            .lpszClassName  = WINDOW_TITLE " window class",
            .hCursor        = LoadCursor(0, IDC_ARROW),
            .style          = CS_OWNDC,
        };
        
        RegisterClassA(&window_class);
        
        RECT adjusted_rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
        AdjustWindowRect(&adjusted_rect, WS_OVERLAPPEDWINDOW, 0);
        
        const int width = adjusted_rect.right - adjusted_rect.left;
        const int height = adjusted_rect.bottom - adjusted_rect.top;

        const int monitor_width = GetSystemMetrics(SM_CXSCREEN);
        const int monitor_height = GetSystemMetrics(SM_CYSCREEN);
        
        const int x = monitor_width / 2 - width / 2;
        const int y = monitor_height / 2 - height / 2;
        
        HWND window_handle = CreateWindowA(
            window_class.lpszClassName,
            WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            x,
            y,
            width,
            height,
            0, 0,
            hInstance,
            0
        );
        assert(window_handle != INVALID_HANDLE_VALUE);

        the_platform.window_handle  = window_handle;
        the_platform.window_width   = WINDOW_WIDTH;
        the_platform.window_height  = WINDOW_HEIGHT;
    }
    g_platform = &the_platform;

    // Load game code
    Game_Code_VTable game_code_vtable;
    Game_Code game_code = {
        .function_count = ARRAY_COUNT(game_code_vtable_names),
        .functions      = (void**)&game_code_vtable,
    };
    const b32 loaded_game_code = try_reload_dll(&game_code);
    assert(loaded_game_code);

    game_code_vtable.init_game(g_platform);

    LARGE_INTEGER qpc_freq;
    QueryPerformanceFrequency(&qpc_freq);

    ShowWindow(the_platform.window_handle, SW_SHOW);

    LARGE_INTEGER last_frame_time;
    QueryPerformanceCounter(&last_frame_time);
    while (is_running) {
        LARGE_INTEGER current_frame_time;
        QueryPerformanceCounter(&current_frame_time);

        // Update frame timing
        g_platform->last_frame_time     = last_frame_time.QuadPart / (f64)qpc_freq.QuadPart;
        g_platform->current_frame_time  = current_frame_time.QuadPart / (f64)qpc_freq.QuadPart;
        last_frame_time = current_frame_time;

        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        const f32 dt  = (f32)(g_platform->current_frame_time - g_platform->last_frame_time);
        game_code_vtable.tick_game(dt);

        // Do a hot reload if we can
        if (try_reload_dll(&game_code)) {
            reset_arena(the_platform.permanent_arena);
            game_code_vtable.init_game(g_platform);
        }
    }

    game_code_vtable.shutdown_game();

    return 0;
}
