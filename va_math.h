#ifndef VA_3D_H
#define VA_3D_H

#include <stdint.h>

typedef int32_t va_fp16;

static inline int32_t va_clamp_i32(int32_t min, int32_t max, int32_t value) {
    return (value < min ? min : (value > max ? max : value));
}

va_fp16 va_sin(int32_t angle);
va_fp16 va_cos(int32_t angle);

struct va_point2_fp16 {
    va_fp16 x, y;
};

struct va_point3_fp16 {
    va_fp16 x, y, z;
};

struct va_matrix_fp16 {
    va_fp16 m00, m10, m20, m30;
    va_fp16 m01, m11, m21, m31;
    va_fp16 m02, m12, m22, m32;
};

void va_matrix_set_identity(
    struct va_matrix_fp16 * __restrict mtx);

void va_matrix_translate(
    struct va_matrix_fp16 * __restrict mtx,
    va_fp16 x,
    va_fp16 y,
    va_fp16 z);

void va_matrix_rot_x(
    struct va_matrix_fp16 * __restrict mtx,
    int32_t angle);

void va_matrix_rot_y(
    struct va_matrix_fp16 * __restrict mtx,
    int32_t angle);

void va_matrix_rot_z(
    struct va_matrix_fp16 * __restrict mtx,
    int32_t angle);

void va_matrix_transform_points(
    const struct va_matrix_fp16 * __restrict mtx,
    const struct va_point3_fp16 * __restrict src,
    struct va_point3_fp16 * __restrict dst,
    int32_t nbr_points);

void va_matrix_project_points(
    int32_t xy_scale_bits,
    int32_t z_offset,
    const struct va_point3_fp16 * __restrict src,
    struct va_point2_fp16 * __restrict dst,
    int32_t nbr_points);

int32_t va_smoothstep(int32_t edge0, int32_t edge1, int32_t x);

#endif
