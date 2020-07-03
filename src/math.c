#include "math.h"

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
    Matrix4 result = (Matrix4) { 0 };

    const f32 cotangent = 1.f / tanf(fov * (PI / 360.f));
    result.e[0 + 0 * 4] = cotangent / asepct_ratio;
    result.e[1 + 1 * 4] = cotangent;
    result.e[3 + 2 * 4] = -1.f;
    result.e[2 + 2 * 4] = (near + far) / (near - far);
    result.e[2 + 3 * 4] = (2.f * near * far) / (near - far);
    return result;
}

Matrix4 m4_translate(Vector3 translation) {
    Matrix4 result = m4_identity();
    result.e[0 + 3 * 4] = translation.x;
    result.e[1 + 3 * 4] = translation.y;
    result.e[2 + 3 * 4] = translation.z;
    return result;
}


/*
Matrix4 m4_rotate(Vector3 axis, f32 angle) {
    Matrix4 result = m4_identity();
    const f32 r = angle * TO_RAD;
    const f32 c = cosf(r);
    const f32 s = sinf(r);
    const f32 omc = 1.f - c;

    const f32 x = axis.x;
    const f32 y = axis.y;
    const f32 z = axis.z;

    result.e[0 + 0 * 4] = x * omc + c;
    result.e[1 + 0 * 4] = y * x * omc + z * s;
    result.e[2 + 0 * 4] = x * z * omc - y * s;
    result.e[0 + 1 * 4] = x * y * omc - z * s;
    result.e[1 + 1 * 4] = y * omc + c;
    result.e[2 + 1 * 4] = y * z * omc + x * s;
    result.e[0 + 2 * 4] = x * z * omc + y * s;
    result.e[1 + 2 * 4] = y * z * omc - x * s;
    result.e[2 + 2 * 4] = z * omc + c;

    return result;
}
*/

b32 rect_overlaps_rect(Rect a, Rect b, Rect* overlap) {
    if (overlap) {
        const f32 min_x = (a.min.x < b.min.x) ? b.min.x : a.min.x;
        const f32 min_y = (a.min.y < b.min.y) ? b.min.y : a.min.y;
        const f32 max_x = (a.max.x > b.max.x) ? b.max.x : a.max.x;
        const f32 max_y = (a.max.y > b.max.y) ? b.max.y : a.max.y;

        overlap->min = v2(min_x, min_y);
        overlap->max = v2(max_x, max_y);
    }

    return !(b.min.x > a.max.x || b.max.x < a.min.x || b.max.y < a.min.y || b.min.y > a.max.y)
}

b32 rect_overlaps_point(Rect a, Vector2 b) {
    return !(b.x < a.min.x || b.x > a.max.x || b.y < a.min.y || b.y > a.max.y);
}

b32 line_intersect_line(Vector2 a1, Vector2 a2, Vector2 b1, Vector2 b2, Vector2* intersection) {
    const Vector2 a = v2_sub(a2, a1);
    const Vector2 b = v2_sub(b2, b1);

    const f32 ab_cross  v2_cross(a, b);
    if (ab_cross == 0.f) return false;

    const Vector2 c = b1 - a1;
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
            const Vector2 b_dir = v2_norm(b2 - b1); // We technically know this already
            result->normal = v2_perp(b_dir);
            return true;
        }
    }

    if (dot_right < 0.5f) {
        const Vector2 b1 = b.max;
        const Vector2 b2 = v2(b.max.x, b.min.y);
        if (line_intersect_line(a1, a2, b1, b2, &result->intersection)) {
            const Vector2 b_dir = v2_norm(b2 - b1); // We technically know this already
            result->normal = v2_perp(b_dir);
            return true;
        }
    }

    if (dot_up > 0.5f) {
        const Vector2 b1 = b.min;
        const Vector2 b2 = v2(b.max.x, b.min.y);
        if (line_intersect_line(a1, a2, b1, b2, &result->intersection)) {
            const Vector2 b_dir = v2_norm(b2 - b1); // We technically know this already
            result->normal = v2_perp(b_dir);
            return true;
        }
    }

    if (dot_up < 0.5f) {
        const Vector2 b1 = b.max;
        const Vector2 b2 = v2(b.min.x, b.max.y);
        if (line_intersect_line(a1, a2, b1, b2, &result->intersection)) {
            const Vector2 b_dir = v2_norm(b2 - b1); // We technically know this already
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