#include "platform.h"
#include "memory.c"
#include "program_options.h"

// Windows include needs to happen before _win32.c includes
#define WIN32_LEAN_AND_MEAN
#define win32_MEAN_AND_LEAN
#include <windows.h>
#include <windowsx.h>

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
    if (!found_metadata) {
        return false;
    }

    if (!game_code->library || game_code->last_write_time != metadata.last_write_time) {
        new_dll_path(game_code->dll_path);
        const BOOL did_copy_file = CopyFileA(dll_path, game_code->dll_path, false);
        if (!did_copy_file) return false;

        game_code->last_write_time = metadata.last_write_time;

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
        metadata->creation_time = (u64)creation_time.dwHighDateTime << 32 | creation_time.dwLowDateTime;

        const FILETIME last_access_time = file_data.ftLastAccessTime;
        metadata->last_access_time = (u64)last_access_time.dwHighDateTime << 32 | last_access_time.dwLowDateTime;

        const FILETIME last_write_time = file_data.ftLastWriteTime;
        metadata->last_write_time = (u64)last_write_time.dwHighDateTime << 32 | last_write_time.dwLowDateTime;

        metadata->size = (int)file_data.nFileSizeLow;
        metadata->read_only = (file_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
        return true;
    }

    return false;
}

static PLATFORM_FIND_ALL_FILES_IN_DIR(win32_find_all_files_in_dir) {
    // @Cleanup paths on windows can be any len now. 
    const usize path_len = str_len(path);
    char path_final[MAX_PATH];
    mem_copy(path_final, path, path_len);
    char* path_end = path_final + path_len;

    // @Cleanup @Cleanup @Cleanup @Cleanup
    if (path_final[path_len - 1] != '\\' && path_final[path_len - 1] != '/') {
        assert(path_len < path_len + 3);
        mem_copy(path_end, "\\*\0", 3);
    } else {
        assert(path_len < path_len + 2);
        mem_copy(path_end, "*\0", 2);
    }

    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(path_final, &find_data);

    Directory_Result* first = 0;
    Directory_Result* last = 0;
    do {
        Directory_Result* const current = mem_alloc_struct(allocator, Directory_Result);
        current->next = 0;
        if (last) last->next = current;
        last = current;
        if (!first) first = current;

        File_Metadata* const metadata = &current->metadata;

        const FILETIME creation_time = find_data.ftCreationTime;
        metadata->creation_time = (u64)creation_time.dwHighDateTime << 32 | creation_time.dwLowDateTime;
        const FILETIME last_access_time = find_data.ftLastAccessTime;
        metadata->last_access_time = (u64)last_access_time.dwHighDateTime << 32 | last_access_time.dwLowDateTime;
        const FILETIME last_write_time = find_data.ftLastWriteTime;
        metadata->last_write_time = (u64)last_write_time.dwHighDateTime << 32 | last_write_time.dwLowDateTime;

        String* const built_path = &current->path;
        const b32 ends_with_slash = path[path_len - 1] == '\\' || path[path_len - 1] == '/';
        const usize built_path_allocation_size = path_len + str_len(find_data.cFileName) + ends_with_slash + 2;
        built_path->data = mem_alloc_array(allocator, u8, built_path_allocation_size);
        built_path->len = sprintf((char*)built_path->data, ends_with_slash ? "%s%s" : "%s/%s", path, find_data.cFileName);
        built_path->data[built_path->len] = 0;
        built_path->allocator = allocator;

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            current->type = DET_Directory;

            if (do_recursive && str_cmp(find_data.cFileName, ".") != 0 && str_cmp(find_data.cFileName, "..") != 0) {
                current->next = win32_find_all_files_in_dir((const char*)current->path.data, do_recursive, allocator);

                // @SPEED(colby): This kind of sucks
                while (last->next) last = last->next;
            }
        } else {
            current->type = DET_File;
            metadata->read_only = (find_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
            metadata->size = find_data.nFileSizeLow;
        }
    } while (FindNextFileA(find_handle, &find_data));

    FindClose(find_handle);

    return first;
}

