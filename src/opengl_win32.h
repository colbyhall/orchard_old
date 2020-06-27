#ifndef OPENGL_WIN32_H
#define OPENGL_WIN32_H

#include <gl/Gl.h>
#include <gl/wglext.h>
#include "language_layer.h"

typedef HGLRC(__cdecl *WGL_Create_Context_Attribs_ARB)(HDC hdc, HGLRC share_context, const s32* attrib_list);
typedef b32 (__cdecl *WGL_Choose_Pixel_Format_ARB)(HDC hdc, const s32* int_attrib_list, const f32* float_attrib_list, u32 max_formats, s32* int_formats, u32* num_formats);
typedef b32 (__cdecl *WGL_Swap_Interval_Ext)(s32 interval);

extern WGL_Swap_Interval_Ext wglSwapIntervalEXT;

#endif /* OPENGL_WIN32_H */