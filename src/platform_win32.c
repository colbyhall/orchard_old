#include "platform.h"
#include "memory.h"
#include "memory.c"
#include "program_options.h"

// Windows include needs to happen before _win32.c includes
#define WIN32_LEAN_AND_MEAN
#define win32_MEAN_AND_LEAN
#include <windows.h>

Platform* g_platform;

static b32 is_running = 1;
static HWND window_handle;

typedef struct Game_Code {
    HMODULE library;
    u64 last_write_time;
    
    void** functions;
    int function_count;

    char dll_path[1024];
} Game_Code;

#define INIT_GAME(name) void name(Platform* platform)
typedef INIT_GAME(Init_Game);

#define TICK_GAME(name) void name(void)
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
        const DWORD attrib = GetFileAttributesA(path);
        if (attrib & FILE_ATTRIBUTE_READONLY) flags |= FF_ReadOnly;

        const usize size = (usize)GetFileSize(os_handle, 0);
        *handle = (File_Handle) { os_handle, flags, size };
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

    // Load game code
    Game_Code_VTable game_code_vtable;
    Game_Code game_code;
    {
        new_dll_path(game_code.dll_path);
        const BOOL did_copy_file = CopyFileA(dll_path, game_code.dll_path, false);
        assert(did_copy_file);

        game_code.function_count = ARRAY_COUNT(game_code_vtable_names);
        game_code.functions = (void**)&game_code_vtable;
        game_code.library = LoadLibraryA(game_code.dll_path);
        for (int i = 0; i < game_code.function_count; ++i) {
            game_code.functions[i] = (void*)GetProcAddress(game_code.library, game_code_vtable_names[i]);
        }
    }

    Platform the_platform = (Platform) {
        .permanent_arena    = permanent_arena,
        .open_file          = win32_open_file,
    };

    // Create the window
    {
        HINSTANCE hInstance = GetModuleHandle(0); // @Temp

        WNDCLASSA window_class = (WNDCLASSA) {
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
        
        window_handle = CreateWindowA(
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

        the_platform.window_handle = window_handle;
    }
    g_platform = &the_platform;

    game_code_vtable.init_game(g_platform);

    ShowWindow(window_handle, SW_SHOW);

    while (is_running) {
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        game_code_vtable.tick_game();
    }

    game_code_vtable.shutdown_game();

    return 0;
}