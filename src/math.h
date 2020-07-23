#ifndef MATH_H
#define MATH_H

#include <math.h>

#define PI 3.1415f
#define TAU (PI * 2.f)
#define TO_RAD (PI / 180.f)
#define TO_DEG (180.f / PI)

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a < b ? b : a)
#define CLAMP(x, min, max) (MIN(MAX(x, min), max))
#define SIGN(x) (x < 0.f ? -1.f : 1.f)

inline f32 lerpf(f32 a, f32 b, f32 t) {
    return a * (1.f - t) + b * t;
}

typedef union Vector2 {
    struct { f32 x, y; };
    struct { f32 u, v; };
    struct { f32 width, height; };
    f32 e[2];
} Vector2;

inline Vector2 v2(f32 x, f32 y) { return (Vector2) { x, y }; }
inline Vector2 v2s(f32 s) { return v2(s, s); }
inline Vector2 v2z(void) { return v2s(0.f); }
inline Vector2 v2rad(f32 r) { return v2(sinf(r), cosf(r)); }

inline Vector2 v2_add(Vector2 a, Vector2 b) { return v2(a.x + b.x, a.y + b.y); }
inline Vector2 v2_sub(Vector2 a, Vector2 b) { return v2(a.x - b.x, a.y - b.y); }
inline Vector2 v2_mul(Vector2 a, Vector2 b) { return v2(a.x * b.x, a.y * b.y); }
inline Vector2 v2_div(Vector2 a, Vector2 b) { return v2(a.x / b.x, a.y / b.y); }

inline f32 v2_len_sq(Vector2 a) { return a.x * a.x + a.y * a.y; }
inline f32 v2_len(Vector2 a)    { return sqrtf(v2_len_sq(a)); }

inline Vector2 v2_norm(Vector2 a) {
    f32 len = v2_len(a);
    if (len > 0.f) return v2_div(a, v2s(len));
    return v2z();
}

inline f32 v2_dot(Vector2 a, Vector2 b)     { return a.x * b.x + a.y * b.y; }
inline Vector2 v2_inverse(Vector2 a)        { return v2(-a.x, -a.y); }
inline f32 v2_cross(Vector2 a, Vector2 b)   { return a.x * b.y - a.y * b.x; }
inline Vector2 v2_perp(Vector2 a)           { return v2(a.y, -a.x); }
inline Vector2 v2_negate(Vector2 a)         { return v2(-a.x, -a.y); }
inline Vector2 v2_round(Vector2 a)          { return v2(roundf(a.x), roundf(a.y)); }
inline Vector2 v2_floor(Vector2 a)          { return v2((f32)((int)a.x), (f32)((int)a.y)); }

inline Vector2 v2_lerp(Vector2 a, Vector2 b, f32 t) {
    return v2_add(v2_mul(a, v2s(1.f - t)), v2_mul(b, v2s(t)));
}

inline b32 v2_equal(Vector2 a, Vector2 b) { return a.x == b.x && a.y == b.y; }

static Vector2 v2_up     = { 0.f, 1.f };
static Vector2 v2_right  = { 1.f, 0.f };

typedef union Vector3 {
    struct { f32 x, y, z; };
    struct { f32 r, g, b; };
    struct { f32 u, v; };
    struct { Vector2 xy; };
    struct { f32 _pad; Vector2 yz; };
    f32 e[3];
} Vector3;

inline Vector3 v3(f32 x, f32 y, f32 z) { return (Vector3) { x, y, z }; }
inline Vector3 v3s(f32 s) { return v3(s, s, s); }
inline Vector3 v3z(void) { return v3s(0.f); }
inline Vector3 v3xy(Vector2 xy, f32 z) { return v3(xy.x, xy.y, z); }

