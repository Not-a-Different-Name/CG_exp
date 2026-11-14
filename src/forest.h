#pragma once
// 程序化森林：地面 + 树，全部烘焙进**一个**带顶点色的 MeshData。
//
// 为什么烘焙成一个而不是每棵树一个物体：
//   - 整片森林一次 glDrawElements 画完，不用几十个 draw call
//   - 树是纯背景，不需要被选中 / 单独变换，合并没有任何损失
//   - 顶点色让树干和树叶在同一个网格里用不同颜色，不必拆成两个材质
//
// 全部几何用固定种子的 xorshift 伪随机数生成 —— 同一个种子必然得到同一片森林，
// 录像才能复现（rand() 的实现各编译器不同，不能用）。
#include "math3d.h"
#include "obj_loader.h"

#include <vector>

// 把整片森林（地面 + 树）写进 out。out 会被清空。
//   halfSize   林地半边长（世界单位）
//   treeCount  树的数量
//   path       兔子会经过的路径采样点（XZ 平面），该路径两侧 clearance 内不种树，
//              否则兔子会从树干里穿过去
//   clearance  路径两侧保留的空地半径
//   seed       随机种子
void buildForest(MeshData& out,
                 float halfSize,
                 int   treeCount,
                 const std::vector<Vec3>& path,
                 float clearance,
                 unsigned seed);
