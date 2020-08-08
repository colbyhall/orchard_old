#include "asset.h"
#include "debug.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <fast_obj/fast_obj.h>

enum Asset_Flags {
    AF_Initialized = (1 << 0),
};

static char* asset_type_string[] = {
    "None",
    "Shader",
    "Texture2d",
    "Font Collection",
    "Mesh"
};

#define ASSET_TYPE_DEFINITION(def) \
def(AT_Shader, load_shader, unload_null) \
def(AT_Texture2d, load_texture2d, unload_null) \
def(AT_Font_Collection, load_font_collection, unload_null) \
def(AT_Mesh, load_mesh, unload_null)

static b32 load_null(Asset* asset, String file, Allocator asset_memory) { return false; }
static b32 unload_null(Asset* asset, Allocator asset_memory) { return false; }

static b32 load_shader(Asset* asset, String file, Allocator asset_memory) {
    Shader* shader = &asset->shader;

    shader->source = copy_string(file, asset_memory);
    return init_shader(shader);
}

static b32 load_texture2d(Asset* asset, String file, Allocator asset_memory) {
    Texture2d* texture = &asset->texture2d;

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
    Font_Collection* fc = &asset->font_collection;
    return init_font_collection(expand_string(file), asset_memory, fc);
}

static u64 hash_mesh_vertex(void* a, void* b, int size) {
    assert(sizeof(Mesh_Vertex) == size);

    Mesh_Vertex* a_vert = a;

    if (b) {
        Mesh_Vertex* b_vert = b;
        return v3_equal(a_vert->position, b_vert->position) && v3_equal(a_vert->normal, b_vert->normal) && v2_equal(a_vert->uv, b_vert->uv);
    }

    return fnv1_hash(a, size);
}

static b32 load_mesh(Asset* asset, String file, Allocator asset_memory) {
    Mesh* mesh = &asset->mesh;
    fastObjMesh* fast_obj_mesh = fast_obj_read((char*)asset->path.data);
    if (!fast_obj_mesh) return false;

    int index_cap = 0;
    for (int i = 0; i < (int)fast_obj_mesh->group_count; ++i) {
        fastObjGroup* group = fast_obj_mesh->groups + i;

        for (int j = 0; j < (int)group->face_count; ++j) {
            int vertex_count = fast_obj_mesh->face_vertices[group->face_offset + j];

            if (vertex_count == 3) index_cap += 3;
            else if (vertex_count == 4) index_cap += 6;
            else assert(false); // @Incomplete!!! does this even happen???
        }
    }

    u32* indices = mem_alloc_array(asset_memory, u32, index_cap);
    int index_count = 0;

    Hash_Table vertex_index_table = make_hash_table(Mesh_Vertex, int, hash_mesh_vertex, g_platform->permanent_arena);
    reserve_hash_table(&vertex_index_table, index_cap / 3);

    for (int i = 0; i < (int)fast_obj_mesh->group_count; ++i) {
        fastObjGroup* group = fast_obj_mesh->groups + i;

        int index_offset = group->index_offset;
        for (int j = 0; j < (int)group->face_count; ++j) {
            int vertex_count = fast_obj_mesh->face_vertices[group->face_offset + j];

            int found_indices[4];

            for (int k = 0; k < vertex_count; ++k) {
                fastObjIndex* index = fast_obj_mesh->indices + index_offset + k;
                
                float* p = fast_obj_mesh->positions + index->p * 3;
                Vector3 position = v3(-p[2], p[0], p[1]);

                float* n = fast_obj_mesh->normals + index->n * 3;
                Vector3 normal = v3(-n[2], n[0], n[1]);

                float* u = fast_obj_mesh->texcoords + index->t * 2;
                Vector2 uv = v2(u[0], u[1]);

                Mesh_Vertex vertex = { position, normal, uv };
                int* found_indice = push_hash_table(&vertex_index_table, vertex, vertex_index_table.pair_count);
                if (found_indice) found_indices[k] = *found_indice;
                else found_indices[k] = vertex_index_table.pair_count - 1;
            }
            if (vertex_count == 3) {
                mem_copy(indices + index_count, found_indices, vertex_count * sizeof(u32));
                index_count += 3;
            } else if (vertex_count == 4) {
                indices[index_count + 0] = found_indices[0];
                indices[index_count + 1] = found_indices[1];
                indices[index_count + 2] = found_indices[2];

                indices[index_count + 3] = found_indices[0];
                indices[index_count + 4] = found_indices[2];
                indices[index_count + 5] = found_indices[3];

                index_count += 6;
            }
            
            index_offset += vertex_count;
        }
    }

    mesh->vertices = mem_alloc_array(asset_memory, Mesh_Vertex, vertex_index_table.pair_count);
    mesh->vertex_count = vertex_index_table.pair_count;
    mem_copy(mesh->vertices, vertex_index_table.keys, sizeof(Mesh_Vertex) * mesh->vertex_count);
    mesh->indices = indices;
    mesh->index_count = index_count;

    upload_mesh(mesh);

    return true;
}

