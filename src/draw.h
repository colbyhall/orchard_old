#ifndef DRAW_H
#define DRAW_H

#include "math.h"
#include "opengl.h"
#include <stb/stb_truetype.h>

typedef struct Mesh_Vertex {
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
} Mesh_Vertex;

void set_mesh_vertex_format(void);

typedef struct Mesh {
    Mesh_Vertex* vertices;
    int vertex_count;

    u32* indices;
    int index_count;

    GLuint vao, vbo, vio;
} Mesh;

b32 upload_mesh(Mesh* m);
void draw_mesh(Mesh* m, Vector3 position, Quaternion rotation, Vector3 scale);

typedef struct Font_Glyph {
    f32 width, height;
    f32 bearing_x, bearing_y;
    f32 advance;

    Vector2 uv0, uv1;
} Font_Glyph;

typedef struct Font {
    int size;
    f32 ascent, descent, line_gap;

    Font_Glyph* glyphs;
    int glyph_count;

    Texture2d atlas;
    stbtt_fontinfo* info; // @HACK(colby): This really sucks
} Font;

#define FONT_CAP 32
typedef struct Font_Collection {
    stbtt_fontinfo info;

    Font fonts[FONT_CAP];
    int font_count;

    int* codepoint_indices;
    int codepoint_count;

    Allocator asset_memory;
} Font_Collection;

b32 init_font_collection(u8* data, int len, Allocator asset_memory, Font_Collection* collection);
Font* font_at_size(Font_Collection* collection, int size);
Font_Glyph* glyph_from_rune(Font* f, Rune r);

void init_draw(Platform* platform);
void resize_draw(int new_width, int new_height);

void refresh_shader_transform(void);
void draw_right_handed(Rect viewport);
void draw_from(Vector2 pos, f32 ortho_size); // Used for drawing our 2d scene using the back buffer for ortho size

void imm_begin(void);
void imm_flush(void);
void imm_vertex(Vector3 position, Vector3 normal, Vector2 uv, Vector4 color);
void set_imm_vertex_format(void);

void imm_textured_rect(Rect rect, f32 z, Vector2 uv0, Vector2 uv1, Vector4 color);
inline void imm_rect(Rect rect, f32 z, Vector4 color) { imm_textured_rect(rect, z, v2s(-1.f), v2s(-1.f), color); }

void imm_textured_border_rect(Rect rect, f32 z, f32 thickness, Vector2 uv0, Vector2 uv1, Vector4 color);
inline void imm_border_rect(Rect rect, f32 z, f32 thickness, Vector4 color) { imm_textured_border_rect(rect, z, thickness, v2s(-1.f), v2s(-1.f), color); }

void imm_textured_line(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector2 uv0, Vector2 uv1, Vector4 color);
inline void imm_line(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector4 color) { imm_textured_line(a1, a2, z, thickness, v2s(-1.f), v2s(-1.f), color); }
void imm_arrow(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector4 color);

void imm_glyph(Font_Glyph* g, Font* font, Vector2 xy, f32 z, Vector4 color);
Font_Glyph* imm_rune(Rune r, Font* font, Vector2 xy, f32 z, Vector4 color);
void imm_string(String str, Font* font, f32 max_width, Vector2 xy, f32 z, Vector4 color);

void imm_textured_plane(Vector3 pos, Quaternion rot, Rect rect, Vector2 uv0, Vector2 uv1, Vector4 color);
inline void imm_plane(Vector3 pos, Quaternion rot, Rect rect, Vector4 color) { imm_textured_plane(pos, rot, rect, v2s(-1.f), v2s(-1.f), color); }

void begin_clip_rect(Rect rect);
void end_clip_rect(void);

#endif /* DRAW_H */