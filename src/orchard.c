#include "platform.h"

// Include the OS stuff before any implementations
#if PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define WIN32_MEAN_AND_LEAN
#define NOMINMAX

#include <windows.h>

#undef WIN32_LEAN_AND_MEAN
#undef WIN32_MEAN_AND_LEAN
#undef NOMINMAX
#undef far
#undef near
#undef FAR
#undef NEAR
#else
#error Platform not yet implemented.
#endif

#include "memory.c"
#include "math.c"
#include "opengl.c"

Platform* g_platform = 0;

DLL_EXPORT void init_game(Platform* platform) {
    g_platform = platform;

    init_opengl(platform);
}

DLL_EXPORT void tick_game(void) {

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(1.f, 0.f, 0.2f, 1.f);

    {
        HDC window_context = GetDC(g_platform->window_handle);
        SwapBuffers(window_context);
        ReleaseDC((HWND)g_platform->window_handle, window_context);
    }
}

DLL_EXPORT void shutdown_game(void) {

}