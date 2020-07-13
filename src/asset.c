#include "asset.h"
#include "debug.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <fast_obj/fast_obj.h>

enum Asset_Flags {
    AF_Initialized = (1 << 0),
};

static const char* asset_type_string[] = {
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

static u64 hash_mesh_vertex(void* a, void* b, int size) {
    assert(sizeof(Mesh_Vertex) == size);

    const Mesh_Vertex* const a_vert = a;

    if (b) {
        const Mesh_Vertex* const b_vert = b;
        return v3_equal(a_vert->position, b_vert->position) && v3_equal(a_vert->normal, b_vert->normal) && v2_equal(a_vert->uv, b_vert->uv);
    }

    return fnv1_hash(a, size);
}

static b32 load_mesh(Asset* asset, String file, Allocator asset_memory) {
    Mesh* const mesh = &asset->mesh;
    fastObjMesh* const fast_obj_mesh = fast_obj_read((const char*)asset->path.data);
    if (!fast_obj_mesh) return false;

    int index_cap = 0;
    for (int i = 0; i < (int)fast_obj_mesh->group_count; ++i) {
        const fastObjGroup* const group = fast_obj_mesh->groups + i;

        for (int j = 0; j < (int)group->face_count; ++j) {
            const int vertex_count = fast_obj_mesh->face_vertices[group->face_offset + j];
            if (vertex_count == 3) index_cap += 3;
            else if (vertex_count == 4) index_cap += 6;
            else assert(false); // @Incomplete!!! does this even happen???
        }
    }

    u32* indices = mem_alloc_array(asset_memory, u32, index_cap);
    int index_count = 0;

    Hash_Table vertex_index_table = make_hash_table(Mesh_Vertex, int, hash_mesh_vertex, g_platform->permanent_arena);

    f32 time_messing_with_hash = 0.f;

    for (int i = 0; i < (int)fast_obj_mesh->group_count; ++i) {
        const fastObjGroup* const group = fast_obj_mesh->groups + i;

        int index_offset = group->index_offset;
        for (int j = 0; j < (int)group->face_count; ++j) {
            const int vertex_count = fast_obj_mesh->face_vertices[group->face_offset + j];

            int found_indices[4];

            for (int k = 0; k < vertex_count; ++k) {
                const fastObjIndex* const index = fast_obj_mesh->indices + index_offset + k;
                
                const float* const p = fast_obj_mesh->positions + index->p * 3;
                const Vector3 position = v3(p[0], p[1], p[2]);

                const float* const n = fast_obj_mesh->normals + index->n * 3;
                const Vector3 normal = v3(n[0], n[1], n[2]);

                const float* const u = fast_obj_mesh->texcoords + index->t * 2;
                const Vector2 uv = v2(u[0], u[1]);

                const f32 time = g_platform->time_in_seconds();

                Mesh_Vertex vertex = { position, normal, uv };
                if (push_hash_table(&vertex_index_table, vertex, vertex_index_table.pair_count)) {
                    found_indices[k] = vertex_index_table.pair_count - 1;
                } else {
                    int* v_index = find_hash_table(&vertex_index_table, vertex);
                    found_indices[k] = *v_index;
                }

                time_messing_with_hash += g_platform->time_in_seconds() - time;
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

    o_log("[Asset] spent %fs messing with hash table", time_messing_with_hash);

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
    { AT_Shader,    "glsl" },
    { AT_Texture2d, "png" },
    { AT_Texture2d, "jpg" },
    { AT_Texture2d, "jpeg" },
    { AT_Texture2d, "bmp" },
    { AT_Mesh,      "obj" },
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

        const f32 start_time = g_platform->time_in_seconds();

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
            } else { \
                const f32 duration = g_platform->time_in_seconds() - start_time; \
                o_log("[Asset] took %fs to load %s from path %s", duration, asset_type_string[type], (const char*)iter->path.data); \
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