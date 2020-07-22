#include "platform.h"
#include "memory.c"
#include "program_options.h"
#include "string.c"

// Windows include needs to happen before _win32.c includes
#define WIN32_LEAN_AND_MEAN
#define win32_MEAN_AND_LEAN
#include <windows.h>
#include <windowsx.h>

Platform* g_platform;

static b32 is_running = true;
static LARGE_INTEGER g_qpc_freq;

// Use discrete GPU
__declspec(dllexport) DWORD NvOptimusEnablement = 0x01;
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x01;

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

static char* game_code_vtable_names[] = {
    "init_game",
    "tick_game",
    "shutdown_game",
};

static char* dll_path = "bin/game_module.dll";

static void new_dll_path(char* buffer) {
    static int last = 0;
    sprintf(buffer, "bin\\temp_game_module_%i.dll", last);
    last += 1;
}

static b32 try_reload_dll(Game_Code* game_code) {
    File_Metadata metadata;
    b32 found_metadata = g_platform->file_metadata(from_cstr(dll_path), &metadata);
    if (!found_metadata) {
        return false;
    }

    if (!game_code->library || game_code->last_write_time != metadata.last_write_time) {
        new_dll_path(game_code->dll_path);
        BOOL did_copy_file = CopyFileA(dll_path, game_code->dll_path, false);
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
    b32 read   = flags & FF_Read;
    b32 write  = flags & FF_Write;
    b32 create = flags & FF_Create;

    assert(read || write);

    DWORD desired_access = 0;
    if (read)  desired_access |= GENERIC_READ;
    if (write) desired_access |= GENERIC_WRITE;

    DWORD creation = OPEN_EXISTING;
    if (create) creation = OPEN_ALWAYS;

    // Try to create the file
    void* os_handle = CreateFileA((char*)path.data, desired_access, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, creation, FILE_ATTRIBUTE_NORMAL, 0);
    b32 is_open = os_handle != INVALID_HANDLE_VALUE;

    // If we actually opened the file then fill out the handle data
    if (is_open) {
        *handle = (File_Handle) { os_handle, path, flags };
        return true;
    }
    
    return false;
}

static PLATFORM_CLOSE_FILE(win32_close_file) {
    if (!handle->os_handle) return false;

    b32 result = CloseHandle(handle->os_handle) == 1;
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
    if (GetFileAttributesExA((char*)path.data, GetFileExInfoStandard, &file_data)) {
        if (!metadata) return true;
        
        FILETIME creation_time = file_data.ftCreationTime;
        metadata->creation_time = (u64)creation_time.dwHighDateTime << 32 | creation_time.dwLowDateTime;

        FILETIME last_access_time = file_data.ftLastAccessTime;
        metadata->last_access_time = (u64)last_access_time.dwHighDateTime << 32 | last_access_time.dwLowDateTime;

        FILETIME last_write_time = file_data.ftLastWriteTime;
        metadata->last_write_time = (u64)last_write_time.dwHighDateTime << 32 | last_write_time.dwLowDateTime;

        metadata->size = (int)file_data.nFileSizeLow;
        metadata->read_only = (file_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
        return true;
    }

    return false;
}

static PLATFORM_FIND_ALL_FILES_IN_DIR(win32_find_all_files_in_dir) {
    // @Cleanup paths on windows can be any len now. 
    usize path_len = (usize)path.len;
    char path_final[MAX_PATH];
    mem_copy(path_final, path.data, path_len);
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
        Directory_Result* current = mem_alloc_struct(allocator, Directory_Result);
        current->next = 0;
        if (last) last->next = current;
        last = current;
        if (!first) first = current;

        File_Metadata* metadata = &current->metadata;

        FILETIME creation_time = find_data.ftCreationTime;
        metadata->creation_time = (u64)creation_time.dwHighDateTime << 32 | creation_time.dwLowDateTime;
        FILETIME last_access_time = find_data.ftLastAccessTime;
        metadata->last_access_time = (u64)last_access_time.dwHighDateTime << 32 | last_access_time.dwLowDateTime;
        FILETIME last_write_time = find_data.ftLastWriteTime;
        metadata->last_write_time = (u64)last_write_time.dwHighDateTime << 32 | last_write_time.dwLowDateTime;

        String* built_path = &current->path;
        b32 ends_with_slash = path.data[path_len - 1] == '\\' || path.data[path_len - 1] == '/';
        usize built_path_allocation_size = path_len + str_len(find_data.cFileName) + ends_with_slash + 2;
        built_path->data = mem_alloc_array(allocator, u8, built_path_allocation_size);
        built_path->len = sprintf((char*)built_path->data, ends_with_slash ? "%s%s" : "%s/%s", (char*)path.data, find_data.cFileName);
        built_path->data[built_path->len] = 0;
        built_path->allocator = allocator;

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            current->type = DET_Directory;

            if (do_recursive && str_cmp(find_data.cFileName, ".") != 0 && str_cmp(find_data.cFileName, "..") != 0) {
                current->next = win32_find_all_files_in_dir(current->path, do_recursive, allocator);

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

static PLATFORM_CREATE_DIRECTORY(win32_create_directory) {
    return CreateDirectoryA((char*)path.data, 0);
}

static PLATFORM_LOCAL_TIME(win32_local_time) {
    SYSTEMTIME time;
    GetLocalTime(&time);

    return (System_Time) {
        time.wYear,
        time.wMonth,
        time.wDayOfWeek,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds,
    };
}

static PLATFORM_CYCLES(win32_cycles) {
    return __rdtsc();
}

static PLATFORM_TIME_IN_SECONDS(win32_time_in_seconds) {
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);

    return (f64)time.QuadPart / (f64)g_qpc_freq.QuadPart;
}

#define push_os_event(t, ...) g_platform->events[g_platform->num_events++] = (OS_Event) { .type = t, __VA_ARGS__ }

static LRESULT the_window_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    switch (Msg) {
    case WM_DESTROY:
        is_running = false;
        break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
        u8 key = (u8)wParam;
        g_platform->input.state.keys_down[key] = true;
        push_os_event(OET_Key_Pressed, .key = key);
    } break;
    case WM_SYSKEYUP:
    case WM_KEYUP: {
        u8 key = (u8)wParam;
        g_platform->input.state.keys_down[key] = false;
        push_os_event(OET_Key_Released, .key = key);
    } break;
    case WM_LBUTTONDOWN:
        g_platform->input.state.mouse_buttons_down[MOUSE_LEFT] = true;
        SetCapture((HWND)g_platform->window_handle);
        push_os_event(OET_Mouse_Button_Pressed, .mouse_button = MOUSE_LEFT);
        break;
    case WM_LBUTTONUP:
        g_platform->input.state.mouse_buttons_down[MOUSE_LEFT] = false;
        ReleaseCapture();
        push_os_event(OET_Mouse_Button_Released, .mouse_button = MOUSE_LEFT);
        break;
    case WM_RBUTTONDOWN:
        g_platform->input.state.mouse_buttons_down[MOUSE_RIGHT] = true;
        SetCapture((HWND)g_platform->window_handle);
        push_os_event(OET_Mouse_Button_Pressed, .mouse_button = MOUSE_RIGHT);
        break;
    case WM_RBUTTONUP:
        g_platform->input.state.mouse_buttons_down[MOUSE_RIGHT] = false;
        ReleaseCapture();
        push_os_event(OET_Mouse_Button_Released, .mouse_button = MOUSE_RIGHT);
        break;
    case WM_MBUTTONDOWN:
        g_platform->input.state.mouse_buttons_down[MOUSE_MIDDLE] = true;
        SetCapture((HWND)g_platform->window_handle);
        push_os_event(OET_Mouse_Button_Pressed, .mouse_button = MOUSE_MIDDLE);
        break;
    case WM_MBUTTONUP:
        g_platform->input.state.mouse_buttons_down[MOUSE_MIDDLE] = false;
        ReleaseCapture();
        push_os_event(OET_Mouse_Button_Released, .mouse_button = MOUSE_MIDDLE);
        break;
    case WM_MOUSEWHEEL:
        g_platform->input.state.mouse_wheel_delta = GET_WHEEL_DELTA_WPARAM(wParam);
        break;
    case WM_MOUSEMOVE: {
        int mouse_x = GET_X_LPARAM(lParam);
        int mouse_y = g_platform->window_height - GET_Y_LPARAM(lParam);
        
        g_platform->input.state.mouse_x = mouse_x;
        g_platform->input.state.mouse_y = mouse_y;

        g_platform->input.state.mouse_dx = mouse_x - g_platform->input.prev_state.mouse_x;
        g_platform->input.state.mouse_dy = mouse_y - g_platform->input.prev_state.mouse_y;
    } break;
    case WM_SIZE:
    case WM_SIZING:
        RECT viewport = { 0 };
        GetClientRect((HWND)g_platform->window_handle, &viewport);
        int old_width = g_platform->window_width;
        int old_height = g_platform->window_height;
        g_platform->window_width  = viewport.right - viewport.left;
        g_platform->window_height = viewport.bottom - viewport.top;
        push_os_event(OET_Window_Resized, .old_width = old_width, .old_height = old_height);
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
    Allocator permanent_arena = arena_allocator_raw(
        VirtualAlloc(0, PERMANENT_MEMORY_CAP, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE),
        PERMANENT_MEMORY_CAP
    );
    
    // This is a different virtual alloc because we need to rese the permanent allocator completely when doing a hot reload
    Allocator frame_arena = arena_allocator_raw(
        VirtualAlloc(0, FRAME_MEMORY_CAP, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE),
        FRAME_MEMORY_CAP
    );

    // Move us to project base dir
    {
        char buffer[512];
        DWORD len = GetModuleFileNameA(0, buffer, 512);
        for (u32 i = len - 1; i > 0; --i) {
            char c = buffer[i];
            if (c == '\\' || c == '/') {
                buffer[i] = 0;
                break;
            }
        }
        SetCurrentDirectoryA(buffer);
        SetCurrentDirectoryA("..");
    }

    QueryPerformanceFrequency(&g_qpc_freq);

    Platform the_platform = {
        .permanent_arena    = permanent_arena,
        .frame_arena        = frame_arena,
        .open_file          = win32_open_file,
        .close_file         = win32_close_file,
        .read_file          = win32_read_file,
        .write_file         = win32_write_file,
        .file_metadata      = win32_file_metadata,
        .find_all_files_in_dir = win32_find_all_files_in_dir,
        .create_directory   = win32_create_directory,
        .local_time         = win32_local_time,
        .cycles             = win32_cycles,
        .time_in_seconds    = win32_time_in_seconds,
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

        WNDCLASSA window_class = {
            .lpfnWndProc    = the_window_proc,
            .hInstance      = hInstance,
            .hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1),
            .lpszClassName  = WINDOW_TITLE " window class",
            .hCursor        = LoadCursor(0, IDC_ARROW),
            .style          = CS_OWNDC,
        };
        
        RegisterClassA(&window_class);
        
        int dpi_scaled_width  = (int)((f32)WINDOW_WIDTH * the_platform.dpi_scale);
        int dpi_scaled_height = (int)((f32)WINDOW_HEIGHT * the_platform.dpi_scale);

        RECT adjusted_rect = { 0, 0, dpi_scaled_width, dpi_scaled_height };
        AdjustWindowRect(&adjusted_rect, WS_OVERLAPPEDWINDOW, 0);
        
        int width = adjusted_rect.right - adjusted_rect.left;
        int height = adjusted_rect.bottom - adjusted_rect.top;

        int monitor_width = GetSystemMetrics(SM_CXSCREEN);
        int monitor_height = GetSystemMetrics(SM_CYSCREEN);
        
        int x = monitor_width / 2 - width / 2;
        int y = monitor_height / 2 - height / 2;
        
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
        .function_count = array_count(game_code_vtable_names),
        .functions      = (void**)&game_code_vtable,
    };
    b32 loaded_game_code = try_reload_dll(&game_code);
    assert(loaded_game_code);

    game_code_vtable.init_game(g_platform);
    reset_arena(the_platform.frame_arena);

    ShowWindow(the_platform.window_handle, SW_SHOW);

    LARGE_INTEGER last_frame_time;
    QueryPerformanceCounter(&last_frame_time);
    while (is_running) {
        LARGE_INTEGER current_frame_time;
        QueryPerformanceCounter(&current_frame_time);

        // Update frame timing
        g_platform->last_frame_time     = last_frame_time.QuadPart / (f64)g_qpc_freq.QuadPart;
        g_platform->current_frame_time  = current_frame_time.QuadPart / (f64)g_qpc_freq.QuadPart;
        last_frame_time = current_frame_time;

        g_platform->input.prev_state = g_platform->input.state;

        // Reset input state
        g_platform->input.state.mouse_dx = 0;
        g_platform->input.state.mouse_dy = 0;
        g_platform->input.state.mouse_wheel_delta = 0;
        g_platform->num_events = 0;

        // Poll input from the window
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        f32 dt  = (f32)(g_platform->current_frame_time - g_platform->last_frame_time);
        game_code_vtable.tick_game(dt);
        reset_arena(the_platform.frame_arena);

        // Do a hot reload if it can
        if (try_reload_dll(&game_code)) {
            reset_arena(the_platform.permanent_arena);
            game_code_vtable.init_game(g_platform);
            reset_arena(the_platform.frame_arena);
        }
    }

    game_code_vtable.shutdown_game();

    return 0;
}
