#include "draw.h"
#include "opengl.h"
#include "debug.h"
#include "asset.h"

#define STBRP_STATIC
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>

#define STB_IMAGE_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

typedef struct Draw_State {
    Framebuffer back_buffer;
    Framebuffer hdr_buffer;
    Framebuffer g_buffer;

    Matrix4 projection_matrix;
    Matrix4 view_matrix;
    Matrix4 model_matrix;

    int num_draw_calls;

    b32 is_initialized;
} Draw_State;

static Draw_State* draw_state = 0;

b32 upload_mesh(Mesh* m) {
    glGenVertexArrays(1, &m->vao);
    glBindVertexArray(m->vao);

    glGenBuffers(2, &m->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Mesh_Vertex) * m->vertex_count, m->vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->vio);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->index_count * sizeof(u32), m->indices, GL_STATIC_DRAW);

    return true;
}

void draw_mesh(Mesh* m, Vector3 position, Quaternion rotation, Vector3 scale) {
    draw_state->model_matrix = m4_mul(m4_mul(m4_translate(position), m4_rotate(rotation)), m4_scale(scale));
    refresh_shader_transform();

    set_uniform_v4("color", v4s(1.f));

    glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->vio);

    set_mesh_vertex_format();

    glDrawElements(GL_TRIANGLES, m->index_count, GL_UNSIGNED_INT, 0);
}

void set_mesh_vertex_format(void) {
    GLuint position_loc = 0;
    glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, sizeof(Mesh_Vertex), 0);
    glEnableVertexAttribArray(position_loc);
    
    GLuint normal_loc = 1;
    glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, sizeof(Mesh_Vertex), (void*)sizeof(Vector3));
    glEnableVertexAttribArray(normal_loc);

    GLuint uv_loc = 2;
    glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, sizeof(Mesh_Vertex), (void*)(sizeof(Vector3) + sizeof(Vector3)));
    glEnableVertexAttribArray(uv_loc);
}

b32 init_font_collection(u8* data, int len, Allocator asset_memory, Font_Collection* collection) {
    *collection = (Font_Collection) { 0 };
    if (!stbtt_InitFont(&collection->info, data, stbtt_GetFontOffsetForIndex(data, 0))) {
        return false;
    }

    collection->codepoint_count = collection->info.numGlyphs;
    collection->codepoint_indices = mem_alloc_array(asset_memory, int, collection->codepoint_count);
    
    int glyphs_found = 0;
    // Ask STBTT for the glyph indices.
    // @TODO(colby): linearly search the codepoint space because STBTT doesn't expose CP->glyph idx;
    //               later we will parse the ttf file in a similar way to STBTT.
    //               Linear search is exactly 17 times slower than parsing for 65536 glyphs.
    for (int codepoint = 0; codepoint < 0x110000; ++codepoint) {
        int idx = stbtt_FindGlyphIndex(&collection->info, codepoint);
        if (idx <= 0) continue;
        glyphs_found += 1;
        collection->codepoint_indices[idx] = codepoint;
    }

    collection->asset_memory = asset_memory;

    return true;
}

