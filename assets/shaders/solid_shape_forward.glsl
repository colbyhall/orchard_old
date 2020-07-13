#ifdef VERTEX
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 color;
uniform mat4 projection;
uniform mat4 view;
out vec4 out_color;
out vec2 out_uv;
void main() {
    gl_Position =  projection * view * vec4(position, 1.0);
    out_color = color;
    out_uv = uv;
}
#endif
#ifdef FRAGMENT
out vec4 frag_color;
in vec4 out_color;
in vec2 out_uv;

uniform sampler2D tex;

void main() {
    if (out_uv.x < 0.0) {
        frag_color = out_color;
    } else {
        frag_color = texture(tex, out_uv);
    }
}
#endif