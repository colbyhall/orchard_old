#include "opengl.h"

WGL_Create_Context_Attribs_ARB wglCreateContextAttribsARB = 0;
WGL_Choose_Pixel_Format_ARB wglChoosePixelFormatARB = 0;
WGL_Swap_Interval_Ext wglSwapIntervalEXT = 0;

#define LOAD_GL_BINDINGS(type, func) func = (type)wglGetProcAddress(#func);
#define CHECK_GL_BINDINGS(type, func) if (!func) return false;

extern OpenGL_Context* g_gl_context;

b32 init_opengl(Platform* platform) {
    Allocator arena = platform->permanent_arena;

    g_gl_context = mem_alloc_struct(arena, OpenGL_Context);

    if (g_gl_context->is_initialized) {
        GL_BINDINGS(LOAD_GL_BINDINGS);
        GL_BINDINGS(CHECK_GL_BINDINGS);
        return true;
    }

    // @NOTE(colby): Create a fake window to grab the extended gl functions
    {
        WNDCLASSA window_class = (WNDCLASSA) { 
            .lpfnWndProc = DefWindowProc,
            .hInstance = GetModuleHandleA(0),
            .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
            .lpszClassName = "gl_class",
            .hCursor = LoadCursor(0, IDC_ARROW),
            .style = CS_OWNDC,
        };

        if (!RegisterClassA(&window_class)) return false;

        HWND gl_window_handle = CreateWindowA(window_class.lpszClassName, "gl_window", WS_OVERLAPPEDWINDOW, 0, 0, 200, 200, 0, 0, window_class.hInstance, 0);

        if (gl_window_handle == INVALID_HANDLE_VALUE) return false;

        HDC window_context = GetDC(gl_window_handle);

        PIXELFORMATDESCRIPTOR dpf = (PIXELFORMATDESCRIPTOR) {
            .nSize = sizeof(dpf),
            .nVersion = 1,
            .iPixelType = PFD_TYPE_RGBA,
            .dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER,
            .cColorBits = 32,
            .cDepthBits = 24,
            .cAlphaBits = 8,
            .iLayerType = PFD_MAIN_PLANE,
        };

        const s32 suggested_pixel_format_index = ChoosePixelFormat(window_context, &dpf);

        DescribePixelFormat(window_context, suggested_pixel_format_index, sizeof(dpf), &dpf);
        SetPixelFormat(window_context, suggested_pixel_format_index, &dpf);

        HGLRC glrc = wglCreateContext(window_context);

        if (wglMakeCurrent(window_context, glrc)) {
            wglChoosePixelFormatARB = (WGL_Choose_Pixel_Format_ARB)wglGetProcAddress("wglChoosePixelFormatARB");
            wglCreateContextAttribsARB = (WGL_Create_Context_Attribs_ARB)wglGetProcAddress("wglCreateContextAttribsARB");
            wglSwapIntervalEXT = (WGL_Swap_Interval_Ext)wglGetProcAddress("wglSwapIntervalEXT");

            GL_BINDINGS(LOAD_GL_BINDINGS);
            GL_BINDINGS(CHECK_GL_BINDINGS);

            wglMakeCurrent(window_context, 0);
        }

        wglDeleteContext(glrc);
        ReleaseDC(gl_window_handle, window_context);
        DestroyWindow(gl_window_handle);
    }

    // @NOTE(colby): Setup our window from the game state for gl and make it gl current
    HWND window_handle = platform->window_handle;
    HDC window_context = GetDC(window_handle);

    const s32 attrib_list[] =
    {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
        0,
    };

    s32 pixel_format = 0;
    u32 num_formats;
    wglChoosePixelFormatARB(window_context, attrib_list, 0, 1, &pixel_format, &num_formats);

    PIXELFORMATDESCRIPTOR spf;
    DescribePixelFormat(window_context, pixel_format, sizeof(spf), &spf);
    SetPixelFormat(window_context, pixel_format, &spf);

    const s32 win32_opengl_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
    };

    HGLRC glrc = wglCreateContextAttribsARB(window_context, 0, win32_opengl_attribs);
    if (!wglMakeCurrent(window_context, glrc)) return false;
    ReleaseDC(window_handle, window_context);

    glGetIntegerv(GL_MAJOR_VERSION, &g_gl_context->maj_version);
    glGetIntegerv(GL_MINOR_VERSION, &g_gl_context->min_version);

    g_gl_context->is_initialized = true;

    o_log_verbose("[OpenGL] Loaded opengl with version %i:%i.", g_gl_context->maj_version, g_gl_context->min_version);

    return true;
}

void swap_gl_buffers(Platform* platform) {
    HDC window_context = GetDC(platform->window_handle);
    SwapBuffers(window_context);
    ReleaseDC((HWND)platform->window_handle, window_context);
}

#undef LOAD_GL_BINDINGS
#undef CHECK_GL_BINDINGS