#ifdef VERTEX
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 color;
uniform mat4 projection;
uniform mat4 view;
out vec4 frag_color;
out vec3 frag_normal;
out vec3 frag_position;
out vec2 frag_uv;
void main() {
    gl_Position =  projection * view * vec4(position, 1.0);
    frag_color = color;
    frag_normal = normal;
    frag_position = gl_Position.xyz;
    frag_uv = uv;
}
#endif
#ifdef FRAGMENT
layout (location = 0) out vec4 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec3 position;
in vec4 frag_color;
in vec3 frag_normal;
in vec3 frag_position;
in vec2 frag_uv;

uniform sampler2D diffuse_tex;

void main() {
    if (frag_uv.x < 0.0) {
        diffuse = frag_color;
    } else {
        diffuse = texture(diffuse_tex, frag_uv);
    }
    normal = frag_normal;
    position = frag_position;
}
#endif