inline Vector3 v3_add(Vector3 a, Vector3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vector3 v3_sub(Vector3 a, Vector3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vector3 v3_mul(Vector3 a, Vector3 b) { return v3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline Vector3 v3_div(Vector3 a, Vector3 b) { return v3(a.x / b.x, a.y / b.y, a.z / b.z); }

inline f32 v3_len_sq(Vector3 a) { return a.x * a.x + a.y * a.y + a.z * a.z; }
inline f32 v3_len(Vector3 a) { return sqrtf(v3_len_sq(a)); }

inline Vector3 v3_norm(Vector3 a) {
    f32 len = v3_len(a);
    if (len > 0.f) return v3_div(a, v3s(len));
    return v3z();
}

inline f32 v3_dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vector3 v3_inverse(Vector3 a) { return v3(-a.x, -a.y, -a.z); }
inline Vector3 v3_cross(Vector3 a, Vector3 b) {
    return v3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}
inline b32 v3_equal(Vector3 a, Vector3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

static Vector3 v3_forward = { 1.f, 0.f, 0.f };
static Vector3 v3_right   = { 0.f, 1.f, 0.f };
static Vector3 v3_up      = { 0.f, 0.f, 1.f };

typedef union Vector4 {
    struct { f32 x, y, z, w; };
    struct { f32 u, v; };
    struct { f32 r, g, b, a; };
    struct { f32 _pad[2]; f32 width, height; };
    Vector2 xy;
    Vector3 xyz;
    f32 e[4];
} Vector4;

inline Vector4 v4(f32 x, f32 y, f32 z, f32 w) { return (Vector4) { x, y, z, w }; }
inline Vector4 v4s(f32 s) { return v4(s, s, s, s); }
inline Vector4 color_from_hex(int color) {
    u8 red   = (u8)((color & 0xFF000000) >> 24);
    u8 green = (u8)((color & 0x00FF0000) >> 16);
    u8 blue  = (u8)((color & 0x0000FF00) >> 8);
    u8 alpha = (u8)(color & 0x000000FF);

    return v4((f32)red / 255.f, (f32)green / 255.f, (f32)blue / 255.f, (f32)alpha / 255.f);
}

typedef union Quaternion {
    struct { f32 x, y, z, w; };
    struct { Vector3 xyz; };
    struct { f32 _padding; Vector3 yzw; };
} Quaternion;

static Quaternion quat_identity = { 0.f, 0.f, 0.f, 1.f };

Quaternion quat_from_axis_angle(Vector3 axis, f32 angle_rad);
Quaternion quat_from_euler_angles(f32 roll, f32 pitch, f32 yaw);

inline f32 quat_len_sq(Quaternion a) { return a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w; }
inline f32 quat_len(Quaternion a) { return sqrtf(quat_len_sq(a)); }

inline Quaternion quat_norm(Quaternion a) {
    f32 scale = 1.f / quat_len(a);
    return (Quaternion) { a.x * scale, a.y * scale, a.z * scale, a.w * scale };
}

Vector3 rotate_vector_with_quat(Vector3 a, Quaternion b);
inline Vector3 quat_forward(Quaternion a) { return rotate_vector_with_quat(v3_forward, a); }
inline Vector3 quat_right(Quaternion a) { return rotate_vector_with_quat(v3_right, a); }
inline Vector3 quat_up(Quaternion a) { return rotate_vector_with_quat(v3_up, a); }

inline Quaternion quat_inverse(Quaternion a) { return (Quaternion) { -a.x, -a.y, -a.z, a.w }; }

typedef union Matrix4 {
    f32 col_row[4][4];
    f32 e[4 * 4];
} Matrix4;

Matrix4 m4_mul(Matrix4 a, Matrix4 b);
Matrix4 m4_identity(void);
Matrix4 m4_ortho(f32 size, f32 aspect_ratio, f32 far, f32 near);
Matrix4 m4_persp(f32 fov, f32 asepct_ratio, f32 far, f32 near);
Matrix4 m4_translate(Vector3 translation);
Matrix4 m4_rotate(Quaternion rot);
Matrix4 m4_scale(Vector3 scale);

typedef struct Rect {
    Vector2 min;
    Vector2 max;
} Rect;

inline Rect rect_from_raw(f32 x0, f32 y0, f32 x1, f32 y1) { return (Rect) { v2(x0, y0), v2(x1, y1) }; }
inline Rect rect_from_pos(Vector2 pos, Vector2 size) {
    Vector2 half_size = v2_div(size, v2s(2.f));
    Vector2 min = v2_sub(pos, half_size);
    Vector2 max = v2_add(pos, half_size);
    return (Rect) { min, max };
}
inline Rect rect_from_points(Vector2 p0, Vector2 p1) { 
    return (Rect) { 
        v2(MIN(p0.x, p1.x), MIN(p0.y, p1.y)), 
        v2(MAX(p0.x, p1.x), MAX(p0.y, p1.y)),
    };
}
inline Vector2 rect_size(Rect a) { return v2_sub(a.max, a.min); }

b32 rect_overlaps_rect(Rect a, Rect b, Rect* overlap);
b32 rect_overlaps_point(Rect a, Vector2 b);
inline Rect move_rect(Rect a, Vector2 offset) { return (Rect) { v2_add(a.min, offset), v2_add(a.max, offset) }; }

b32 line_intersect_line(Vector2 a1, Vector2 a2, Vector2 b1, Vector2 b2, Vector2* intersection);

typedef struct Rect_Intersect_Result {
    Vector2 intersection;
    Vector2 normal;
} Rect_Intersect_Result;

b32 line_intersect_rect(Vector2 a1, Vector2 a2, Rect b, Rect_Intersect_Result* result);
b32 rect_sweep_rect(Vector2 a1, Vector2 a2, Vector2 size, Rect b, Rect_Intersect_Result* result);

typedef struct Random_Seed {
    int initial_seed;
    int seed;
} Random_Seed;

inline Random_Seed init_seed(int seed) { return (Random_Seed) { seed, seed }; }

void iterate_seed(Random_Seed* seed);
f32 random_f32(Random_Seed* seed);
f32 random_f32_in_range(Random_Seed* seed, f32 min, f32 max);

f32 perlin_get_2d(Random_Seed seed, f32 x, f32 y, f32 freq, int depth);

#endif /* MATH_H */