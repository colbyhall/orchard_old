#include "opengl.h"

#if PLATFORM_WINDOWS
#include "opengl_win32.c"
#else
#error OpenGL platform implementation missing for this OS
#endif

#define DEFINE_GL_FUNCTIONS(type, func) type func = 0;
GL_BINDINGS(DEFINE_GL_FUNCTIONS)
#undef  DEFINE_GL_FUNCTIONS