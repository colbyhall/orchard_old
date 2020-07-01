#include "draw.h"

#define FAR_CLIP_PLANE 1000.f
#define NEAR_CLIP_PLANE 0.1
static Immediate_Renderer* g_imm_renderer = 0;
Shader* solid_shape_shader = 0;

const char* solid_shape_shader_source = 
"#ifdef VERTEX\n\
layout(location = 0) in vec3 position;\n\
layout(location = 1) in vec2 normal;\n\
layout(location = 2) in vec2 uv;\n\
layout(location = 3) in vec4 color;\n\
uniform mat4 projection;\n\
uniform mat4 view;\n\
out vec4 out_color;\n\
out vec2 out_uv;\n\
void main() {\n\
    gl_Position =  projection * view * vec4(position, 1.0);\n\
    out_color = color;\n\
    out_uv = uv;\n\
}\n\
#endif\n\
#ifdef FRAGMENT\n\
out vec4 frag_color;\n\
in vec4 out_color;\n\
in vec2 out_uv;\n\
\n\
uniform sampler2D tex;\n\
\n\
void main() {\n\
    if (out_uv.x < 0.0) {\n\
        frag_color = out_color;\n\
    } else {\n\
        frag_color = texture(tex, out_uv);\n\
    }\n\
}\n\
#endif\n";

void init_imm_renderer(Allocator allocator) {
    g_imm_renderer = mem_alloc_struct(allocator, Immediate_Renderer);

    if (g_imm_renderer->is_initialized) return;

    glGenVertexArrays(1, &g_imm_renderer->vao);
    glBindVertexArray(g_imm_renderer->vao);

    glGenBuffers(1, &g_imm_renderer->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_imm_renderer->vbo);

    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glClearDepth(1.f);
    
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    g_imm_renderer->is_initialized = true;

    solid_shape_shader = mem_alloc_struct(allocator, Shader);
    solid_shape_shader->source = (GLchar*)solid_shape_shader_source;
    solid_shape_shader->source_len = (int)str_len(solid_shape_shader_source);
    if (!init_shader(solid_shape_shader)) assert(false);

    set_shader(solid_shape_shader);
}

void imm_refresh_transform(void) {
    set_uniform_m4("view",         g_imm_renderer->view);
    set_uniform_m4("projection",   g_imm_renderer->projection);
}

void imm_render_right_handed(Rect viewport) {
    const Vector2 draw_size = rect_size(viewport);
    const f32 aspect_ratio = draw_size.width / draw_size.height;
    const f32 ortho_size = draw_size.height / 2.f;

    g_imm_renderer->projection  = m4_ortho(ortho_size, aspect_ratio, FAR_CLIP_PLANE, NEAR_CLIP_PLANE);
    g_imm_renderer->view        = m4_translate(v3(-draw_size.width / 2.f, -ortho_size, 0.f));

    imm_refresh_transform();
}

void imm_render_ortho(Vector3 pos, f32 aspect_ratio, f32 ortho_size) {
    g_imm_renderer->projection = m4_ortho(ortho_size, aspect_ratio, FAR_CLIP_PLANE, NEAR_CLIP_PLANE);
    g_imm_renderer->view = m4_translate(v3_negate(pos));

    imm_refresh_transform();
}

void imm_begin(void) {
    g_imm_renderer->vertex_count = 0;
}

void imm_flush(void) {
    Shader* const bound_shader = get_bound_shader();
    if (!bound_shader) {
        printf("Tried to flush imm_renderer when no shader was bound");
        return;
    }

    glBindVertexArray(g_imm_renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_imm_renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_imm_renderer->vertices[0]) * g_imm_renderer->vertex_count, g_imm_renderer->vertices, GL_STREAM_DRAW);

    const GLuint position_loc = 0;
    glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), 0);
    glEnableVertexAttribArray(position_loc);
    
    const GLuint normal_loc = 1;
    glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void*)sizeof(Vector3));
    glEnableVertexAttribArray(normal_loc);

    const GLuint uv_loc = 2;
    glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void*)(sizeof(Vector3) + sizeof(Vector3)));
    glEnableVertexAttribArray(uv_loc);

    const GLuint color_loc = 3;
    glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void*)(sizeof(Vector3) + sizeof(Vector3) + sizeof(Vector2)));
    glEnableVertexAttribArray(color_loc);
    
    glDrawArrays(GL_TRIANGLES, 0, g_imm_renderer->vertex_count);

    glDisableVertexAttribArray(position_loc);
    glDisableVertexAttribArray(normal_loc);
    glDisableVertexAttribArray(uv_loc);
    glDisableVertexAttribArray(color_loc);

    glBindVertexArray(0);
}

void imm_vertex(Vector3 position, Vector3 normal, Vector2 uv, Vector4 color) {
    if (g_imm_renderer->vertex_count >= MAX_IMM_VERTS - 1) {
        imm_flush();
        imm_begin();
    }

    Immediate_Vertex* const this_vertex = g_imm_renderer->vertices + g_imm_renderer->vertex_count++;
    *this_vertex = (Immediate_Vertex) { position, normal, uv, color };
}

