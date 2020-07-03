#include "opengl.h"

#if PLATFORM_WINDOWS
#include "opengl_win32.c"
#else
#error OpenGL platform implementation missing for this OS
#endif

#define DEFINE_GL_FUNCTIONS(type, func) type func = 0;
GL_BINDINGS(DEFINE_GL_FUNCTIONS)
#undef  DEFINE_GL_FUNCTIONS

OpenGL_Context* g_gl_context = 0;

b32 init_shader(Shader* shader) {
    const GLuint program_id = glCreateProgram();

    const GLuint vert_id = glCreateShader(GL_VERTEX_SHADER);
    const GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);

    const GLchar* shader_header = "#version 330 core\n#extension GL_ARB_seperate_shader_objects: enable\n";

    assert(shader->source);

    const GLchar* vert_shader[] = { shader_header, "#define VERTEX 1\n", (const GLchar*)shader->source };
    const GLchar* frag_shader[] = { shader_header, "#define FRAGMENT 1\n", (const GLchar*)shader->source };

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

        if (vert_errors[0] != 0)
        {
            printf(vert_errors);
        }
        if (frag_errors[0] != 0)
        {
            printf(frag_errors);
        }
        if (program_errors[0] != 0)
        {
            printf(program_errors);
        }
        return false;
    }

    glDeleteShader(vert_id);
    glDeleteShader(frag_id);

    glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &shader->num_uniforms);
    for (GLint i = 0; i < shader->num_uniforms; ++i) {
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
        printf("[OpenGL] Tried to set uniform but no shader was bound\n");
        return false;
    }

    for (int i = 0; i < s->num_uniforms; ++i) {
        Shader_Uniform* const it = &s->uniforms[i];

        if (strcmp(name, it->name) != 0) continue; // @CRT

        if (it->type != type) {
            printf("[OpenGL] Tried to set uniform with wrong type. %s has the type of %s\n", name, get_shader_var_type_string(GL_FLOAT_MAT4));
            return 0;
        }

        return it;
    }

    printf("[OpenGL] Failed to find uniform %s\n", name);
    return 0;
}

b32 set_uniform_m4(const char* name, Matrix4 m) {
    Shader_Uniform* const var = find_uniform(name, GL_FLOAT_MAT4);
    if (!var) return false; 

    glUniformMatrix4fv(var->location, 1, GL_FALSE, m.e);
    return true;
}

b32 set_uniform_texture(const char* name, Texture2d* t) {
    Shader_Uniform* const var = find_uniform(name, GL_SAMPLER_2D);
    if (!var) return false; 

    glActiveTexture(GL_TEXTURE0 + var->location);
    glBindTexture(GL_TEXTURE_2D, t->id);

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