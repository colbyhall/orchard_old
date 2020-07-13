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
    Matrix4 model = m4_mul(m4_mul(m4_translate(position), m4_rotate(rotation)), m4_scale(scale));
    set_uniform_m4("model", model);
    set_uniform_v4("color", v4s(1.f));

    glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->vio);

    set_mesh_vertex_format();

    glDrawElements(GL_TRIANGLES, m->index_count, GL_UNSIGNED_INT, 0);
}

void set_mesh_vertex_format(void) {
    const GLuint position_loc = 0;
    glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, sizeof(Mesh_Vertex), 0);
    glEnableVertexAttribArray(position_loc);
    
    const GLuint normal_loc = 1;
    glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, sizeof(Mesh_Vertex), (void*)sizeof(Vector3));
    glEnableVertexAttribArray(normal_loc);

    const GLuint uv_loc = 2;
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
    // @Temporary: linearly search the codepoint space because STBTT doesn't expose CP->glyph idx;
    //             later we will parse the ttf file in a similar way to STBTT.
    //             Linear search is exactly 17 times slower than parsing for 65536 glyphs.
    for (int codepoint = 0; codepoint < 0x110000; ++codepoint) {
        const int idx = stbtt_FindGlyphIndex(&collection->info, codepoint);
        if (idx <= 0) continue;
        glyphs_found += 1;
        collection->codepoint_indices[idx] = codepoint;
    }

    // Find the atlas area
    f32 atlas_area = 0.f;
    for (int i = 0; i < collection->codepoint_count; ++i) {
        int x0, x1, y0, y1;
        stbtt_GetGlyphBox(&collection->info, i, &x0, &y0, &x1, &y1);

        const f32 width  = (f32)(x1 - x0);
        const f32 height = (f32)(y1 - y0);

        atlas_area += width * height;
    }
    collection->atlas_area = atlas_area;
    collection->asset_memory = asset_memory;

    return true;
}

