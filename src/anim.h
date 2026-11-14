#pragma once
// 录制用的脚本动画。
//
// 关键约束：applyAnimation 必须是**时间的纯函数** —— 同一个 t 永远得到同一个场景/相机状态。
// 录像时渲染速度是不确定的（这台机器快、换台机器慢），如果动画按真实时间推进，
// 出来的视频就会忽快忽慢。用「帧序号 / fps」当时间轴，离线渲染多少帧都由参数决定，
// 视频必然匀速且可复现。
#include "math3d.h"

#include <string>

enum class AnimKind
{
    None,          // 不动（只用于单帧截图）
    Turntable,     // 模型绕 Y 轴自转，相机不动
    Orbit,         // 相机绕模型环绕，模型不动（演示观察变换 / 实验七）
    Projections,   // 依次切换四种投影模式（实验七 + 斜平行投影加分项）
    Transforms,    // 立方体依次做平移 / 旋转 / 缩放（实验五几何变换加分项）
    Roam           // 兔子在森林里蹦跳漫步（程序化步态 + 追尾相机）
};

// 漫步路径（XZ 平面上的平滑曲线）。森林生成和漫游动画共用这一个函数，
// 保证「种树时避开的路线」正是「兔子真正走过的路线」。
Vec3 roamPath(float t);

bool parseAnimKind(const std::string& s, AnimKind& out);
const char* animKindName(AnimKind k);

// 把场景/相机设置到该动画在 t 秒时的状态。totalSeconds 是动画总时长。
void applyAnimation(AnimKind kind, float t, float totalSeconds);