#define FONT_ATLAS_SIZE 4096
Font* font_at_size(Font_Collection* collection, int size) {
    size = CLAMP(size, 2, 512);

    for (int i = 0; i < collection->font_count; ++i) {
        Font* f = &collection->fonts[i];
        if (f->size == size) return f;
    }

    assert(collection->font_count + 1 < FONT_CAP);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&collection->info, &ascent, &descent, &line_gap);

    Font* f = &collection->fonts[collection->font_count++];

    f->size = size;
    f->info = &collection->info;
    f->glyph_count = collection->codepoint_count;

    f32 font_scale = stbtt_ScaleForPixelHeight(&collection->info, (f32)size);
    f->ascent   = (f32)ascent * font_scale;
    f->descent  = (f32)descent * font_scale;
    f->line_gap = (f32)line_gap * font_scale;

    int h_oversample = 1;
    int v_oversample = 1;

    if (size <= 36) {
        h_oversample = 2;
        v_oversample = 2;
    }
    if (size <= 12) {
        h_oversample = 4;
        v_oversample = 4;
    }
    if (size <= 8) {
        h_oversample = 8;
        v_oversample = 8;
    }

    Texture2d* atlas = &f->atlas;
    atlas->depth  = 1;
    atlas->width  = FONT_ATLAS_SIZE;
    atlas->height = FONT_ATLAS_SIZE;

    atlas->pixels = mem_alloc_array(collection->asset_memory, u8, atlas->width * atlas->height); // @Leak

    stbtt_pack_context pc;
    stbtt_packedchar* pdata = mem_alloc_array(collection->asset_memory, stbtt_packedchar, collection->codepoint_count); // @Leak

    stbtt_pack_range pr;
    stbtt_PackBegin(&pc, atlas->pixels, atlas->width, atlas->height, 0, 1, 0);
    pr.chardata_for_range               = pdata;
    pr.array_of_unicode_codepoints      = collection->codepoint_indices;
    pr.first_unicode_codepoint_in_range = 0;
    pr.num_chars                        = collection->codepoint_count;
    pr.font_size                        = (f32)size;

    stbtt_PackSetSkipMissingCodepoints(&pc, 1);
    stbtt_PackSetOversampling(&pc, h_oversample, v_oversample);
    stbtt_PackFontRanges(&pc, collection->info.data, 0, &pr, 1);
    stbtt_PackEnd(&pc);

    b32 ok = upload_texture2d(atlas);
    if (!ok) assert(false);

    f->glyphs = mem_alloc_array(collection->asset_memory, Font_Glyph, collection->codepoint_count);
    for (int i = 0; i < collection->codepoint_count; ++i) {
        Font_Glyph* g = f->glyphs + i;

        g->uv0 = v2((f32)pdata[i].x0 / (f32)atlas->width, (f32)pdata[i].y1 / (f32)atlas->width);
        g->uv1 = v2((f32)pdata[i].x1 / (f32)atlas->width, (f32)pdata[i].y0 / (f32)atlas->width);

        g->width     = ((f32)pdata[i].x1 - pdata[i].x0) / (f32)h_oversample;
        g->height    = ((f32)pdata[i].y1 - pdata[i].y0) / (f32)v_oversample;
        g->bearing_x = pdata[i].xoff;
        g->bearing_y = pdata[i].yoff;
        g->advance   = pdata[i].xadvance;
    }

    return f;
}

Font_Glyph* glyph_from_rune(Font* f, Rune r) {
    int index = stbtt_FindGlyphIndex(f->info, r);
    if (index > 0) {
        assert(index < f->glyph_count);
        return &f->glyphs[index];
    }

    return 0;
}

Font_Collection* g_font_collection = 0;

typedef struct Immediate_Vertex {
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
    Vector4 color;
} Immediate_Vertex;

// Must be multiple of 3
#define MAX_IMM_VERTS (4096 * 3)

typedef struct Immediate_Renderer {
    GLuint vao, vbo;
    Immediate_Vertex vertices[MAX_IMM_VERTS];
    int vertex_count;
} Immediate_Renderer;
static Immediate_Renderer* imm_renderer = 0;

#define FAR_CLIP_PLANE 1000.f
#define NEAR_CLIP_PLANE 0.001f

void init_draw(Platform* platform) {
    imm_renderer      = mem_alloc_struct(platform->permanent_arena, Immediate_Renderer);
    draw_state        = mem_alloc_struct(platform->permanent_arena, Draw_State);

    if (draw_state->is_initialized) return;
    draw_state->is_initialized = true;

    glGenVertexArrays(1, &imm_renderer->vao);
    glBindVertexArray(imm_renderer->vao);

    glGenBuffers(1, &imm_renderer->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, imm_renderer->vbo);

    glEnable(GL_FRAMEBUFFER_SRGB); 
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

    b32 ok = init_framebuffer(
        g_platform->window_width, 
        g_platform->window_height, 
        FF_Albedo, 
        &draw_state->back_buffer
    );
    assert(ok);
}

void resize_draw(int new_width, int new_height) {
    // resize_framebuffer(&draw_state->g_buffer, new_width, new_height);
    // resize_framebuffer(&draw_state->hdr_buffer, new_width, new_height);
    resize_framebuffer(&draw_state->back_buffer, new_width, new_height);
}

void refresh_shader_transform(void) {
    set_uniform_m4("view",         draw_state->view_matrix);
    set_uniform_m4("projection",   draw_state->projection_matrix);
}

