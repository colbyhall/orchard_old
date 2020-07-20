#ifdef VERTEX
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 frag_normal;
out vec3 frag_position;
out vec2 frag_uv;

void main() {
    gl_Position = projection * view * model * vec4(position, 1.0);

    mat3 inv_model = mat3(transpose(inverse(model))); 
    frag_normal   = normalize(inv_model * normal);
    frag_position = inv_model * position;
    frag_uv       = uv;
}
#endif

#ifdef FRAGMENT
layout (location = 0) out vec4 position;
layout (location = 1) out vec4 normal;
layout (location = 2) out vec4 albedo;

in vec3 frag_position;
in vec3 frag_normal;
in vec2 frag_uv;

uniform vec4 color;
uniform sampler2D diffuse_tex;

void main() {
    position = vec4(frag_position, 1.0);
    normal   = vec4(frag_normal, 1.0);
    albedo   = vec4(color.xyz, 1.0);
}
#endif