Font* font_at_size(Font_Collection* collection, int size) {
    size = CLAMP(size, 2, 512);

    for (int i = 0; i < collection->font_count; ++i) {
        Font* const f = &collection->fonts[i];
        if (f->size == size) {
            return f;
        }
    }

    assert(collection->font_count + 1 < FONT_CAP);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&collection->info, &ascent, &descent, &line_gap);

    Font* const f = &collection->fonts[collection->font_count++];

    f->size = size;
    f->info = &collection->info;
    f->glyph_count = collection->codepoint_count;

    const f32 font_scale = stbtt_ScaleForPixelHeight(&collection->info, (f32)size);
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

    Texture2d* const atlas = &f->atlas;
    atlas->depth = 1;

    // @Cleanup @Cleanup @Cleanup
    if (size <= 12) {
        atlas->width  = 512 * h_oversample;
        atlas->height = 512 * v_oversample;
    } else {
        f32 area = collection->atlas_area * h_oversample * v_oversample;
        area *= 1.f + 1.f / sqrtf((f32)size); // fudge factor for small sizes
        area *= font_scale * font_scale;

        const f32 root = sqrtf(area);

        u32 atlas_dimension = (u32)root;
        atlas_dimension = (atlas_dimension + 127) & ~127;

        atlas->width  = atlas_dimension;
        atlas->height = atlas_dimension;
    }

    atlas->pixels = mem_alloc_array(collection->asset_memory, u8, atlas->width * atlas->height); // @Leak

    stbtt_pack_context pc;
    stbtt_packedchar* const pdata = mem_alloc_array(collection->asset_memory, stbtt_packedchar, collection->codepoint_count); // @Leak

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

    const b32 ok = upload_texture2d(atlas);
    if (!ok) assert(false);

    f->glyphs = mem_alloc_array(collection->asset_memory, Font_Glyph, collection->codepoint_count);
    for (int i = 0; i < collection->codepoint_count; ++i) {
        Font_Glyph* const g = f->glyphs + i;

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
    const int index = stbtt_FindGlyphIndex(f->info, r);
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
#define MAX_IMM_VERTS (1024 * 3)

typedef struct Immediate_Renderer {
    GLuint vao, vbo;
    Immediate_Vertex vertices[MAX_IMM_VERTS];
    int vertex_count;

    Matrix4 projection;
    Matrix4 view;

    b32 is_initialized;
} Immediate_Renderer;
static Immediate_Renderer* g_imm_renderer = 0;

static Framebuffer* g_back_buffer;
#define BACK_BUFFER_WIDTH   512
#define BACK_BUFFER_HEIGHT  288

Shader* g_solid_shape_shader = 0;
Shader* g_solid_shape_geometry_shader = 0;
Shader* g_solid_shape_lighting_shader = 0;
Shader* g_font_shader = 0;

#define FAR_CLIP_PLANE 1000.f
#define NEAR_CLIP_PLANE 0.1


void init_draw(Allocator allocator) {
    g_imm_renderer      = mem_alloc_struct(allocator, Immediate_Renderer);
    g_back_buffer       = mem_alloc_struct(allocator, Framebuffer);
    g_font_collection   = mem_alloc_struct(allocator, Font_Collection);

    g_solid_shape_shader = find_shader(string_from_raw("assets/shaders/solid_shape_forward"));
    g_solid_shape_geometry_shader = find_shader(string_from_raw("assets/shaders/solid_shape_geometry"));
    g_solid_shape_lighting_shader = find_shader(string_from_raw("assets/shaders/solid_shape_lighting"));
    g_font_shader = find_shader(string_from_raw("assets/shaders/font"));

    if (g_imm_renderer->is_initialized) return;

    glGenVertexArrays(1, &g_imm_renderer->vao);
    glBindVertexArray(g_imm_renderer->vao);

    glGenBuffers(1, &g_imm_renderer->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_imm_renderer->vbo);

    glBindVertexArray(0);

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
    g_imm_renderer->is_initialized = true;

    set_shader(g_solid_shape_shader);

    if (!init_framebuffer(BACK_BUFFER_WIDTH, BACK_BUFFER_HEIGHT, FF_GBuffer, g_back_buffer)) assert(false);

    String font_file;
    if (!read_file_into_string(string_from_raw("assets\\fonts\\consola.ttf"), &font_file, allocator)) assert(false);

    if (!init_font_collection(expand_string(font_file), allocator, g_font_collection)) assert(false);
}

void imm_refresh_transform(void) {
    set_uniform_m4("view",         g_imm_renderer->view);
    set_uniform_m4("projection",   g_imm_renderer->projection);
}

void imm_draw_right_handed(Rect viewport) {
    const Vector2 draw_size = rect_size(viewport);
    const f32 aspect_ratio = draw_size.width / draw_size.height;
    const f32 ortho_size = draw_size.height / 2.f;

    g_imm_renderer->projection  = m4_ortho(ortho_size, aspect_ratio, FAR_CLIP_PLANE, NEAR_CLIP_PLANE);
    g_imm_renderer->view        = m4_translate(v3(-draw_size.width / 2.f, -ortho_size, 0.f));

    imm_refresh_transform();
}

void imm_draw_ortho(Vector3 pos, f32 aspect_ratio, f32 ortho_size) {
    g_imm_renderer->projection = m4_ortho(ortho_size, aspect_ratio, FAR_CLIP_PLANE, NEAR_CLIP_PLANE);
    g_imm_renderer->view = m4_translate(v3_negate(pos));

    imm_refresh_transform();
}

void imm_draw_from(Vector3 pos) {
    pos.x = roundf(pos.x);
    pos.y = roundf(pos.y);
    pos.z = roundf(pos.z);

    const f32 ortho_size    = (f32)BACK_BUFFER_HEIGHT / 2.f;
    const f32 aspect_ratio = (f32)BACK_BUFFER_WIDTH / (f32)BACK_BUFFER_HEIGHT;
    g_imm_renderer->projection = m4_ortho(ortho_size, aspect_ratio, FAR_CLIP_PLANE, NEAR_CLIP_PLANE);
    g_imm_renderer->view = m4_translate(v3_negate(pos));    

    imm_refresh_transform();
}

void imm_draw_persp(Vector3 pos) {
    pos.x = roundf(pos.x);
    pos.y = roundf(pos.y);
    pos.z = roundf(pos.z);

    const f32 aspect_ratio = (f32)BACK_BUFFER_WIDTH / (f32)BACK_BUFFER_HEIGHT;
    g_imm_renderer->projection = m4_persp(90.f, aspect_ratio, FAR_CLIP_PLANE, NEAR_CLIP_PLANE);
    g_imm_renderer->view = m4_translate(v3_negate(pos));

    imm_refresh_transform();   
}

void imm_begin(void) {
    g_imm_renderer->vertex_count = 0;
}

void imm_flush(void) {
    Shader* const bound_shader = get_bound_shader();
    static b32 thrown_bound_shader_error = false;
    if (!bound_shader) {
        if (!thrown_bound_shader_error) { 
            o_log_error("[Draw] Tried to flush imm_renderer when no shader was bound. \n");
            thrown_bound_shader_error = true;
        }
        return;
    }
    thrown_bound_shader_error = false;

    glBindVertexArray(g_imm_renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_imm_renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_imm_renderer->vertices[0]) * g_imm_renderer->vertex_count, g_imm_renderer->vertices, GL_STREAM_DRAW);

    set_imm_vertex_format();
    
    glDrawArrays(GL_TRIANGLES, 0, g_imm_renderer->vertex_count);
}

void set_imm_vertex_format(void) {
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
}

void begin_draw(void) {
    begin_framebuffer(*g_back_buffer);
    clear_framebuffer(v3s(0.01f));
    set_shader(g_solid_shape_geometry_shader);
}

