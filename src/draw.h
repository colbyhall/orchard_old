#ifndef DRAW_H
#define DRAW_H

#include "opengl.h"
#include "math.h"

typedef struct Immediate_Vertex {
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
    Vector4 color;
} Immediate_Vertex;

// Must be multiple of 3
#define MAX_IMM_VERTS (1024 * 3)

typedef struct Immediate_Renderer {
    GLuint vao, vbo;
    Immediate_Vertex vertices[MAX_IMM_VERTS];
    int vertex_count;

    Matrix4 projection;
    Matrix4 view;

    b32 is_initialized;
} Immediate_Renderer;

extern Shader* solid_shape_shader;

void init_imm_renderer(Allocator allocator);

void imm_refresh_transform(void);
void imm_render_right_handed(Rect viewport);
void imm_render_ortho(Vector3 pos, f32 aspect_ratio, f32 ortho_size);

void imm_begin(void);
void imm_flush(void);
void imm_vertex(Vector3 position, Vector3 normal, Vector2 uv, Vector4 color);

void imm_textured_rect(Rect rect, f32 z, Vector2 uv0, Vector2 uv1, Vector4 color);
void imm_rect(Rect rect, f32 z, Vector4 color);

void imm_textured_border_rect(Rect rect, f32 z, f32 thickness, Vector2 uv0, Vector2 uv1, Vector4 color);
void imm_border_rect(Rect rect, f32 z, f32 thickness, Vector4 color);

void imm_textured_line(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector2 uv0, Vector2 uv1, Vector4 color);
void imm_line(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector4 color);
void imm_arrow(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector4 color);

void begin_clip_rect(Rect rect);
void end_clip_rect(void);

#endif /* DRAW_H */