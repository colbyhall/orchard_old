#ifndef DRAW_H
#define DRAW_H

#include "math.h"

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

void begin_clip_rect(Rect rect);
void end_clip_rect(void);

#endif /* DRAW_H */