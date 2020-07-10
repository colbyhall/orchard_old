#ifndef ASSET_H
#define ASSET_H

#include "draw.h"
#include "opengl.h"

typedef enum Asset_Type {
    AT_None,
    AT_Shader,
    AT_Texture2d,
    AT_Font_Collection,
    AT_End,
    AT_Count = AT_End - 1,
} Asset_Type;

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

void init_asset_manager(Platform* platform);
Asset* find_asset(String path);
inline Shader* find_shader(String path) { 
    Asset* const found = find_asset(path);
    if (found && found->type == AT_Shader) return &found->shader;
    return 0;
}

inline Texture2d* find_texture2d(String path) { 
    Asset* const found = find_asset(path);
    if (found && found->type == AT_Texture2d) return &found->texture2d;
    return 0;
}

inline Font_Collection* find_font_collection(String path) { 
    Asset* const found = find_asset(path);
    if (found && found->type == AT_Font_Collection) return &found->font_collection;
    return 0;
}

#endif /* ASSET_H */