#define ASSET_CAP 1024 // This can be increased if needed
#define PATH_MEMORY_CAP (ASSET_CAP * 1024) // Rough Estimate
#define ASSET_MEMORY_CAP gigabyte(1)
typedef struct Asset_Manager {
    Asset assets[ASSET_CAP];
    int asset_count;

    Allocator path_memory;
    Allocator asset_memory;

    b32 is_initialized;
} Asset_Manager;

static Asset_Manager* asset_manager = 0;

typedef struct Asset_Type_Extension {
    Asset_Type type;
    const char* ext;
} Asset_Type_Extension;

static Asset_Type_Extension asset_type_extensions[] = {
    { AT_Shader,    "glsl"},
    { AT_Texture2d, "png" },
    { AT_Texture2d, "jpg" },
    { AT_Texture2d, "jpeg"},
    { AT_Texture2d, "psd" },
    { AT_Texture2d, "bmp" },
    { AT_Mesh,      "obj" },
    { AT_Font_Collection, "ttf" },
};

static Asset_Type get_asset_type_from_path(String path) {
    int period_index = find_from_left(path, '.');
    assert(period_index != -1);
    String ext = advance_string(path, period_index + 1);
    
    for (int i = 0; i < array_count(asset_type_extensions); ++i) {
        Asset_Type_Extension ate = asset_type_extensions[i]; // Nom nom nom
        if (string_equal(ext, from_cstr(ate.ext))) return ate.type;
    }

    return AT_None;
}

void init_asset_manager(Platform* platform) {
    asset_manager = mem_alloc_struct(platform->permanent_arena, Asset_Manager);
    asset_manager->path_memory  = arena_allocator(platform->permanent_arena, PATH_MEMORY_CAP);
    asset_manager->asset_memory = arena_allocator(platform->permanent_arena, ASSET_MEMORY_CAP); // @TODO(colby): do pool allocator

    if (asset_manager->is_initialized) return;
    asset_manager->is_initialized = true;

    for (directory_iterator(from_cstr("assets/"), true, platform->frame_arena)) {
        Temp_Memory temp_memory = begin_temp_memory(platform->frame_arena);
        if (iter->type != DET_File) continue;

        f64 start_time = g_platform->time_in_seconds();

        Asset_Type type = get_asset_type_from_path(iter->path);
        if (!type) continue;

        Asset* asset = &asset_manager->assets[asset_manager->asset_count++];
        asset->path = copy_string(iter->path, asset_manager->path_memory);
        asset->type = type;
        
        String file;
        if (!read_file_into_string(iter->path, &file, asset_manager->asset_memory)) {
            end_temp_memory(temp_memory);
            o_log_error("[Asset] %s failed to load file from path %s", asset_type_string[type], (const char*)iter->path.data);
            continue;
        }

        switch (type) {
#define LOAD_ASSET(at, load, unload) \
        case at: \
            if (!load(asset, file, asset_manager->asset_memory)) { \
                end_temp_memory(temp_memory); \
                o_log_error("[Asset] %s failed to initialize from path %s", asset_type_string[type], (const char*)iter->path.data); \
                continue; \
            } else { \
                f64 duration = g_platform->time_in_seconds() - start_time; \
                o_log("[Asset] took %ims to load %s from path %s", (int)(duration * 1000.0), asset_type_string[type], (const char*)iter->path.data); \
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
    for (int i = 0; i < asset_manager->asset_count; ++i) {
        Asset* asset = &asset_manager->assets[i];
        if (starts_with(asset->path, path)) return asset;
    }

    return 0;
}