void imm_textured_rect(Rect rect, f32 z, Vector2 uv0, Vector2 uv1, Vector4 color) {
    const Vector3 top_left_pos      = v3(rect.min.x, rect.max.y, z);
    const Vector3 top_right_pos     = v3xy(rect.max, z);
    const Vector3 bottom_left_pos   = v3xy(rect.min, z);
    const Vector3 bottom_right_pos  = v3(rect.max.x, rect.min.y, z);

    const Vector2 top_left_uv       = v2(uv0.x, uv1.y);
    const Vector2 top_right_uv      = v2(uv1.x, uv1.y);
    const Vector2 bottom_left_uv    = v2(uv0.x, uv0.y);
    const Vector2 bottom_right_uv   = v2(uv1.x, uv0.y);

    const Vector3 normal = v3z(); // @Incomplete

    imm_vertex(top_left_pos, normal, top_left_uv, color);
    imm_vertex(bottom_left_pos, normal, bottom_left_uv, color);
    imm_vertex(top_right_pos, normal, top_right_uv, color);

    imm_vertex(bottom_left_pos, normal, bottom_left_uv, color);
    imm_vertex(bottom_right_pos, normal, bottom_right_uv, color);
    imm_vertex(top_right_pos, normal, top_right_uv, color);
}

void imm_rect(Rect rect, f32 z, Vector4 color) {
    imm_textured_rect(rect, z, v2s(-1.f), v2s(-1.f), color);
}

void imm_textured_border_rect(Rect rect, f32 z, f32 thickness, Vector2 uv0, Vector2 uv1, Vector4 color) {
    const Vector2 size = rect_size(rect);

    // Left quad
    {
        const Rect    left      = (Rect) { rect.min, v2(rect.min.x + thickness, rect.max.y) };
        const Vector2 left_uv1  = v2(uv0.x + thickness / size.width * (uv1.x - uv0.x), uv1.y);
        imm_textured_rect(left, z, uv0, left_uv1, color);
    }

    // Right quad
    {
        const Rect right        = (Rect) { v2(rect.max.x - thickness, rect.min.y), rect.max };
        const Vector2 right_uv0 = v2(uv1.x - thickness / size.width * (uv1.x - uv0.x), uv0.y);
        const Vector2 right_uv1 = v2(uv1.x - thickness / size.width * (uv1.x - uv0.x), uv1.y);
        imm_textured_rect(right, z, right_uv0, right_uv1, color);
    }

    // Top quad
    {
        const Rect top          = (Rect) { v2(rect.min.x, rect.max.y - thickness), rect.max };
        const Vector2 top_uv0   = v2(uv0.x, uv1.y - thickness / size.height * (uv1.y - uv0.y));
        imm_textured_rect(top, z, top_uv0, uv1, color);
    }

    // Bottom quad
    {
        const Rect bot          = (Rect) { rect.min, v2(rect.max.x, rect.min.y + thickness) };
        const Vector2 bot_uv1   = v2(uv1.x, uv0.y + thickness / size.width * (uv1.y - uv0.y));
        imm_textured_rect(bot, z, uv0, bot_uv1, color);
    }
}

void imm_border_rect(Rect rect, f32 z, f32 thickness, Vector4 color) {
    imm_textured_border_rect(rect, z, thickness, v2s(-1.f), v2s(-1.f), color);
}

void imm_textured_line(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector2 uv0, Vector2 uv1, Vector4 color) {
    const f32 height    = thickness / 2.f;
    const Vector2 dir   = v2_norm(v2_sub(a2, a1));
    const Vector2 perp  = v2_mul(v2_perp(dir), v2s(height));

    const Vector3 top_left      = v3xy(v2_sub(a2, perp), z);
    const Vector3 bottom_left   = v3xy(v2_sub(a1, perp), z);
    const Vector3 top_right     = v3xy(v2_add(a2, perp), z);
    const Vector3 bottom_right  = v3xy(v2_add(a1, perp), z);

    const Vector2 top_left_uv       = v2(uv0.x, uv1.y);
    const Vector2 top_right_uv      = v2(uv1.x, uv1.y);
    const Vector2 bottom_left_uv    = v2(uv0.x, uv0.y);
    const Vector2 bottom_right_uv   = v2(uv1.x, uv0.y);
    
    const Vector3 normal = v3s(0.f); // @Incomplete

    imm_vertex(top_left, normal, top_left_uv, color);
    imm_vertex(bottom_left, normal, bottom_left_uv, color);
    imm_vertex(top_right, normal, top_right_uv, color);

    imm_vertex(bottom_left, normal, bottom_left_uv, color);
    imm_vertex(bottom_right, normal, bottom_right_uv, color);
    imm_vertex(top_right, normal, top_right_uv, color);
}

void imm_line(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector4 color) {
    imm_textured_line(a1, a2, z, thickness, v2s(-1.f), v2s(-1.f), color);
}

void imm_arrow(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector4 color) {
    const f32 height = thickness * 2.f;
    const Vector2 dir = v2_norm(v2_sub(a1, a2));
    const Vector2 perp = v2_mul(v2_perp(dir), v2s(height));

    const Vector2 down = v2_mul(dir, v2s(height));
    const Vector2 l2 = v2_add(v2_add(a2, v2(-perp.x, -perp.y)), down);
    const Vector2 r2 = v2_add(v2_add(a2, perp), down);

    imm_line(a2, l2, z, thickness, color);
    imm_line(a2, r2, z, thickness, color);
    imm_line(a1, a2, z, thickness, color);
}