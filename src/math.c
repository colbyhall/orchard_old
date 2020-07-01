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