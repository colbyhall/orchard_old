#ifdef VERTEX
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform vec4 color;

out vec4 frag_color;
out vec3 frag_normal;
out vec3 frag_position;
out vec2 frag_uv;

void main() {
    gl_Position = projection * view * model * vec4(position, 1.0);

    frag_color = color;
    frag_normal = normalize((model * vec4(normal, 0.0)).xyz);
    frag_position = gl_Position.xyz;
    frag_uv = uv;
}
#endif

#ifdef FRAGMENT
layout (location = 0) out vec4 diffuse;
layout (location = 1) out vec4 normal;
layout (location = 2) out vec4 position;

in vec4 frag_color;
in vec3 frag_normal;
in vec3 frag_position;
in vec2 frag_uv;

uniform sampler2D diffuse_tex;

void main() {

    vec3 light_pos = vec3(0.0, 300.0, 0.0);
    vec3 light_dir = normalize(light_pos - frag_position);
    vec3 light_color = vec3(1.0);

    float ambient = 0.1f;
    float diff = max(dot(frag_normal, light_dir), 0.0);

    normal = vec4(frag_normal, 1.0);
    diffuse = vec4(((ambient + diff) * light_color) * frag_color.xyz, 1.0);
    position = vec4(frag_position, 1.0);
}
#endif