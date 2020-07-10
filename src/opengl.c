#include "opengl.h"
#include "debug.h"

#if PLATFORM_WINDOWS
#include "opengl_win32.c"
#else
#error OpenGL platform implementation missing for this OS
#endif

#define DEFINE_GL_FUNCTIONS(type, func) type func = 0;
GL_BINDINGS(DEFINE_GL_FUNCTIONS)
#undef  DEFINE_GL_FUNCTIONS

OpenGL_Context* g_gl_context = 0;

b32 upload_texture2d(Texture2d* t) {
    GLint format = 0;
    switch (t->depth) {
    case 1:
        format = GL_RED;
        break;
    case 3:
        format = GL_RGB;
        break;
    case 4:
        format = GL_RGBA;
        break;
    default:
        invalid_code_path;
        break;
    }

    if (t->id == 0) {
        glGenTextures(1, &t->id);
        glBindTexture(GL_TEXTURE_2D, t->id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexImage2D(
            GL_TEXTURE_2D, 
            0, 
            t->depth == 4 ? GL_SRGB_ALPHA : format, 
            t->width, 
            t->height, 
            0, 
            format, 
            GL_UNSIGNED_BYTE, 
            t->pixels
        );
    } else {
        glBindTexture(GL_TEXTURE_2D, t->id);

        glTexSubImage2D(
            GL_TEXTURE_2D, 
            0, 0, 0, 
            t->width, 
            t->height, 
            format, 
            GL_UNSIGNED_BYTE, 
            t->pixels
        );
    }

    return true;
}

b32 init_shader(Shader* shader) {
    const GLuint program_id = glCreateProgram();

    const GLuint vert_id = glCreateShader(GL_VERTEX_SHADER);
    const GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);

    const GLchar* shader_header = "#version 330 core\n#extension GL_ARB_seperate_shader_objects: enable\n";

    assert(shader->source.data && shader->source.len);

    const GLchar* vert_shader[] = { shader_header, "#define VERTEX 1\n", (const GLchar*)shader->source.data };
    const GLchar* frag_shader[] = { shader_header, "#define FRAGMENT 1\n", (const GLchar*)shader->source.data };

    glShaderSource(vert_id, 3, vert_shader, 0);
    glShaderSource(frag_id, 3, frag_shader, 0);

    glCompileShader(vert_id);
    glCompileShader(frag_id);

    glAttachShader(program_id, vert_id);
    glAttachShader(program_id, frag_id);

    glLinkProgram(program_id);
    glValidateProgram(program_id);

    GLint is_linked = 0;
    glGetProgramiv(program_id, GL_LINK_STATUS, &is_linked);
    if (!is_linked) {
        GLsizei ignored;
        char vert_errors[4096];
        char frag_errors[4096];
        char program_errors[4096];

        glGetShaderInfoLog(vert_id, sizeof(vert_errors), &ignored, vert_errors);
        glGetShaderInfoLog(frag_id, sizeof(frag_errors), &ignored, frag_errors);
        glGetProgramInfoLog(program_id, sizeof(program_errors), &ignored, program_errors);

        o_log_error("[OpenGL] Shader compile failed");
        if (vert_errors[0] != 0)
        {
            o_log_error(vert_errors);
        }
        if (frag_errors[0] != 0)
        {
            o_log_error(frag_errors);
        }
        if (program_errors[0] != 0)
        {
            o_log_error(program_errors);
        }
        return false;
    }

    glDeleteShader(vert_id);
    glDeleteShader(frag_id);

    glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &shader->uniform_count);
    for (GLint i = 0; i < shader->uniform_count; ++i) {
        GLsizei length;
        GLint size;
        GLenum type;
        GLchar name[SHADER_UNFORM_NAME_CAP];
        glGetActiveUniform(program_id, (GLuint)i, SHADER_UNFORM_NAME_CAP, &length, &size, &type, name);
        Shader_Uniform uniform;
        mem_copy(uniform.name, name, length);
        uniform.name[length] = 0;
        uniform.name_len = (u32)length;
        uniform.type = type;
        uniform.location = glGetUniformLocation(program_id, name);

        shader->uniforms[i] = uniform;
    }

    shader->id = program_id;
    return true;
}

b32 free_shader(Shader* shader) {
    return false; // @TODO
}

static const char* get_shader_var_type_string(GLenum type) {
    switch (type) {
        case GL_FLOAT:      return "f32";
        case GL_FLOAT_VEC2: return "Vector2";
        case GL_FLOAT_VEC3: return "Vector3";
        case GL_FLOAT_VEC4: return "Vector4";
        case GL_FLOAT_MAT4: return "Matrix4";
        case GL_SAMPLER_2D: return "Texture2D";
        default:            invalid_code_path;
    }

    return "";
}

