#include "math.h"

Quaternion quat_from_axis_angle(Vector3 axis, f32 angle_rad) {
    angle_rad *= 0.5f;
    const f32 s = sinf(angle_rad);
    const f32 c = cosf(angle_rad);
    return (Quaternion) { s * axis.x, s * axis.y, s * axis.z, c };
}

Quaternion quat_from_euler_angles(f32 roll, f32 pitch, f32 yaw) {
    const f32 cr = cosf(roll * 0.5f);
    const f32 sr = sinf(roll * 0.5f);

    const f32 cp = cosf(pitch * 0.5f);
    const f32 sp = sinf(pitch * 0.5f);

    const f32 cy = cosf(yaw * 0.5f);
    const f32 sy = sinf(yaw * 0.5f);

    return (Quaternion) { 
        sr * cp * cy - cr * sp * sy,
        cr * sp * cy + sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
        cr * cp * cy + sr * sp * sy,
    };
}   

Vector3 rotate_vector_with_quat(Vector3 a, Quaternion b) {
    const Vector3 t = v3_mul(v3_cross(b.xyz, a), v3s(2.f));
    return v3_add(v3_add(a, v3_mul(t, v3s(b.w))), v3_cross(b.xyz, t));
}

Matrix4 m4_mul(Matrix4 a, Matrix4 b) {
    Matrix4 result;
    for (u32 y = 0; y < 4; ++y) {
        for (u32 x = 0; x < 4; ++x) {
            f32 sum = 0.f;
            for (u32 o = 0; o < 4; ++o) {
                sum += a.e[x + o * 4] * b.e[o + y * 4];
            }
            result.e[x + y * 4] = sum;
        }
    }
    return result;
}

Matrix4 m4_identity(void) {
    Matrix4 result = (Matrix4) { 0 };
    result.col_row[0][0] = 1.f;
    result.col_row[1][1] = 1.f;
    result.col_row[2][2] = 1.f;
    result.col_row[3][3] = 1.f;
    return result;
}

Matrix4 m4_ortho(f32 size, f32 aspect_ratio, f32 far, f32 near) {
    const f32 right = size * aspect_ratio;
    const f32 left = -right;

    const f32 top = size;
    const f32 bottom = -top;

    Matrix4 result = m4_identity();
    result.e[0 + 0 * 4] = 2.f / (right - left);
    result.e[1 + 1 * 4] = 2.f / (top - bottom);
    result.e[2 + 2 * 4] = -2.f / (far - near);

    result.e[0 + 3 * 4] = -((right + left) / (right - left));
    result.e[1 + 3 * 4] = -((top + bottom) / (top - bottom));
    result.e[2 + 3 * 4] = -((far + near) / (far - near));

    return result;
}

Matrix4 m4_persp(f32 fov, f32 asepct_ratio, f32 far, f32 near) {
    Matrix4 result = m4_identity();

    const f32 cotangent = 1.f / tanf(fov * (PI / 360.f));
    result.e[0 + 0 * 4] = cotangent / asepct_ratio;
    result.e[1 + 1 * 4] = cotangent;
    result.e[3 + 2 * 4] = -1.f;
    result.e[2 + 2 * 4] = (near + far) / (near - far);
    result.e[2 + 3 * 4] = (2.f * near * far) / (near - far);
    result.e[3 + 3 * 4] = 0.f;
    
    return result;
}

Matrix4 m4_translate(Vector3 translation) {
    Matrix4 result = m4_identity();
    result.e[0 + 3 * 4] = translation.x;
    result.e[1 + 3 * 4] = translation.y;
    result.e[2 + 3 * 4] = translation.z;
    return result;
}

Matrix4 m4_rotate(Quaternion rot) {
    Matrix4 result = m4_identity();

    rot = quat_norm(rot);

    const f32 xx = rot.x * rot.x;
    const f32 xy = rot.x * rot.y;
    const f32 xz = rot.x * rot.z;
    const f32 xw = rot.x * rot.w;

    const f32 yy = rot.y * rot.y;
    const f32 yz = rot.y * rot.z;
    const f32 yw = rot.y * rot.w;

    const f32 zz = rot.z * rot.z;
    const f32 zw = rot.z * rot.w;

    result.col_row[0][0] = 1.f - 2.f * (yy + zz);
    result.col_row[1][0] = 2.f * (xy - zw);
    result.col_row[2][0] = 2.f * (xz + yw);

    result.col_row[0][1] = 2.f * (xy + zw);
    result.col_row[1][1] = 1.f - 2.f * (xx + zz);
    result.col_row[2][1] = 2.f * (yz - xw);

    result.col_row[0][2] = 2.f * (xz - yw);
    result.col_row[1][2] = 2.f * (yz + xw);
    result.col_row[2][2] = 1.f - 2.f * (xx + yy);

    return result;
}