void draw_right_handed(Rect viewport) {
    Vector2 draw_size = rect_size(viewport);
    f32 aspect_ratio  = draw_size.width / draw_size.height;
    f32 ortho_size    = draw_size.height / 2.f;

    draw_state->projection_matrix = m4_ortho(ortho_size, aspect_ratio, FAR_CLIP_PLANE, NEAR_CLIP_PLANE);
    draw_state->view_matrix       = m4_translate(v3(-draw_size.width / 2.f, -ortho_size, 0.f));
    draw_state->model_matrix      = m4_identity();

    refresh_shader_transform();
}

static void draw_ortho(Vector3 pos, Quaternion rot, f32 aspect_ratio, f32 ortho_size) {
    draw_state->projection_matrix = m4_ortho(ortho_size, aspect_ratio, FAR_CLIP_PLANE, NEAR_CLIP_PLANE);
    draw_state->view_matrix       = m4_mul(m4_rotate(quat_inverse(rot)), m4_translate(v3_inverse(pos)));
    draw_state->model_matrix      = m4_identity();

    refresh_shader_transform();
}

void draw_persp(Vector3 pos, Quaternion rot, f32 aspect_ratio, f32 fov) {
    draw_state->projection_matrix = m4_persp(fov, aspect_ratio, FAR_CLIP_PLANE, NEAR_CLIP_PLANE);
    draw_state->view_matrix       = m4_mul(m4_rotate(quat_inverse(rot)), m4_translate(v3_inverse(pos)));
    draw_state->model_matrix      = m4_identity();

    refresh_shader_transform();   
}

void draw_from(Vector2 pos, f32 ortho_size) {
    draw_state->projection_matrix = m4_ortho(ortho_size, (f32)draw_state->back_buffer.width / (f32)draw_state->back_buffer.height, FAR_CLIP_PLANE, NEAR_CLIP_PLANE);
    draw_state->view_matrix       = m4_translate(v3_inverse(v3xy(pos, 0.f)));
    draw_state->model_matrix      = m4_identity();

    refresh_shader_transform();
}

void imm_begin(void) {
    imm_renderer->vertex_count = 0;
}

void imm_flush(void) {
    Shader* bound_shader = get_bound_shader();
    static b32 thrown_bound_shader_error = false;
    if (!bound_shader) {
        if (!thrown_bound_shader_error) { 
            o_log_error("[Draw] Tried to flush imm_renderer when no shader was bound. \n");
            thrown_bound_shader_error = true;
        }
        return;
    }
    thrown_bound_shader_error = false;

    glBindVertexArray(imm_renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, imm_renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(imm_renderer->vertices[0]) * imm_renderer->vertex_count, imm_renderer->vertices, GL_STREAM_DRAW);

    set_imm_vertex_format();
    
    glDrawArrays(GL_TRIANGLES, 0, imm_renderer->vertex_count);

    draw_state->num_draw_calls += 1;
}

void set_imm_vertex_format(void) {
    GLuint position_loc = 0;
    glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), 0);
    glEnableVertexAttribArray(position_loc);
    
    GLuint normal_loc = 1;
    glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void*)sizeof(Vector3));
    glEnableVertexAttribArray(normal_loc);

    GLuint uv_loc = 2;
    glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void*)(sizeof(Vector3) + sizeof(Vector3)));
    glEnableVertexAttribArray(uv_loc);

    GLuint color_loc = 3;
    glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, sizeof(Immediate_Vertex), (void*)(sizeof(Vector3) + sizeof(Vector3) + sizeof(Vector2)));
    glEnableVertexAttribArray(color_loc);
}

void imm_vertex(Vector3 position, Vector3 normal, Vector2 uv, Vector4 color) {
    if (imm_renderer->vertex_count == MAX_IMM_VERTS) {
        imm_flush();
        imm_begin();
    }

    Immediate_Vertex* this_vertex = imm_renderer->vertices + imm_renderer->vertex_count++;
    *this_vertex = (Immediate_Vertex) { position, normal, uv, color };
}

void imm_textured_rect(Rect rect, f32 z, Vector2 uv0, Vector2 uv1, Vector4 color) {
    Vector3 top_left_pos      = v3(rect.min.x, rect.max.y, z);
    Vector3 top_right_pos     = v3xy(rect.max, z);
    Vector3 bottom_left_pos   = v3xy(rect.min, z);
    Vector3 bottom_right_pos  = v3(rect.max.x, rect.min.y, z);

    Vector2 top_left_uv       = v2(uv0.x, uv1.y);
    Vector2 top_right_uv      = v2(uv1.x, uv1.y);
    Vector2 bottom_left_uv    = v2(uv0.x, uv0.y);
    Vector2 bottom_right_uv   = v2(uv1.x, uv0.y);

    Vector3 normal = v3z(); // @Incomplete

    imm_vertex(top_left_pos, normal, top_left_uv, color);
    imm_vertex(bottom_left_pos, normal, bottom_left_uv, color);
    imm_vertex(top_right_pos, normal, top_right_uv, color);

    imm_vertex(bottom_left_pos, normal, bottom_left_uv, color);
    imm_vertex(bottom_right_pos, normal, bottom_right_uv, color);
    imm_vertex(top_right_pos, normal, top_right_uv, color);
}

