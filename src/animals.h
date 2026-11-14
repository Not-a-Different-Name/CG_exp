#pragma once
// 森林小动物（2026 加分项：增加小动物等三维对象）。
//
// 一只鹿和一只狐狸，几何全部程序化拼装（椭球/圆柱/锥，与分形树同一套形状库），
// 风格与低多边形森林一致。每只动物是场景里的一个 Object，面朝 +X、四脚落地。
//
// 行为是时间的纯函数（录制可复现）：各自沿圆形路线漫步，
// 「走 11 秒 → 低头吃草 4 秒」循环；吃草时俯身贴地、位置冻结。
// 兔子不归本模块管 —— 它仍是 roam 漫游动画的主角。
class Scene;

bool  animalsCreate(Scene& scene);            // 只在 Forest 场景调用
void  animalsUpdate(Scene& scene, float t);
bool  animalsAlive();
const char* animalsStatusText();              // HUD/日志：各动物当前状态

// 按物体名取当前世界坐标（--selftest 用）
bool  animalPartPosition(const Scene& scene, const char* name, float out[3]);
