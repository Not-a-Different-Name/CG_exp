#pragma once
// 三次贝塞尔闭曲线（2026 加分项：贝塞尔曲线路径）。
//
// 曲线由若干「锚点」给出：相邻锚点之间用一段三次贝塞尔连接，控制柄按
// Catmull-Rom 规则从相邻锚点推出 —— 曲线穿过每个锚点且切线连续（C1）。
// 每段仍用 de Casteljau 递归插值求值，控制点（锚点 + 控制柄）可可视化。
//
// 关键点：贝塞尔参数 u 与弧长不成正比（u 均匀走，速度会忽快忽慢）。
// 这里把整条闭曲线按固定步长采样成折线并累计弧长，位置用「弧长」参数化，
// 机器人/动物就能按弧长匀速移动。
#include "math3d.h"
#include "obj_loader.h"

#include <vector>

struct BezierPath
{
    struct Segment { Vec3 p[4]; };

    std::vector<Segment> segments;     // 每段 4 个控制点（首尾锚点 + 两个控制柄）
    std::vector<Vec3>    anchors;      // 曲线穿过的锚点（可视化用）
    std::vector<Vec3>    samples;      // 采样折线（不含闭合重复点）
    std::vector<float>   cum;          // 累计弧长，cum[i] 对应 samples[i]，末尾是总长
    float                total = 0.0f;

    // 由锚点生成闭合曲线（锚点按顺序首尾相连）。
    void buildClosed(const std::vector<Vec3>& anchorPoints);

    // 按比例 u∈[0,1] 直接求值（de Casteljau），用于生成采样点/画控制多边形。
    Vec3 evalProportional(float u) const;

    // 按弧长 s 求值（s 自动对总长取模）—— 匀速移动用这个。
    Vec3 evalAtDistance(float s) const;
    Vec3 tangentAtDistance(float s) const;

    // 距给定点最近的弧长参数（把对象放到曲线上时用）。
    float nearestDistance(const Vec3& p) const;
};

// 把曲线画成低多边形「胶管 + 控制点标记」的网格，拼进场景后走正常渲染/阴影管线。
//   curveCol 曲线颜色；anchorCol 锚点颜色；handleCol 控制柄颜色
void buildBezierVisual(MeshData& out, const BezierPath& path,
                       const float curveCol[3],
                       const float anchorCol[3],
                       const float handleCol[3]);