void end_draw(void) {
    end_framebuffer();

    clear_framebuffer(v3s(0.f));
    glViewport(0, 0, g_platform->window_width, g_platform->window_height);

    set_shader(g_solid_shape_lighting_shader);
    set_uniform_texture("diffuse_tex", g_back_buffer->color[FCI_Diffuse]);

    const Rect viewport = { v2z(), v2((f32)g_platform->window_width, (f32)g_platform->window_height) };
    const f32 viewport_aspect_ratio = viewport.max.width / viewport.max.height;
    const f32 back_buffer_aspect_ratio = (f32)BACK_BUFFER_WIDTH / (f32)BACK_BUFFER_HEIGHT;

    imm_draw_ortho(v3z(), viewport_aspect_ratio, viewport.max.height / 2.f);

    Rect draw_rect;
    if (viewport_aspect_ratio >= back_buffer_aspect_ratio) {
        draw_rect = rect_from_pos(v2z(), v2(
            viewport.max.height * back_buffer_aspect_ratio, 
            viewport.max.height
        ));
    } else {
        const f32 ratio = (f32)BACK_BUFFER_HEIGHT / (f32)BACK_BUFFER_WIDTH;
        draw_rect = rect_from_pos(v2z(), v2(
            viewport.max.width, 
            viewport.max.width * ratio
        ));
    }

    imm_begin();
    imm_textured_rect(draw_rect, -5.f, v2z(), v2s(1.f), v4s(1.f));
    imm_flush();
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

void imm_glyph(Font_Glyph* g, Font* font, Vector2 xy, f32 z, Vector4 color) {
    // Draw font from bottom up. Yes this makes doing paragraphs harder but it goes along with OpenGL's coordinate system
    xy = v2_sub(xy, v2(0.f, font->descent));
    const f32 x0 = xy.x + g->bearing_x;
    const f32 y1 = xy.y - g->bearing_y;
    const f32 x1 = x0 + g->width;
    const f32 y0 = y1 - g->height;
    const Rect rect = rect_from_raw(x0, y0, x1, y1);

    imm_textured_rect(rect, z, g->uv0, g->uv1, color);
}

Font_Glyph* imm_rune(Rune r, Font* font, Vector2 xy, f32 z, Vector4 color) {
    Font_Glyph* const g = glyph_from_rune(font, r);
    if (g != 0) {
        imm_glyph(g, font, xy, z, color);
        return g;
    }

    return 0;
}

void imm_string(String str, Font* font, f32 max_width, Vector2 xy, f32 z, Vector4 color) {
    const Vector2 orig_xy = xy;

    Font_Glyph* const space_g = glyph_from_rune(font, ' ');
    for (int i = 0; i < str.len; ++i) {
        if (xy.x + space_g->advance > orig_xy.x + max_width) {
            xy.x = orig_xy.x;
            xy.y -= (f32)font->size;
        }

        const char c = str.data[i]; // @TODO(colby): UTF8
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
            Font_Glyph* const g = imm_rune(c, font, xy, z, color);
            xy.x += g->advance;
        } break;
        }
    }
}

void imm_textured_plane(Vector3 pos, Quaternion rot, Rect rect, Vector2 uv0, Vector2 uv1, Vector4 color) {
    Vector3 right = quat_right(rot);
    Vector3 up    = quat_up(rot);

    const Vector3 left = v3_mul(right, v3s(rect.min.x));
    const Vector3 down = v3_mul(up, v3s(rect.min.y));
    right = v3_mul(right, v3s(rect.max.x));
    up    = v3_mul(up, v3s(rect.max.y));

    const Vector3 top_left_pos      = v3_add(pos, v3_add(up, left));
    const Vector3 top_right_pos     = v3_add(pos, v3_add(up, right));
    const Vector3 bottom_left_pos   = v3_add(pos, v3_add(down, left));
    const Vector3 bottom_right_pos  = v3_add(pos, v3_add(down, right));

    const Vector2 top_left_uv       = v2(uv0.x, uv1.y);
    const Vector2 top_right_uv      = v2(uv1.x, uv1.y);
    const Vector2 bottom_left_uv    = v2(uv0.x, uv0.y);
    const Vector2 bottom_right_uv   = v2(uv1.x, uv0.y);

    const Vector3 normal = v3s(0.f); // @Incomplete

    imm_vertex(top_left_pos, normal, top_left_uv, color);
    imm_vertex(bottom_left_pos, normal, bottom_left_uv, color);
    imm_vertex(top_right_pos, normal, top_right_uv, color);

    imm_vertex(bottom_left_pos, normal, bottom_left_uv, color);
    imm_vertex(bottom_right_pos, normal, bottom_right_uv, color);
    imm_vertex(top_right_pos, normal, top_right_uv, color);
}