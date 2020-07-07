#include "draw.h"

typedef enum Asset_Type {
    AT_Shader,
    AT_Texture2d,
    AT_Font_Collection,
} Asset_Type;

enum Asset_Flags {
    AF_Initialized = (1 << 0),
};

typedef struct Asset {
    String      path;
    int         flags;
    Asset_Type  type;
    union {
        Shader          shader;
        Texture2d       texture2d;
        Font_Collection font_collection;
    };
} Asset;

#define ASSET_CAP 1024 // This can be increased if needed
typedef struct Asset_Manager {
    Asset assets[ASSET_CAP];
    int asset_count;

    Allocator path_memory;
    Allocator asset_memory;

    b32 is_initialized;
} Asset_Manager;

static Asset_Manager* g_asset_manager = 0;

void init_asset_manager(Platform* platform) {
    g_asset_manager = mem_alloc_struct(platform->permanent_arena, Asset_Manager);

    if (g_asset_manager->is_initialized) return;

    for (directory_iterator("assets/", true, platform->permanent_arena)) {
        printf("%s\n", iter->path.data);
    }

    g_asset_manager->is_initialized = true;
}