Matrix4 m4_scale(Vector3 scale) {
    Matrix4 result = m4_identity();
    result.col_row[0][0] = scale.x;
    result.col_row[1][1] = scale.y;
    result.col_row[2][2] = scale.z;
    return result;
}

b32 rect_overlaps_rect(Rect a, Rect b, Rect* overlap) {
    if (overlap) {
        const f32 min_x = (a.min.x < b.min.x) ? b.min.x : a.min.x;
        const f32 min_y = (a.min.y < b.min.y) ? b.min.y : a.min.y;
        const f32 max_x = (a.max.x > b.max.x) ? b.max.x : a.max.x;
        const f32 max_y = (a.max.y > b.max.y) ? b.max.y : a.max.y;

        overlap->min = v2(min_x, min_y);
        overlap->max = v2(max_x, max_y);
    }

    return !(b.min.x > a.max.x || b.max.x < a.min.x || b.max.y < a.min.y || b.min.y > a.max.y);
}

b32 rect_overlaps_point(Rect a, Vector2 b) {
    return !(b.x < a.min.x || b.x > a.max.x || b.y < a.min.y || b.y > a.max.y);
}

b32 line_intersect_line(Vector2 a1, Vector2 a2, Vector2 b1, Vector2 b2, Vector2* intersection) {
    const Vector2 a = v2_sub(a2, a1);
    const Vector2 b = v2_sub(b2, b1);

    const f32 ab_cross = v2_cross(a, b);
    if (ab_cross == 0.f) return false;

    const Vector2 c = v2_sub(b1, a1);
    const f32 t = v2_cross(c, b) / ab_cross;
    if (t < 0.f || t > 1.f) return false;

    const f32 u = v2_cross(c, a) / ab_cross;
    if (u < 0.f || u > 1.f) return false;

    if (intersection) *intersection = v2_add(a1, v2_mul(a, v2s(t)));

    return true;
}

b32 line_intersect_rect(Vector2 a1, Vector2 a2, Rect b, Rect_Intersect_Result* result) {
    const Vector2 a     = v2_sub(a2, a1);
    const Vector2 dir   = v2_norm(a);
    const f32 dot_up    = v2_dot(v2_up, dir);
    const f32 dot_right = v2_dot(v2_right, dir);

    if (dot_right > 0.5f) {
        const Vector2 b1 = b.min;
        const Vector2 b2 = v2(b.min.x, b.max.y);
        if (line_intersect_line(a1, a2, b1, b2, &result->intersection)) {
            const Vector2 b_dir = v2_norm(v2_sub(b2, b1)); // We technically know this already
            result->normal = v2_perp(b_dir);
            return true;
        }
    }

    if (dot_right < 0.5f) {
        const Vector2 b1 = b.max;
        const Vector2 b2 = v2(b.max.x, b.min.y);
        if (line_intersect_line(a1, a2, b1, b2, &result->intersection)) {
            const Vector2 b_dir = v2_norm(v2_sub(b2, b1)); // We technically know this already
            result->normal = v2_perp(b_dir);
            return true;
        }
    }

    if (dot_up > 0.5f) {
        const Vector2 b1 = b.min;
        const Vector2 b2 = v2(b.max.x, b.min.y);
        if (line_intersect_line(a1, a2, b1, b2, &result->intersection)) {
            const Vector2 b_dir = v2_norm(v2_sub(b2, b1)); // We technically know this already
            result->normal = v2_perp(b_dir);
            return true;
        }
    }

    if (dot_up < 0.5f) {
        const Vector2 b1 = b.max;
        const Vector2 b2 = v2(b.min.x, b.max.y);
        if (line_intersect_line(a1, a2, b1, b2, &result->intersection)) {
            const Vector2 b_dir = v2_norm(v2_sub(b2, b1)); // We technically know this already
            result->normal = v2_perp(b_dir);
            return true;
        }
    }

    return false;
}

