#ifndef OPENGL_H
#define OPENGL_H

#include "platform.h"

// The goal is to eventually have a simple wrapper api around all graphics api's. After that we 
// have that we can have seperate builds for certain graphics api's. In doing that we will want
// all opengl methods to eventually be just in the implementation file instead of the header
// - Colby 6/31/20

#if PLATFORM_WINDOWS
#include "opengl_win32.h"
#endif

#include <gl/glext.h>

#define GL_BINDINGS(macro) \
macro(PFNGLBINDSAMPLERPROC, glBindSampler) \
macro(PFNGLDELETESAMPLERSPROC, glDeleteSamplers) \
macro(PFNGLGENSAMPLERSPROC, glGenSamplers) \
macro(PFNGLSAMPLERPARAMETERIPROC, glSamplerParameteri) \
macro(PFNGLCLEARBUFFERFVPROC, glClearBufferfv) \
macro(PFNGLCLEARBUFFERIVPROC, glClearBufferiv) \
macro(PFNGLCLEARBUFFERUIVPROC, glClearBufferuiv) \
macro(PFNGLCLEARBUFFERFIPROC, glClearBufferfi) \
macro(PFNGLCOLORMASKIPROC, glColorMaski) \
macro(PFNGLDISABLEIPROC, glDisablei) \
macro(PFNGLENABLEIPROC, glEnablei) \
macro(PFNGLGETBOOLEANI_VPROC, glGetBooleani_v) \
macro(PFNGLGETINTEGERI_VPROC, glGetIntegeri_v) \
macro(PFNGLISENABLEDIPROC, glIsEnabledi) \
macro(PFNGLBLENDCOLORPROC, glBlendColor) \
macro(PFNGLBLENDEQUATIONPROC, glBlendEquation) \
macro(PFNGLDRAWRANGEELEMENTSPROC, glDrawRangeElements) \
macro(PFNGLTEXIMAGE3DPROC, glTexImage3D) \
macro(PFNGLTEXSUBIMAGE3DPROC, glTexSubImage3D) \
macro(PFNGLCOPYTEXSUBIMAGE3DPROC, glCopyTexSubImage3D) \
macro(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
macro(PFNGLSAMPLECOVERAGEPROC, glSampleCoverage) \
macro(PFNGLCOMPRESSEDTEXIMAGE3DPROC, glCompressedTexImage3D) \
macro(PFNGLCOMPRESSEDTEXIMAGE2DPROC, glCompressedTexImage2D) \
macro(PFNGLCOMPRESSEDTEXIMAGE1DPROC, glCompressedTexImage1D) \
macro(PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC, glCompressedTexSubImage3D) \
macro(PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC, glCompressedTexSubImage2D) \
macro(PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC, glCompressedTexSubImage1D) \
macro(PFNGLGETCOMPRESSEDTEXIMAGEPROC, glGetCompressedTexImage) \
macro(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) \
macro(PFNGLMULTIDRAWARRAYSPROC, glMultiDrawArrays) \
macro(PFNGLMULTIDRAWELEMENTSPROC, glMultiDrawElements) \
macro(PFNGLPOINTPARAMETERFPROC, glPointParameterf) \
macro(PFNGLPOINTPARAMETERFVPROC, glPointParameterfv) \
macro(PFNGLPOINTPARAMETERIPROC, glPointParameteri) \
macro(PFNGLPOINTPARAMETERIVPROC, glPointParameteriv) \
macro(PFNGLGENQUERIESPROC, glGenQueries) \
macro(PFNGLDELETEQUERIESPROC, glDeleteQueries) \
macro(PFNGLISQUERYPROC, glIsQuery) \
macro(PFNGLBEGINQUERYPROC, glBeginQuery) \
macro(PFNGLENDQUERYPROC, glEndQuery) \
macro(PFNGLGETQUERYIVPROC, glGetQueryiv) \
macro(PFNGLGETQUERYOBJECTIVPROC, glGetQueryObjectiv) \
macro(PFNGLGETQUERYOBJECTUIVPROC, glGetQueryObjectuiv) \
macro(PFNGLBINDBUFFERPROC, glBindBuffer) \
macro(PFNGLBINDBUFFERBASEPROC, glBindBufferBase) \
macro(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) \
macro(PFNGLGENBUFFERSPROC, glGenBuffers) \
macro(PFNGLISBUFFERPROC, glIsBuffer) \
macro(PFNGLBUFFERDATAPROC, glBufferData) \
macro(PFNGLBUFFERSUBDATAPROC, glBufferSubData) \
macro(PFNGLGETBUFFERSUBDATAPROC, glGetBufferSubData) \
macro(PFNGLMAPBUFFERPROC, glMapBuffer) \
macro(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) \
macro(PFNGLGETBUFFERPARAMETERIVPROC, glGetBufferParameteriv) \
macro(PFNGLGETBUFFERPOINTERVPROC, glGetBufferPointerv) \
macro(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) \
macro(PFNGLDRAWBUFFERSPROC, glDrawBuffers) \
macro(PFNGLSTENCILOPSEPARATEPROC, glStencilOpSeparate) \
macro(PFNGLSTENCILFUNCSEPARATEPROC, glStencilFuncSeparate) \
macro(PFNGLSTENCILMASKSEPARATEPROC, glStencilMaskSeparate) \
macro(PFNGLATTACHSHADERPROC, glAttachShader) \
macro(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
macro(PFNGLBINDFRAGDATALOCATIONPROC, glBindFragDataLocation) \
macro(PFNGLCOMPILESHADERPROC, glCompileShader) \
macro(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
macro(PFNGLCREATESHADERPROC, glCreateShader) \
macro(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
macro(PFNGLDELETESHADERPROC, glDeleteShader) \
macro(PFNGLDETACHSHADERPROC, glDetachShader) \
macro(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) \
macro(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
macro(PFNGLGETACTIVEATTRIBPROC, glGetActiveAttrib) \
macro(PFNGLGETACTIVEUNIFORMPROC, glGetActiveUniform) \
macro(PFNGLGETATTACHEDSHADERSPROC, glGetAttachedShaders) \
macro(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation) \
macro(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
macro(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
macro(PFNGLGETSHADERIVPROC, glGetShaderiv) \
macro(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
macro(PFNGLGETSHADERSOURCEPROC, glGetShaderSource) \
macro(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
macro(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex) \
macro(PFNGLGETUNIFORMFVPROC, glGetUniformfv) \
macro(PFNGLGETUNIFORMIVPROC, glGetUniformiv) \
macro(PFNGLGETVERTEXATTRIBDVPROC, glGetVertexAttribdv) \
macro(PFNGLGETVERTEXATTRIBFVPROC, glGetVertexAttribfv) \
macro(PFNGLGETVERTEXATTRIBIVPROC, glGetVertexAttribiv) \
macro(PFNGLGETVERTEXATTRIBPOINTERVPROC, glGetVertexAttribPointerv) \
macro(PFNGLISPROGRAMPROC, glIsProgram) \
macro(PFNGLISSHADERPROC, glIsShader) \
macro(PFNGLLINKPROGRAMPROC, glLinkProgram) \
macro(PFNGLSHADERSOURCEPROC, glShaderSource) \
macro(PFNGLUSEPROGRAMPROC, glUseProgram) \
macro(PFNGLUNIFORM1FPROC, glUniform1f) \
macro(PFNGLUNIFORM2FPROC, glUniform2f) \
macro(PFNGLUNIFORM3FPROC, glUniform3f) \
macro(PFNGLUNIFORM4FPROC, glUniform4f) \
macro(PFNGLUNIFORM1IPROC, glUniform1i) \
macro(PFNGLUNIFORM2IPROC, glUniform2i) \
macro(PFNGLUNIFORM3IPROC, glUniform3i) \
macro(PFNGLUNIFORM4IPROC, glUniform4i) \
macro(PFNGLUNIFORM1FVPROC, glUniform1fv) \
macro(PFNGLUNIFORM2FVPROC, glUniform2fv) \
macro(PFNGLUNIFORM3FVPROC, glUniform3fv) \
macro(PFNGLUNIFORM4FVPROC, glUniform4fv) \
macro(PFNGLUNIFORM1IVPROC, glUniform1iv) \
macro(PFNGLUNIFORM2IVPROC, glUniform2iv) \
macro(PFNGLUNIFORM3IVPROC, glUniform3iv) \
macro(PFNGLUNIFORM4IVPROC, glUniform4iv) \
macro(PFNGLUNIFORM1UIVPROC, glUniform1uiv) \
macro(PFNGLUNIFORM2UIVPROC, glUniform2uiv) \
macro(PFNGLUNIFORM3UIVPROC, glUniform3uiv) \
macro(PFNGLUNIFORM4UIVPROC, glUniform4uiv) \
macro(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding) \
macro(PFNGLUNIFORMMATRIX2FVPROC, glUniformMatrix2fv) \
macro(PFNGLUNIFORMMATRIX3FVPROC, glUniformMatrix3fv) \
macro(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
macro(PFNGLVALIDATEPROGRAMPROC, glValidateProgram) \
macro(PFNGLVERTEXATTRIB1DPROC, glVertexAttrib1d) \
macro(PFNGLVERTEXATTRIB1DVPROC, glVertexAttrib1dv) \
macro(PFNGLVERTEXATTRIB1FPROC, glVertexAttrib1f) \
macro(PFNGLVERTEXATTRIB1FVPROC, glVertexAttrib1fv) \
macro(PFNGLVERTEXATTRIB1SPROC, glVertexAttrib1s) \
macro(PFNGLVERTEXATTRIB1SVPROC, glVertexAttrib1sv) \
macro(PFNGLVERTEXATTRIB2DPROC, glVertexAttrib2d) \
macro(PFNGLVERTEXATTRIB2DVPROC, glVertexAttrib2dv) \
macro(PFNGLVERTEXATTRIB2FPROC, glVertexAttrib2f) \
macro(PFNGLVERTEXATTRIB2FVPROC, glVertexAttrib2fv) \
macro(PFNGLVERTEXATTRIB2SPROC, glVertexAttrib2s) \
macro(PFNGLVERTEXATTRIB2SVPROC, glVertexAttrib2sv) \
macro(PFNGLVERTEXATTRIB3DPROC, glVertexAttrib3d) \
macro(PFNGLVERTEXATTRIB3DVPROC, glVertexAttrib3dv) \
macro(PFNGLVERTEXATTRIB3FPROC, glVertexAttrib3f) \
macro(PFNGLVERTEXATTRIB3FVPROC, glVertexAttrib3fv) \
macro(PFNGLVERTEXATTRIB3SPROC, glVertexAttrib3s) \
macro(PFNGLVERTEXATTRIB3SVPROC, glVertexAttrib3sv) \
macro(PFNGLVERTEXATTRIB4NBVPROC, glVertexAttrib4Nbv) \
macro(PFNGLVERTEXATTRIB4NIVPROC, glVertexAttrib4Niv) \
macro(PFNGLVERTEXATTRIB4NSVPROC, glVertexAttrib4Nsv) \
macro(PFNGLVERTEXATTRIB4NUBPROC, glVertexAttrib4Nub) \
macro(PFNGLVERTEXATTRIB4NUBVPROC, glVertexAttrib4Nubv) \
macro(PFNGLVERTEXATTRIB4NUIVPROC, glVertexAttrib4Nuiv) \
macro(PFNGLVERTEXATTRIB4NUSVPROC, glVertexAttrib4Nusv) \
macro(PFNGLVERTEXATTRIB4BVPROC, glVertexAttrib4bv) \
macro(PFNGLVERTEXATTRIB4DPROC, glVertexAttrib4d) \
macro(PFNGLVERTEXATTRIB4DVPROC, glVertexAttrib4dv) \
macro(PFNGLVERTEXATTRIB4FPROC, glVertexAttrib4f) \
macro(PFNGLVERTEXATTRIB4FVPROC, glVertexAttrib4fv) \
macro(PFNGLVERTEXATTRIB4IVPROC, glVertexAttrib4iv) \
macro(PFNGLVERTEXATTRIB4SPROC, glVertexAttrib4s) \
macro(PFNGLVERTEXATTRIB4SVPROC, glVertexAttrib4sv) \
macro(PFNGLVERTEXATTRIB4UBVPROC, glVertexAttrib4ubv) \
macro(PFNGLVERTEXATTRIB4UIVPROC, glVertexAttrib4uiv) \
macro(PFNGLVERTEXATTRIB4USVPROC, glVertexAttrib4usv) \
macro(PFNGLVERTEXATTRIBI4IVPROC, glVertexAttribI4iv) \
macro(PFNGLVERTEXATTRIBI4UIVPROC, glVertexAttribI4uiv) \
macro(PFNGLVERTEXATTRIBI4SVPROC, glVertexAttribI4sv) \
macro(PFNGLVERTEXATTRIBI4USVPROC, glVertexAttribI4usv) \
macro(PFNGLVERTEXATTRIBI4BVPROC, glVertexAttribI4bv) \
macro(PFNGLVERTEXATTRIBI4UBVPROC, glVertexAttribI4ubv) \
macro(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
macro(PFNGLVERTEXATTRIBIPOINTERPROC, glVertexAttribIPointer) \
macro(PFNGLUNIFORMMATRIX2X3FVPROC, glUniformMatrix2x3fv) \
macro(PFNGLUNIFORMMATRIX3X2FVPROC, glUniformMatrix3x2fv) \
macro(PFNGLUNIFORMMATRIX2X4FVPROC, glUniformMatrix2x4fv) \
macro(PFNGLUNIFORMMATRIX4X2FVPROC, glUniformMatrix4x2fv) \
macro(PFNGLUNIFORMMATRIX3X4FVPROC, glUniformMatrix3x4fv) \
macro(PFNGLUNIFORMMATRIX4X3FVPROC, glUniformMatrix4x3fv) \
macro(PFNGLISRENDERBUFFERPROC, glIsRenderbuffer) \
macro(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
macro(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) \
macro(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
macro(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) \
macro(PFNGLGETRENDERBUFFERPARAMETERIVPROC, glGetRenderbufferParameteriv) \
macro(PFNGLISFRAMEBUFFERPROC, glIsFramebuffer) \
macro(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
macro(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
macro(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
macro(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
macro(PFNGLFRAMEBUFFERTEXTURE1DPROC, glFramebufferTexture1D) \
macro(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
macro(PFNGLFRAMEBUFFERTEXTURE3DPROC, glFramebufferTexture3D) \
macro(PFNGLFRAMEBUFFERTEXTUREPROC, glFramebufferTexture) \
macro(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
macro(PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC, glGetFramebufferAttachmentParameteriv) \
macro(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap) \
macro(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) \
macro(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) \
macro(PFNGLFRAMEBUFFERTEXTURELAYERPROC, glFramebufferTextureLayer) \
macro(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange) \
macro(PFNGLFLUSHMAPPEDBUFFERRANGEPROC, glFlushMappedBufferRange) \
macro(PFNGLVERTEXATTRIBDIVISORPROC, glVertexAttribDivisor) \
macro(PFNGLDRAWARRAYSINSTANCEDPROC, glDrawArraysInstanced) \
macro(PFNGLDRAWELEMENTSINSTANCEDPROC, glDrawElementsInstanced) \
macro(PFNGLGETSTRINGIPROC, glGetStringi) \
macro(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
macro(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) \
macro(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
macro(PFNGLCOPYBUFFERSUBDATAPROC, glCopyBufferSubData) \
macro(PFNGLTEXBUFFERPROC, glTexBuffer) \
macro(PFNGLTEXIMAGE2DMULTISAMPLEPROC, glTexImage2DMultisample) \
macro(PFNGLQUERYCOUNTERPROC, glQueryCounter)\
macro(PFNGLISSYNCPROC, glIsSync)\
macro(PFNGLDELETESYNCPROC, glDeleteSync)\
macro(PFNGLGETQUERYOBJECTUI64VPROC, glGetQueryObjectui64v)\
macro(PFNGLFENCESYNCPROC, glFenceSync)\
macro(PFNGLGETSYNCIVPROC, glGetSynciv)\
macro(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync)\
macro(PFNGLBINDBUFFERRANGEPROC, glBindBufferRange)\
macro(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback)

#define DECLARE_GL_FUNCTIONS(type, func) extern type func;
GL_BINDINGS(DECLARE_GL_FUNCTIONS)
#undef  DECLARE_GL_FUNCTIONS

b32 init_opengl(Platform* platform);
void swap_gl_buffers(Platform* platform);

typedef struct Texture2d {
    u8* pixels;
    int width, height, depth;

    GLuint id;
} Texture2d;

b32 upload_texture2d(Texture2d* t);

#define SHADER_UNFORM_NAME_CAP 48
typedef struct Shader_Uniform {
    GLchar name[SHADER_UNFORM_NAME_CAP];
    int name_len;

    GLenum type;
    GLint location;
} Shader_Uniform;

#define SHADER_UNIFORM_CAP 16
typedef struct Shader {
    GLuint id;

    String source;

    Shader_Uniform uniforms[SHADER_UNIFORM_CAP];
    int uniform_count;
} Shader;

b32 init_shader(Shader* shader);
b32 free_shader(Shader* shader);

b32 set_uniform_m4(const char* name, Matrix4 m);
b32 set_uniform_texture(const char* name, Texture2d t);

void set_shader(Shader* s);
Shader* get_bound_shader(void);

enum Framebuffer_Flags {
    FF_Diffuse     = (1 << 0),
    FF_Position    = (1 << 1),
    FF_Normal      = (1 << 2),
    FF_Depth       = (1 << 3),

    FF_GBuffer     = (FF_Diffuse | FF_Position | FF_Normal | FF_Depth),
};

enum Framebuffer_Colors_Index {
    FCI_Diffuse = 0,
    FCI_Normal,
    FCI_Position,
    FCI_Count,
};

typedef struct Framebuffer {
    GLuint handle;
    Texture2d color[FCI_Count];
    Texture2d depth;
    int width, height;
    int flags;
} Framebuffer;

b32 init_framebuffer(int width, int height, int flags, Framebuffer* framebuffer);
b32 free_framebuffer(Framebuffer* framebuffer);
b32 resize_framebuffer(Framebuffer* framebuffer, int width, int height);

void begin_framebuffer(Framebuffer framebuffer);
void end_framebuffer(void);
void clear_framebuffer(Vector3 color);

typedef struct OpenGL_Context {
    GLint maj_version, min_version;

    Shader* bound_shader;

    b32 is_initialized;
} OpenGL_Context;

#endif /* OPENGL_H */