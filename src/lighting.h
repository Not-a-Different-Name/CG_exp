#pragma once
// 光照（实验十）。
// 关键顺序：glLightfv(GL_POSITION) 会被「当前 MODELVIEW 矩阵」变换，所以必须在
// gluLookAt 之后、任何物体变换之前调用，否则光会跟着模型一起转、漫游时高光乱飘。
#include <GL/freeglut.h>

// 只设置光照常量（颜色、模型参数、开关）。在 GL 初始化时调用一次。
void initLighting();

// 在每帧的 gluLookAt 之后、物体变换之前调用：把光源位置按当前模型视图变换一次。
// 这样光源固定在世界坐标里，相机移动时明暗关系保持稳定。
void applyLightPositions();

// 顶点法线在交互缩放（非均匀）下会失效，必须重新归一化
inline void enableNormalizeIfLighting() { glEnable(GL_NORMALIZE); }