void imm_textured_border_rect(Rect rect, f32 z, f32 thickness, Vector2 uv0, Vector2 uv1, Vector4 color) {
    Vector2 size = rect_size(rect);

    // Left quad
    {
        Rect    left      = (Rect) { rect.min, v2(rect.min.x + thickness, rect.max.y) };
        Vector2 left_uv1  = v2(uv0.x + thickness / size.width * (uv1.x - uv0.x), uv1.y);
        imm_textured_rect(left, z, uv0, left_uv1, color);
    }

    // Right quad
    {
        Rect right        = (Rect) { v2(rect.max.x - thickness, rect.min.y), rect.max };
        Vector2 right_uv0 = v2(uv1.x - thickness / size.width * (uv1.x - uv0.x), uv0.y);
        Vector2 right_uv1 = v2(uv1.x - thickness / size.width * (uv1.x - uv0.x), uv1.y);
        imm_textured_rect(right, z, right_uv0, right_uv1, color);
    }

    // Top quad
    {
        Rect top          = (Rect) { v2(rect.min.x, rect.max.y - thickness), rect.max };
        Vector2 top_uv0   = v2(uv0.x, uv1.y - thickness / size.height * (uv1.y - uv0.y));
        imm_textured_rect(top, z, top_uv0, uv1, color);
    }

    // Bottom quad
    {
        Rect bot          = (Rect) { rect.min, v2(rect.max.x, rect.min.y + thickness) };
        Vector2 bot_uv1   = v2(uv1.x, uv0.y + thickness / size.width * (uv1.y - uv0.y));
        imm_textured_rect(bot, z, uv0, bot_uv1, color);
    }
}

void imm_textured_line(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector2 uv0, Vector2 uv1, Vector4 color) {
    f32 height    = thickness / 2.f;
    Vector2 dir   = v2_norm(v2_sub(a2, a1));
    Vector2 perp  = v2_mul(v2_perp(dir), v2s(height));

    Vector3 top_left      = v3xy(v2_sub(a2, perp), z);
    Vector3 bottom_left   = v3xy(v2_sub(a1, perp), z);
    Vector3 top_right     = v3xy(v2_add(a2, perp), z);
    Vector3 bottom_right  = v3xy(v2_add(a1, perp), z);

    Vector2 top_left_uv       = v2(uv0.x, uv1.y);
    Vector2 top_right_uv      = v2(uv1.x, uv1.y);
    Vector2 bottom_left_uv    = v2(uv0.x, uv0.y);
    Vector2 bottom_right_uv   = v2(uv1.x, uv0.y);
    
    Vector3 normal = v3s(0.f); // @Incomplete

    imm_vertex(top_left, normal, top_left_uv, color);
    imm_vertex(bottom_left, normal, bottom_left_uv, color);
    imm_vertex(top_right, normal, top_right_uv, color);

    imm_vertex(bottom_left, normal, bottom_left_uv, color);
    imm_vertex(bottom_right, normal, bottom_right_uv, color);
    imm_vertex(top_right, normal, top_right_uv, color);
}

void imm_arrow(Vector2 a1, Vector2 a2, f32 z, f32 thickness, Vector4 color) {
    f32 height = thickness * 2.f;
    Vector2 dir = v2_norm(v2_sub(a1, a2));
    Vector2 perp = v2_mul(v2_perp(dir), v2s(height));

    Vector2 down = v2_mul(dir, v2s(height));
    Vector2 l2 = v2_add(v2_add(a2, v2(-perp.x, -perp.y)), down);
    Vector2 r2 = v2_add(v2_add(a2, perp), down);

    imm_line(a2, l2, z, thickness, color);
    imm_line(a2, r2, z, thickness, color);
    imm_line(a1, a2, z, thickness, color);
}

