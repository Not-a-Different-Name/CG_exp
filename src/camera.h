#pragma once
// 观察变换相机（实验七）。
// eye/target/up 三个向量就是 gluLookAt 的参数；绕参考点的旋转用四元数 quatRotate，
// 与指导书里 rotateCamera 分支的算法一致。
#include "math3d.h"

struct Camera
{
    Vec3  eye    { 0.0f, 0.0f, 3.0f };
    Vec3  target { 0.0f, 0.0f, 0.0f };
    Vec3  up     { 0.0f, 1.0f, 0.0f };
    float fovY   = 45.0f;      // 透视投影的垂直视场角（度）

    Vec3  forward()  const { return v3norm(target - eye); }
    Vec3  right()    const { return v3norm(v3cross(forward(), up)); }
    float distance() const { return v3len(target - eye); }

    // 回到默认观察位置（相机放在 +Z 方向上，距离随场景半径自适应）
    void reset(float sceneRadius);

    // 绕参考点旋转：dPitchDeg 为俯仰、dYawDeg 为偏航（单位：度）
    void orbit(float dPitchDeg, float dYawDeg);

    // 相机平面内平移（world 单位）
    void pan(float dxWorld, float dyWorld);

    // 沿视线推拉；factor > 1 拉远，< 1 拉近
    void dolly(float factor);

    // 漫游：沿视线前后 / 沿右向量左右 / 沿世界 Y 升降，amount 单位为当前距离的比例
    void fly(float fwd, float strafe, float rise);

    // 把当前 eye/target/up 交给 gluLookAt，并处理顶视图的退化情况
    void applyLookAt() const;

    // 切到某个标准视图（正平行投影三视图）
    void setPreset(int presetIndex, float sceneRadius);
};
