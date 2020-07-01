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
#include "draw.c"

Platform* g_platform = 0;

DLL_EXPORT void init_game(Platform* platform) {
    g_platform = platform;

    init_opengl(platform);
    init_imm_renderer(platform->permanent_arena);
}

DLL_EXPORT void tick_game(f32 dt) {
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.2f, 0.2f, 0.2f, 1.f);

    const Rect viewport = { v2z(), v2((f32)g_platform->window_width, (f32)g_platform->window_height) };
    imm_render_right_handed(viewport);

    imm_begin();
    imm_rect(rect_from_raw(0.f, 0.f, 100.f, 100.f), -5.f, v4(1.f, 1.f, 1.f, 1.f));
    imm_flush();

    swap_gl_buffers(g_platform);
}

DLL_EXPORT void shutdown_game(void) {

}