static LRESULT the_window_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    switch (Msg) {
    case WM_DESTROY:
        is_running = false;
        break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        g_platform->input.keys_down[(u8)wParam] = true;
        break;
    case WM_SYSKEYUP:
    case WM_KEYUP:
        g_platform->input.keys_down[(u8)wParam] = false;
        break;
    case WM_LBUTTONDOWN:
        g_platform->input.mouse_buttons_down[MOUSE_LEFT] = true;
        SetCapture((HWND)g_platform->window_handle);
        break;
    case WM_LBUTTONUP:
        g_platform->input.mouse_buttons_down[MOUSE_LEFT] = false;
        ReleaseCapture();
        break;
    case WM_RBUTTONDOWN:
        g_platform->input.mouse_buttons_down[MOUSE_RIGHT] = true;
        SetCapture((HWND)g_platform->window_handle);
        break;
    case WM_RBUTTONUP:
        g_platform->input.mouse_buttons_down[MOUSE_RIGHT] = false;
        ReleaseCapture();
        break;
    case WM_MBUTTONDOWN:
        g_platform->input.mouse_buttons_down[MOUSE_MIDDLE] = true;
        SetCapture((HWND)g_platform->window_handle);
        break;
    case WM_MBUTTONUP:
        g_platform->input.mouse_buttons_down[MOUSE_MIDDLE] = false;
        ReleaseCapture();
        break;
    case WM_MOUSEWHEEL:
        g_platform->input.mouse_wheel_delta = GET_WHEEL_DELTA_WPARAM(wParam);
        break;
    case WM_MOUSEMOVE: {
        const int mouse_x = GET_X_LPARAM(lParam);
        const int mouse_y = GET_Y_LPARAM(lParam);
        
        g_platform->input.mouse_x = mouse_x;
        g_platform->input.mouse_y = g_platform->window_height - mouse_y;
    } break;
    case WM_SIZE:
    case WM_SIZING:
        RECT viewport = { 0 };
        GetClientRect((HWND)g_platform->window_handle, &viewport);
        g_platform->window_width  = viewport.right - viewport.left;
        g_platform->window_height = viewport.bottom - viewport.top;
        break;
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE,
    PROCESS_SYSTEM_DPI_AWARE,
    PROCESS_PER_MONITOR_DPI_AWARE
} PROCESS_DPI_AWARENESS;

typedef enum MONITOR_DPI_TYPE {
  MDT_EFFECTIVE_DPI,
  MDT_ANGULAR_DPI,
  MDT_RAW_DPI,
  MDT_DEFAULT
} MONITOR_DPI_TYPE;

typedef HRESULT (*Set_Process_DPI_Awareness)(PROCESS_DPI_AWARENESS value);
typedef HRESULT (*Get_DPI_For_Monitor)(HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT* dpiX, UINT* dpiY);

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
        .find_all_files_in_dir = win32_find_all_files_in_dir,
        .dpi_scale          = 1.f,
    };

    // DPI Scaling
    {
        HMODULE shcore = LoadLibraryA("shcore.dll");
        if (shcore) {
            Set_Process_DPI_Awareness SetProcessDpiAwareness = (Set_Process_DPI_Awareness)GetProcAddress(shcore, "SetProcessDpiAwareness");
            if (SetProcessDpiAwareness) SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);

            HMONITOR main_monitor = MonitorFromWindow(0, MONITOR_DEFAULTTOPRIMARY);
            Get_DPI_For_Monitor GetDpiForMonitor = (Get_DPI_For_Monitor)GetProcAddress(shcore, "GetDpiForMonitor");
            if (GetDpiForMonitor) {
                UINT dpiX, dpiY;
                GetDpiForMonitor(main_monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
                the_platform.dpi_scale = (f32)dpiX / 96.f;
            }

            FreeLibrary(shcore);
        }
    }

    // Create the window
    {
        HINSTANCE hInstance = GetModuleHandle(0); // @Temp

        const WNDCLASSA window_class = {
            .lpfnWndProc    = the_window_proc,
            .hInstance      = hInstance,
            .hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1),
            .lpszClassName  = WINDOW_TITLE " window class",
            .hCursor        = LoadCursor(0, IDC_ARROW),
            .style          = CS_OWNDC,
        };
        
        RegisterClassA(&window_class);
        
        const int dpi_scaled_width  = (int)((f32)WINDOW_WIDTH * the_platform.dpi_scale);
        const int dpi_scaled_height = (int)((f32)WINDOW_HEIGHT * the_platform.dpi_scale);

        RECT adjusted_rect = { 0, 0, dpi_scaled_width, dpi_scaled_height };
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
        the_platform.window_width   = width;
        the_platform.window_height  = height;
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

        // Reset input state
        g_platform->input.mouse_dx = 0;
        g_platform->input.mouse_dy = 0;
        g_platform->input.mouse_wheel_delta = 0;

        // Poll input from the window
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
