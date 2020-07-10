#include "asset.h"
#include "debug.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

enum Asset_Flags {
    AF_Initialized = (1 << 0),
};

static const char* asset_type_string[] = {
    "None",
    "Shader",
    "Texture2d",
    "Font Collection",
};

#define ASSET_TYPE_DEFINITION(def) \
def(AT_Shader, load_shader, unload_null) \
def(AT_Texture2d, load_texture2d, unload_null) \
def(AT_Font_Collection, load_font_collection, unload_null)

static b32 load_null(Asset* asset, String file, Allocator asset_memory) { return false; }
static b32 unload_null(Asset* asset, Allocator asset_memory) { return false; }

static b32 load_shader(Asset* asset, String file, Allocator asset_memory) {
    Shader* const shader = &asset->shader;

    shader->source = copy_string(file, asset_memory);
    return init_shader(shader);
}

static b32 load_texture2d(Asset* asset, String file, Allocator asset_memory) {
    Texture2d* const texture = &asset->texture2d;

    stbi_set_flip_vertically_on_load(true);

    texture->pixels = stbi_load_from_memory(
        expand_string(file), 
        &texture->width,
        &texture->height,
        &texture->depth,
        0
    );
    return upload_texture2d(texture);
}

static b32 load_font_collection(Asset* asset, String file, Allocator asset_memory) {
    Font_Collection* const fc = &asset->font_collection;
    return init_font_collection(expand_string(file), asset_memory, fc);
}

#define ASSET_CAP 1024 // This can be increased if needed
#define PATH_MEMORY_CAP (ASSET_CAP * 1024) // Rough Estimate
#define ASSET_MEMORY_CAP gigabyte(2)
typedef struct Asset_Manager {
    Asset assets[ASSET_CAP];
    int asset_count;

    Allocator path_memory;
    Allocator asset_memory;

    b32 is_initialized;
} Asset_Manager;

static Asset_Manager* g_asset_manager = 0;

typedef struct Asset_Type_Extension {
    Asset_Type type;
    const char* ext;
} Asset_Type_Extension;

static const Asset_Type_Extension asset_type_extensions[] = {
    { AT_Shader, "glsl" },
    { AT_Texture2d, "png" },
    { AT_Texture2d, "jpg" },
    { AT_Texture2d, "jpeg" },
    { AT_Texture2d, "bmp" },
    { AT_Font_Collection, "ttf" },
};

static Asset_Type get_asset_type_from_path(String path) {
    const int period_index = find_from_left(path, '.');
    assert(period_index != -1);
    String ext = advance_string(path, period_index + 1);
    
    for (int i = 0; i < array_count(asset_type_extensions); ++i) {
        const Asset_Type_Extension ate = asset_type_extensions[i]; // Nom nom nom
        if (string_equal(ext, string_from_raw(ate.ext))) return ate.type;
    }

    return AT_None;
}

void init_asset_manager(Platform* platform) {
    g_asset_manager = mem_alloc_struct(platform->permanent_arena, Asset_Manager);

    if (g_asset_manager->is_initialized) return;

    g_asset_manager->is_initialized = true;

    g_asset_manager->path_memory = arena_allocator(PATH_MEMORY_CAP, platform->permanent_arena);
    g_asset_manager->asset_memory = arena_allocator(ASSET_MEMORY_CAP, platform->permanent_arena); // @TODO(colby): do pool allocator

    for (directory_iterator(string_from_raw("assets/"), true, platform->frame_arena)) {
        Temp_Memory temp_memory = begin_temp_memory(platform->frame_arena);
        if (iter->type != DET_File) continue;

        const Asset_Type type = get_asset_type_from_path(iter->path);
        if (!type) continue;

        Asset* const asset = &g_asset_manager->assets[g_asset_manager->asset_count++];
        asset->path = copy_string(iter->path, g_asset_manager->path_memory);
        asset->type = type;
        
        String file;
        if (!read_file_into_string(iter->path, &file, platform->frame_arena)) {
            end_temp_memory(temp_memory);
            o_log_error("[Asset] %s failed to load file from path %s", asset_type_string[type], (const char*)iter->path.data);
            continue;
        }

        switch (type) {
#define LOAD_ASSET(at, load, unload) \
        case at: \
            if (!load(asset, file, g_asset_manager->asset_memory)) { \
                end_temp_memory(temp_memory); \
                o_log_error("[Asset] %s failed to initialize from path %s", asset_type_string[type], (const char*)iter->path.data); \
                continue; \
            } \
            break;
        ASSET_TYPE_DEFINITION(LOAD_ASSET);
#undef LOAD_ASSET
        default:
            o_log_error("[Asset] %s is missing asset type definition", asset_type_string[type]);
        };

        asset->flags |= AF_Initialized;

        end_temp_memory(temp_memory);
    }
}

Asset* find_asset(String path) {
    // @SPEED @SPEED @SPEED @SPEED @SPEED @SPEED
    for (int i = 0; i < g_asset_manager->asset_count; ++i) {
        Asset* const asset = &g_asset_manager->assets[i];
        if (starts_with(asset->path, path)) return asset;
    }

    return 0;
}