#pragma once
// 投影变换与「窗口变化图形不变形」（实验七 + 要求 3）。
//
// 注意：不要照抄指导书的 glFrustum(-2.5, 2.5, -2.5, 2.5, near, far) ——
// 它在近平面上永远是 1:1，窗口一变宽图形就被压扁，正好踩中要求 3。
#include "types.h"

struct Camera;

// 在 GL_PROJECTION 上设置好投影矩阵。调用方需保证已 glMatrixMode(GL_PROJECTION)。
//   winW/winH   : 当前窗口像素尺寸
//   cam         : 观察相机（透视模式需要其距离与 FOV）
//   sceneRadius : 场景包围球半径，用于自适应取景
//   obliqueAngleDeg : 斜平行投影的斜边与水平方向夹角（屏幕上呈现的角度）
void applyProjection(ProjMode mode, int winW, int winH, const Camera& cam,
                     float sceneRadius, float obliqueAngleDeg = 45.0f);

// 该模式是否使用平行投影（正交 / 斜平行），供 HUD 与菜单显示用
bool isParallelProjection(ProjMode mode);

const char* projModeName(ProjMode mode);
