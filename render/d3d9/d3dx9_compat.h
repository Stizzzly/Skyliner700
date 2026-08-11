#ifndef SKYLINER_D3DX9_COMPAT_H
#define SKYLINER_D3DX9_COMPAT_H

#include <d3d9.h>
#include <math.h>

/* Minimal D3DX9 matrix subset.  D3DX9 is deprecated and not supplied by
 * current MinGW packages, so the project keeps the few functions it needs
 * locally while continuing to use native Direct3D 9 matrix types. */
typedef D3DMATRIX D3DXMATRIX;

typedef struct D3DXVECTOR3 {
    float x;
    float y;
    float z;
} D3DXVECTOR3;

static inline float D3DXToRadian(float degrees) {
    return degrees * (3.14159265358979323846f / 180.0f);
}

static inline D3DXMATRIX *D3DXMatrixMultiply(D3DXMATRIX *out, const D3DXMATRIX *a, const D3DXMATRIX *b) {
    D3DXMATRIX result;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            result.m[row][column] = a->m[row][0] * b->m[0][column]
                                  + a->m[row][1] * b->m[1][column]
                                  + a->m[row][2] * b->m[2][column]
                                  + a->m[row][3] * b->m[3][column];
        }
    }
    *out = result;
    return out;
}

static inline D3DXMATRIX *D3DXMatrixRotationY(D3DXMATRIX *out, float angle) {
    const float c = cosf(angle);
    const float s = sinf(angle);
    *out = (D3DXMATRIX){ .m = {
        { c, 0.0f, -s, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { s, 0.0f, c, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f }
    }};
    return out;
}

static inline D3DXMATRIX *D3DXMatrixTranslation(D3DXMATRIX *out, float x, float y, float z) {
    *out = (D3DXMATRIX){ .m = {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { x, y, z, 1.0f }
    }};
    return out;
}

static inline D3DXMATRIX *D3DXMatrixLookAtLH(D3DXMATRIX *out, const D3DXVECTOR3 *eye, const D3DXVECTOR3 *at, const D3DXVECTOR3 *up) {
    float zx = at->x - eye->x, zy = at->y - eye->y, zz = at->z - eye->z;
    float length = sqrtf(zx * zx + zy * zy + zz * zz);
    zx /= length; zy /= length; zz /= length;
    float xx = up->y * zz - up->z * zy;
    float xy = up->z * zx - up->x * zz;
    float xz = up->x * zy - up->y * zx;
    length = sqrtf(xx * xx + xy * xy + xz * xz);
    xx /= length; xy /= length; xz /= length;
    const float yx = zy * xz - zz * xy;
    const float yy = zz * xx - zx * xz;
    const float yz = zx * xy - zy * xx;
    *out = (D3DXMATRIX){ .m = {
        { xx, yx, zx, 0.0f }, { xy, yy, zy, 0.0f }, { xz, yz, zz, 0.0f },
        { -(xx * eye->x + xy * eye->y + xz * eye->z),
          -(yx * eye->x + yy * eye->y + yz * eye->z),
          -(zx * eye->x + zy * eye->y + zz * eye->z), 1.0f }
    }};
    return out;
}

static inline D3DXMATRIX *D3DXMatrixPerspectiveFovLH(D3DXMATRIX *out, float fovY, float aspect, float nearZ, float farZ) {
    const float yScale = 1.0f / tanf(fovY * 0.5f);
    const float xScale = yScale / aspect;
    *out = (D3DXMATRIX){ .m = {
        { xScale, 0.0f, 0.0f, 0.0f }, { 0.0f, yScale, 0.0f, 0.0f },
        { 0.0f, 0.0f, farZ / (farZ - nearZ), 1.0f },
        { 0.0f, 0.0f, -nearZ * farZ / (farZ - nearZ), 0.0f }
    }};
    return out;
}

#endif