b32 rect_sweep_rect(Vector2 a1, Vector2 a2, Vector2 size, Rect b, Rect_Intersect_Result* result) {
    const Vector2 half_size = v2_div(size, v2s(2.f));
    const Rect large_b = { v2_sub(b.min, half_size), v2_add(b.max, half_size) };

    if (line_intersect_rect(a1, a2, large_b, result)) return true;

    const Vector2 b_pos = v2((b.max.x - b.min.x) + b.min.x, (b.max.y - b.min.y) + b.min.y);

    Rect overlap;
    const Rect at_end = rect_from_pos(a2, size);
    if (rect_overlaps_rect(at_end, b, &overlap)) {
        const Vector2 overlap_size = rect_size(overlap);
        Vector2 intersection = a2;
        Vector2 normal = v2z();

        if (overlap_size.x > overlap_size.y) {
            const f32 flip = SIGN(a2.y - b_pos.y);
            intersection.y += overlap_size.y * flip;
            normal.y = flip;
        } else {
            const f32 flip = SIGN(a2.x - b_pos.x);
            intersection.x += overlap_size.x * flip;
            normal.x = flip;
        }

        *result = (Rect_Intersect_Result) { intersection, normal };

        return true;
    }

    return false;
}

// Found this here https://gist.github.com/nowl/828013
static const u8 perline_hash[] = {
    208, 34, 231, 213, 32, 248, 233, 56, 161, 78, 24, 140, 71, 48, 140, 254, 245, 255, 247, 247, 40, 
    185, 248, 251, 245, 28, 124, 204, 204, 76, 36, 1, 107, 28, 234, 163, 202, 224, 245, 128, 167, 204, 
    9, 92, 217, 54, 239, 174, 173, 102, 193, 189, 190, 121, 100, 108, 167, 44, 43, 77, 180, 204, 8, 81, 
    70, 223, 11, 38, 24, 254, 210, 210, 177, 32, 81, 195, 243, 125, 8, 169, 112, 32, 97, 53, 195, 13, 
    203, 9, 47, 104, 125, 117, 114, 124, 165, 203, 181, 235, 193, 206, 70, 180, 174, 0, 167, 181, 41, 
    164, 30, 116, 127, 198, 245, 146, 87, 224, 149, 206, 57, 4, 192, 210, 65, 210, 129, 240, 178, 105, 
    228, 108, 245, 148, 140, 40, 35, 195, 38, 58, 65, 207, 215, 253, 65, 85, 208, 76, 62, 3, 237, 55, 89, 
    232, 50, 217, 64, 244, 157, 199, 121, 252, 90, 17, 212, 203, 149, 152, 140, 187, 234, 177, 73, 174, 
    193, 100, 192, 143, 97, 53, 145, 135, 19, 103, 13, 90, 135, 151, 199, 91, 239, 247, 33, 39, 145, 
    101, 120, 99, 3, 186, 86, 99, 41, 237, 203, 111, 79, 220, 135, 158, 42, 30, 154, 120, 67, 87, 167, 
    135, 176, 183, 191, 253, 115, 184, 21, 233, 58, 129, 233, 142, 39, 128, 211, 118, 137, 139, 255, 
    114, 20, 218, 113, 154, 27, 127, 246, 250, 1, 8, 198, 250, 209, 92, 222, 173, 21, 88, 102, 219
};

static int noise2(Random_Seed seed, int x, int y) {
    int yindex = (y + seed.seed) % 256;
    if (yindex < 0) yindex += 256;

    int xindex = (perline_hash[yindex] + x) % 256;
    if (xindex < 0) xindex += 256;

    const int result = perline_hash[xindex];
    return result;
}

static f32 perlin_lerpf(f32 a, f32 b, f32 t) { 
    return lerpf(a, b, t * t * (3.f - 2.f * t)); 
}

static f32 noise_2d(Random_Seed seed, f32 x, f32 y) {
    const int x_int = (int)floorf(x);
    const int y_int = (int)floorf(y);

    const f32 x_frac = x - x_int;
    const f32 y_frac = y - y_int;

    const int s = noise2(seed, x_int, y_int);
    const int t = noise2(seed, x_int + 1, y_int);

    const int u = noise2(seed, x_int, y_int + 1);
    const int v = noise2(seed, x_int + 1, y_int + 1);

    const f32 low  = perlin_lerpf((f32)s, (f32)t, x_frac);
    const f32 high = perlin_lerpf((f32)u, (f32)v, x_frac);

    return perlin_lerpf(low, high, y_frac);
}

f32 perlin_get_2d(Random_Seed seed, f32 x, f32 y, f32 freq, int depth) {
    f32 xa = x * freq;
    f32 ya = y * freq;
    f32 amp = 1.f;
    f32 fin = 0.f;
    f32 div = 0.f;

    for(int i = 0; i < depth; ++i) {
        div += 256.f * amp;
        fin += noise_2d(seed, xa, ya) * amp;
        amp /= 2.f;
        xa *= 2.f;
        ya *= 2.f;
    }

    return fin / div;
}