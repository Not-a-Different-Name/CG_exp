#include "camera.h"
#include "types.h"

#include <GL/freeglut.h>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.0f;

// 视线方向与 up 反平行时 gluLookAt 的基会退化（叉积为零），且它不报错，
// 画面直接空白。这里检测到就换成备用 up。
constexpr float kDegenerateDot = 0.999f;
} // namespace

void Camera::reset(float sceneRadius)
{
    const float d = sceneRadius * 2.8f;
    eye    = v3(0.55f * d, 0.40f * d, 0.75f * d);
    target = v3(0.0f, 0.0f, 0.0f);
    up     = v3(0.0f, 1.0f, 0.0f);
    fovY   = 45.0f;
}

void Camera::orbit(float dPitchDeg, float dYawDeg)
{
    // 与指导书一致：绕「当前视线 × up」得到的局部右向量做俯仰，
    // 绕世界 Y 轴做偏航，只更新视点（参考点不动）。
    const Vec3 r = right();
    if (v3len(r) < 1e-6f) return;          // 已退化，本次忽略

    const float thetaX = -dPitchDeg * kDeg2Rad;
    const float thetaY = -dYawDeg   * kDeg2Rad;

    float px = target.x - eye.x;
    float py = target.y - eye.y;
    float pz = target.z - eye.z;
    quatRotate(thetaX, r.x, r.y, r.z, px, py, pz);
    quatRotate(thetaY, 0.0f, 1.0f, 0.0f, px, py, pz);
    target = v3(eye.x + px, eye.y + py, eye.z + pz);

    float ux = up.x, uy = up.y, uz = up.z;
    quatRotate(thetaX, r.x, r.y, r.z, ux, uy, uz);
    quatRotate(thetaY, 0.0f, 1.0f, 0.0f, ux, uy, uz);
    up = v3norm(v3(ux, uy, uz));
    if (v3len(up) < 1e-6f) up = v3(0.0f, 1.0f, 0.0f);
}

void Camera::pan(float dxWorld, float dyWorld)
{
    const Vec3 r = right();
    const Vec3 u = v3norm(v3cross(r, forward()));
    const Vec3 delta = r * dxWorld + u * dyWorld;
    eye    = eye    + delta;
    target = target + delta;
}

void Camera::dolly(float factor)
{
    if (factor <= 0.0f) return;
    const float dist = distance();
    const float nd   = dist * factor;
    if (nd < 1e-3f) return;
    const Vec3 f = forward();
    target = eye + f * nd;
}

void Camera::fly(float fwd, float strafe, float rise)
{
    const Vec3 f = forward();
    const Vec3 r = right();
    // 平移量与当前距离成比例，远处移动快、近处移动慢
    const float speed = distance() * 0.25f;
    const Vec3 delta = f * (fwd * speed) + r * (strafe * speed) + v3(0.0f, rise * speed, 0.0f);
    eye    = eye    + delta;
    target = target + delta;
}

void Camera::applyLookAt() const
{
    const Vec3 f = forward();
    if (v3len(target - eye) < 1e-6f) return;      // 视点与参考点重合，放弃本次

    const float d = std::fabs(v3dot(f, up));
    if (d > kDegenerateDot)
    {
        // 视线与 up 几乎平行（顶视图/仰视图）：换 (0,0,-1) 作为 up，
        // 得到标准俯视平面视图：屏幕右 = 世界 +X，屏幕上 = 世界 -Z。
        gluLookAt(eye.x, eye.y, eye.z, target.x, target.y, target.z, 0.0f, 0.0f, -1.0f);
        return;
    }
    gluLookAt(eye.x, eye.y, eye.z,
              target.x, target.y, target.z,
              up.x, up.y, up.z);
}

void Camera::setPreset(int presetIndex, float sceneRadius)
{
    const float D = sceneRadius * 2.8f;
    target = v3(0.0f, 0.0f, 0.0f);
    up     = v3(0.0f, 1.0f, 0.0f);

    switch (static_cast<ViewPreset>(presetIndex))
    {
    case ViewPreset::Front:  eye = v3(0.0f,  0.0f,  D); break;   // 正视图（看向 -Z）
    case ViewPreset::Back:   eye = v3(0.0f,  0.0f, -D); break;   // 后视图
    case ViewPreset::Left:   eye = v3(-D,    0.0f,  0.0f); break; // 左视图
    case ViewPreset::Right:  eye = v3( D,    0.0f,  0.0f); break; // 右视图
    case ViewPreset::Top:                                          // 顶视图
        eye = v3(0.0f,  D,     0.0f);
        // up 与视线反平行会退化，指导书这里也用 (0,0,-1)
        up  = v3(0.0f,  0.0f, -1.0f);
        break;
    case ViewPreset::Bottom:                                       // 仰视图
        eye = v3(0.0f, -D,     0.0f);
        up  = v3(0.0f,  0.0f,  1.0f);
        break;
    case ViewPreset::Free:
    default:
        reset(sceneRadius);
        break;
    }
}