void imm_textured_circle(f32 radius, int segments, Vector2 xy, f32 z, Vector2 uv0, Vector2 uv1, Vector4 color) {
    assert(segments >= 3);

    f32 offset = (4.f * PI) / (f32)segments;
    f32 theta = 0.f;
    for (int i = 0; i < segments; ++i) {
        Vector2 dir0 = v2rad(theta);
        Vector2 dir1 = v2rad(theta + offset);

        Vector3 p0 = v3xy(xy, z);
        Vector3 p1 = v3xy(v2_add(p0.xy, v2_mul(dir0, v2s(radius))), z);
        Vector3 p2 = v3xy(v2_add(p0.xy, v2_mul(dir1, v2s(radius))), z);

        Vector3 normal = v3z(); // @Incomplete

        imm_vertex(p0, normal, uv0, color);
        imm_vertex(p1, normal, uv0, color);
        imm_vertex(p2, normal, uv0, color);

        theta += offset;
    }
}

void imm_glyph(Font_Glyph* g, Font* font, Vector2 xy, f32 z, Vector4 color) {
    // Draw font from bottom up. Yes this makes doing paragraphs harder but it goes along with OpenGL's coordinate system
    xy = v2_sub(xy, v2(0.f, font->descent));
    f32 x0 = xy.x + g->bearing_x;
    f32 y1 = xy.y - g->bearing_y;
    f32 x1 = x0 + g->width;
    f32 y0 = y1 - g->height;
    Rect rect = rect_from_raw(x0, y0, x1, y1);

    imm_textured_rect(rect, z, g->uv0, g->uv1, color);
}

Font_Glyph* imm_rune(Rune r, Font* font, Vector2 xy, f32 z, Vector4 color) {
    Font_Glyph* g = glyph_from_rune(font, r);
    if (g != 0) {
        imm_glyph(g, font, xy, z, color);
        return g;
    }

    return 0;
}

void imm_string(String str, Font* font, f32 max_width, Vector2 xy, f32 z, Vector4 color) {
    Vector2 orig_xy = xy;

    Font_Glyph* space_g = glyph_from_rune(font, ' ');
    for (int i = 0; i < str.len; ++i) {
        if (xy.x + space_g->advance > orig_xy.x + max_width) {
            xy.x = orig_xy.x;
            xy.y -= (f32)font->size;
        }

        char c = str.data[i]; // @TODO(colby): UTF8
        switch (c) {
        case '\n': {
            xy.x = orig_xy.x;
            xy.y -= (f32)font->size;
        } break;
        case '\r': {
            xy.x = orig_xy.x;
        } break;
        case '\t': {
            xy.x += space_g->advance * 4.f;
        } break;
        default: {
            Font_Glyph* g = imm_rune(c, font, xy, z, color);
            xy.x += g->advance;
        } break;
        }
    }
}

void imm_textured_plane(Vector3 pos, Quaternion rot, Rect rect, Vector2 uv0, Vector2 uv1, Vector4 color) {
    Vector3 right = quat_right(rot);
    Vector3 forward = quat_forward(rot);

    Vector3 left = v3_mul(right, v3s(rect.min.x));
    Vector3 back = v3_mul(forward, v3s(rect.min.y));
    right = v3_mul(right, v3s(rect.max.x));
    forward  = v3_mul(forward, v3s(rect.max.y));

    Vector3 top_left_pos      = v3_add(pos, v3_add(forward, left));
    Vector3 top_right_pos     = v3_add(pos, v3_add(forward, right));
    Vector3 bottom_left_pos   = v3_add(pos, v3_add(back, left));
    Vector3 bottom_right_pos  = v3_add(pos, v3_add(back, right));

    Vector2 top_left_uv       = v2(uv0.x, uv1.y);
    Vector2 top_right_uv      = v2(uv1.x, uv1.y);
    Vector2 bottom_left_uv    = v2(uv0.x, uv0.y);
    Vector2 bottom_right_uv   = v2(uv1.x, uv0.y);

    Vector3 normal = v3s(0.f); // @Incomplete

    imm_vertex(top_left_pos, normal, top_left_uv, color);
    imm_vertex(bottom_left_pos, normal, bottom_left_uv, color);
    imm_vertex(top_right_pos, normal, top_right_uv, color);

    imm_vertex(bottom_left_pos, normal, bottom_left_uv, color);
    imm_vertex(bottom_right_pos, normal, bottom_right_uv, color);
    imm_vertex(top_right_pos, normal, top_right_uv, color);
}