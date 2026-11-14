#pragma once
// 程序化森林 v2：L-system 分形树 + 花草（2026 加分项：树木花草用分形算法实现）。
//
// 全部几何烘焙进**一个**带顶点色的 MeshData（整片森林一次 glDrawElements），
// 固定种子 xorshift 随机数保证同一种子生成同一片森林，录像可复现。
//
// 树的生成 = L-system：字符串重写（F → 展开式）迭代 depth 代，
// 再用 3D 海龟（前进/偏航/俯仰/翻滚/入栈出栈）解释成锥形枝干，
// 末端小枝挂叶团。迭代深度与分支角可运行期调整（菜单/按键）并即时重建森林。
#include "math3d.h"
#include "obj_loader.h"

#include <string>
#include <vector>

// ---- 基本形状（动物等其它程序化网格也复用）----
void pushTri(MeshData& m, const Vec3& a, const Vec3& b, const Vec3& c, const float col[3]);
void addCylinderSide(MeshData& m, float y0, float y1, float r0, float r1,
                     const float col[3], int sides);               // 竖直圆柱（可锥形）
void addCone(MeshData& m, float y0, float y1, float r, const float col[3], int sides);
void addEllipsoid(MeshData& m, const Vec3& c, float rx, float ry, float rz,
                  const float col[3], int slices, int stacks);
// 把 part 的顶点经 4x4 矩阵（列主序）变换后追加进 out —— 拼装多部件模型用
void appendTransformed(MeshData& out, const MeshData& part, const float m[16]);

// 把整片森林（地面 + 分形树 + 花草）写进 out。out 会被清空。
//   path / clearance：漫步路线两侧留空地；seed 固定可复现
void buildForest(MeshData& out,
                 float halfSize,
                 int   treeCount,
                 const std::vector<Vec3>& path,
                 float clearance,
                 unsigned seed);

// ---- 运行期分形参数（调整后调 sceneRebuildForest 重建）----
void forestSetDetail(int bump);        // -1/0/+1：迭代深度 减一/基准/加一
int  forestDetail();
void forestAdjustAngle(float scale);   // 分支角整体缩放，1.0 为基准
const char* forestParamsText();        // HUD/日志，如 "depth 3+1, angle x1.15"
