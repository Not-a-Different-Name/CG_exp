#pragma once
// 全局模式枚举。单独放一个头，避免 app / input / projection 之间的循环包含。

// 投影模式（实验七 + 斜平行投影加分项）
enum class ProjMode
{
    Perspective,    // 透视投影
    Ortho,          // 正平行投影（正投影）
    Cavalier,       // 斜平行投影 - 斜等测（L = 1.0）
    Cabinet         // 斜平行投影 - 斜二测（L = 0.5）
};

// 标准视图预设（正平行投影三视图 + 前后上下）
enum class ViewPreset { Free, Front, Back, Left, Right, Top, Bottom };

// 几何变换模式（实验六）
enum class TransMode { Translate, Rotate, Scale };

// 绘制模式
enum class RenderMode { Solid, Wireframe, Points };

// 场景类型
enum class SceneKind
{
    Studio,   // 展示台：实验四立方体 + 茶壶 + 球 + 地面 + 网格 + 坐标轴
    Forest    // 森林：程序化树木 + 起伏草地 + 雾，兔子在里面漫步
};
