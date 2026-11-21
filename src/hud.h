#pragma once
// 屏幕叠加文字（对应实验六里用 glutBitmapCharacter 显示信息的做法）。
// 注意：GLUT 的位图字体只支持 ASCII，所以 HUD 文案用英文。
#include <string>

// 在当前视口上开启 2D 叠加层（内部保存并恢复投影/模型视图矩阵，关闭光照与深度测试）
void hudBegin(int viewportW, int viewportH);
void hudEnd();

void hudColor(float r, float g, float b);
void hudText(float x, float y, const std::string& s);
void hudTitle(float x, float y, const std::string& s);
void hudTextf(float x, float y, const char* fmt, ...);

// 一行文字的实际像素宽度（用 glutBitmapWidth 逐字符累加）。
// 面板尺寸要靠它实测，写死宽度会把长行截断。
float hudTextWidth(const std::string& s);

// 画一个半透明底板，给帮助面板用
void hudPanel(float x, float y, float w, float h, float alpha);

constexpr float kHudLineH = 16.0f;    // 行高（配合 GLUT_BITMAP_9_BY_15）
