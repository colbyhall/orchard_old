#ifndef DRAW_H
#define DRAW_H

#include "math.h"
#include <stb/stb_truetype.h>

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
    int num_glyphs;

    Texture2d atlas;
    stbtt_fontinfo* info; // @HACK(colby): This really sucks
} Font;

#define FONT_CAP 32
typedef struct Font_Collection {
    stbtt_fontinfo info;

    Font fonts[FONT_CAP];
    int num_fonts;

    int* codepoint_indices;
    int codepoint_count;

    f32 atlas_area; // Used for determining oversample amount;
    Allocator asset_memory;
} Font_Collection;

b32 init_font_collection(u8* data, int len, Allocator asset_memory, Font_Collection* collection);
Font* font_at_size(Font_Collection* collection, int size);
Font_Glyph* glyph_from_rune(Font* f, Rune r);

extern Font_Collection* g_font_collection; // @Temp
extern Shader* g_font_shader;

void init_draw(Allocator allocator);

void begin_draw(void);
void end_draw(void);

void imm_refresh_transform(void);
void imm_render_right_handed(Rect viewport);
void imm_render_ortho(Vector3 pos, f32 aspect_ratio, f32 ortho_size);
void imm_render_from(Vector3 pos); // This is the one that uses the g_back_buffer

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

void imm_glyph(Font_Glyph* g, Font* font, Vector2 xy, f32 z, Vector4 color);
Font_Glyph* imm_rune(Rune r, Font* font, Vector2 xy, f32 z, Vector4 color);
void imm_string(String str, Font* font, f32 max_width, Vector2 xy, f32 z, Vector4 color);

void begin_clip_rect(Rect rect);
void end_clip_rect(void);

#endif /* DRAW_H */