static
Shader_Uniform* find_uniform(const char* name, GLenum type) {
    Shader* const s = get_bound_shader();
    if (!s) {
        o_log_error("[OpenGL] Tried to set uniform but no shader was bound\n");
        return false;
    }

    for (int i = 0; i < s->uniform_count; ++i) {
        Shader_Uniform* const it = &s->uniforms[i];

        if (str_cmp(name, it->name) != 0) continue;

        if (it->type != type) {
            o_log_error("[OpenGL] Tried to set uniform with wrong type. %s has the type of %s\n", name, get_shader_var_type_string(GL_FLOAT_MAT4));
            return 0;
        }

        return it;
    }

    o_log_error("[OpenGL] Failed to find uniform %s\n", name);
    return 0;
}

b32 set_uniform_m4(const char* name, Matrix4 m) {
    Shader_Uniform* const var = find_uniform(name, GL_FLOAT_MAT4);
    if (!var) return false; 

    glUniformMatrix4fv(var->location, 1, GL_FALSE, m.e);
    return true;
}

b32 set_uniform_texture(const char* name, Texture2d t) {
    Shader_Uniform* const var = find_uniform(name, GL_SAMPLER_2D);
    if (!var) return false; 

    glActiveTexture(GL_TEXTURE0 + var->location);
    glBindTexture(GL_TEXTURE_2D, t.id);

    glUniform1i(var->location, var->location);
    return true;
}

void set_shader(Shader* s) {
    if (!s) {
        glUseProgram(0);
        g_gl_context->bound_shader = 0;
    } else {
        glUseProgram(s->id);
        g_gl_context->bound_shader = s;
    }
}

Shader* get_bound_shader(void) {
    return g_gl_context->bound_shader;
}

b32 init_framebuffer(int width, int height, int flags, Framebuffer* framebuffer) {
    GLuint handle;
    glGenFramebuffers(1, &handle);
    glBindFramebuffer(GL_FRAMEBUFFER, handle);

    Framebuffer result = { 
        .handle = handle,
        .width  = width,
        .height = height,
        .flags  = flags,
    };

    if ((flags & FF_Diffuse) != 0) {
        GLuint diffuse_texture;
        glGenTextures(1, &diffuse_texture);
        glBindTexture(GL_TEXTURE_2D, diffuse_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + FCI_Diffuse, GL_TEXTURE_2D, diffuse_texture, 0);
        result.color[FCI_Diffuse] = (Texture2d) {  
            .width = width, 
            .height = height, 
            .depth = 4, 
            .id = diffuse_texture,
        };
    }

    if ((flags & FF_Normal) != 0) {
        GLuint normal_texture;
        glGenTextures(1, &normal_texture);
        glBindTexture(GL_TEXTURE_2D, normal_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + FCI_Normal, GL_TEXTURE_2D, normal_texture, 0);
        result.color[FCI_Normal] = (Texture2d) {  
            .width  = width, 
            .height = height, 
            .depth  = 4, 
            .id     = normal_texture,
        };
    }

    if ((flags & FF_Position) != 0) {
        GLuint position_texture;
        glGenTextures(1, &position_texture);
        glBindTexture(GL_TEXTURE_2D, position_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + FCI_Position, GL_TEXTURE_2D, position_texture, 0);
        result.color[FCI_Position] = (Texture2d) {  
            .width  = width, 
            .height = height, 
            .depth  = 4, 
            .id     = position_texture,
        };
    }

    if ((flags & FF_Depth) != 0) {
        GLuint depth_texture;
        glGenTextures(1, &depth_texture);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
        result.depth = (Texture2d) {  
            .width  = width, 
            .height = height, 
            .depth  = 1, 
            .id     = depth_texture,
        };
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // @TODO(colby): do cleanup
        return false;
    }

    *framebuffer = result;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

b32 free_framebuffer(Framebuffer* framebuffer) {
    return false; // @TODO
}

b32 resize_framebuffer(Framebuffer* framebuffer, int width, int height) {
    return false;
}

void begin_framebuffer(Framebuffer framebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.handle);

    GLuint attachments[FCI_Count] = { 0 };
    int num_attachments = 0;
    
    const int flags = framebuffer.flags;
    if ((flags & FF_Diffuse) != 0)  attachments[num_attachments++] = GL_COLOR_ATTACHMENT0 + FCI_Diffuse;
    if ((flags & FF_Normal) != 0)   attachments[num_attachments++] = GL_COLOR_ATTACHMENT0 + FCI_Normal;
    if ((flags & FF_Position) != 0) attachments[num_attachments++] = GL_COLOR_ATTACHMENT0 + FCI_Position;

    glDrawBuffers(num_attachments, attachments);
    glViewport(0, 0, framebuffer.width, framebuffer.height);
}

void end_framebuffer(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void clear_framebuffer(Vector3 color) {
    glClearColor(color.r, color.g, color.b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}