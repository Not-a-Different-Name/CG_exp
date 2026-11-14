#pragma once
// 三维数学小工具（header-only）。
// 矩阵一律使用 OpenGL 的列主序 GLfloat[16]：m[col*4 + row]。
#include <cmath>

struct Vec3
{
    float x, y, z;
    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float ax, float ay, float az) : x(ax), y(ay), z(az) {}
};

inline Vec3 v3(float x, float y, float z) { return Vec3{ x, y, z }; }
inline Vec3 operator+(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 operator-(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 operator*(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }

inline float v3dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float v3len(Vec3 a) { return std::sqrt(v3dot(a, a)); }

inline Vec3 v3cross(Vec3 a, Vec3 b)
{
    return v3(a.y * b.z - a.z * b.y,
              a.z * b.x - a.x * b.z,
              a.x * b.y - a.y * b.x);
}

inline Vec3 v3norm(Vec3 a)
{
    const float len = v3len(a);
    if (len < 1e-8f) return v3(0.0f, 0.0f, 0.0f);
    return a * (1.0f / len);
}

// ---------------------------------------------------------------------------
// 四元数旋转（实验七）
// 把点 (px,py,pz) 绕过原点的单位轴 (ux,uy,uz) 旋转 theta 弧度，结果直接写回引用。
// 与指导书 quatRotate 的引用传参写法等价，这里用显式的临时副本保证旧坐标参与全部计算。
// ---------------------------------------------------------------------------
inline void quatRotate(float theta, float ux, float uy, float uz,
                       float& px, float& py, float& pz)
{
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    const float t = 1.0f - c;

    const float inx = px, iny = py, inz = pz;

    px = (ux * ux * t + c)       * inx + (ux * uy * t - uz * s) * iny + (ux * uz * t + uy * s) * inz;
    py = (uy * ux * t + uz * s)  * inx + (uy * uy * t + c)      * iny + (uy * uz * t - ux * s) * inz;
    pz = (uz * ux * t - uy * s)  * inx + (uz * uy * t + ux * s) * iny + (uz * uz * t + c)      * inz;
}

// ---------------------------------------------------------------------------
// 4x4 矩阵（列主序）
// ---------------------------------------------------------------------------
inline void mat4Identity(float m[16])
{
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// out = a * b   （列主序：m[col*4+row]）
inline void mat4Multiply(float out[16], const float a[16], const float b[16])
{
    float tmp[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
                sum += a[k * 4 + row] * b[col * 4 + k];
            tmp[col * 4 + row] = sum;
        }
    }
    for (int i = 0; i < 16; ++i) out[i] = tmp[i];
}

// 变换一个点（含平移分量）
inline Vec3 mat4MulPoint(const float m[16], Vec3 p)
{
    return v3(m[0] * p.x + m[4] * p.y + m[8]  * p.z + m[12],
              m[1] * p.x + m[5] * p.y + m[9]  * p.z + m[13],
              m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]);
}

// 变换一个方向向量（忽略平移分量）
inline Vec3 mat4MulDir(const float m[16], Vec3 p)
{
    return v3(m[0] * p.x + m[4] * p.y + m[8]  * p.z,
              m[1] * p.x + m[5] * p.y + m[9]  * p.z,
              m[2] * p.x + m[6] * p.y + m[10] * p.z);
}

// 绕任意轴 (ux,uy,uz) 旋转 angleDeg 度的矩阵（列主序）。
// 轴会被内部归一化；退化时返回单位阵。
inline void mat4FromAxisAngle(float m[16], float angleDeg, float ux, float uy, float uz)
{
    const float len = std::sqrt(ux * ux + uy * uy + uz * uz);
    if (len < 1e-8f) { mat4Identity(m); return; }
    ux /= len; uy /= len; uz /= len;

    const float r = angleDeg * 3.14159265358979323846f / 180.0f;
    const float c = std::cos(r), s = std::sin(r), t = 1.0f - c;

    m[0]  = t * ux * ux + c;        // r0c0
    m[1]  = t * ux * uy + s * uz;   // r1c0
    m[2]  = t * ux * uz - s * uy;   // r2c0
    m[3]  = 0.0f;

    m[4]  = t * ux * uy - s * uz;   // r0c1
    m[5]  = t * uy * uy + c;        // r1c1
    m[6]  = t * uy * uz + s * ux;   // r2c1
    m[7]  = 0.0f;

    m[8]  = t * ux * uz + s * uy;   // r0c2
    m[9]  = t * uy * uz - s * ux;   // r1c2
    m[10] = t * uz * uz + c;        // r2c2
    m[11] = 0.0f;

    m[12] = m[13] = m[14] = 0.0f;
    m[15] = 1.0f;
}

// 平移矩阵
inline void mat4FromTranslation(float m[16], float tx, float ty, float tz)
{
    mat4Identity(m);
    m[12] = tx; m[13] = ty; m[14] = tz;
}

// 缩放矩阵
inline void mat4FromScale(float m[16], float sx, float sy, float sz)
{
    mat4Identity(m);
    m[0] = sx; m[5] = sy; m[10] = sz;
}
