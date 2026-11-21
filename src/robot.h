#pragma once
// 机器人「小宇」：长方体层级建模 + 关节动画（2026 实验主角）。
//
// 结构与 cgexp 课程 lab1 同源，迁移到本工程的 Scene/Object 体系：
//   - 7 个基本部件（躯干/头/双臂/双腿）+ 天线和双眼两个细节部件；
//   - 每个部件是 Scene 里的一个 Object，本模块每帧把「根 · 局部关节链」的
//     层级矩阵拆成 pos + xform 写回 —— 经典管线与 GLSL 路径都按
//     T(pos)·xform·S(size) 组合模型矩阵，拆开喂给 Object 后两条路径自动一致，
//     阴影 pass 也无需特殊处理；
//   - 名为 "Robot" 的根物体（不绘制）接收用户的整体平移/旋转/缩放（实验五/六），
//     Tab 选中它之后 t/r/y 或左键拖动即可整体变换机器人；
//   - 四肢的关节链是标准的 T(关节)·R(关节角)·T(杆件偏移)·S(尺寸) 三明治结构，
//     几何建成就已是最终尺寸，所以链里省掉 S。
//
// 姿势系统：stand / wave / T-pose / walk-freeze 四个定格 + 时间驱动的走路循环，
// 关节角之间做指数平滑插值，切换不跳变。所有目标都是时间 t 的纯函数，
// 离线录制（帧号/fps 时间轴）与实时预览行为一致。
#include <string>

class Scene;
struct BezierPath;

enum class RobotPose { Stand = 0, Wave, TPose, WalkFreeze, Count };

// 在 Forest 场景里创建机器人（appSetup 调用，Studio 场景不建）。
void  robotCreate(Scene& scene);
bool  robotImportCharacter(Scene& scene, const std::string& path, bool slim, std::string& error);
bool  robotToggleCharacter(Scene& scene);
const std::string& robotCharacterSource();

// 每帧推进关节并写回各部件。t 用「录制=帧号/fps，预览=真实时钟」的时间轴。
void  robotUpdate(Scene& scene, float t);

// 姿势与走路开关（键盘 n / b 与菜单共用）。snap=true 时立即到位，不播过渡
// （--pose 出图用：headless 只渲染一帧，没时间做插值）。
void  robotSetPose(RobotPose pose, bool snap = false);
void  robotNextPose();
RobotPose robotPose();
const char* robotPoseName(RobotPose pose);
bool  robotParsePose(const std::string& s, RobotPose& out);   // stand|wave|tpose|walk

void  robotToggleWalk();
bool  robotWalking();

// ---- W3-1：三次贝塞尔闭曲线漫步（2026 加分项）----
// 路径是在林间空地里绕行的一圈；行走时按「弧长」参数化，速度恒定。
// 开/关路径散步；开启期间 robotUpdate 会覆写根物体的位置和朝向。
void  robotTogglePath();
void  robotSetPathWalk(bool on);
bool  robotOnPath();

// 曲线/控制点可视化（B 键）。路径网格在 robotCreate 时生成，默认隐藏。
void  robotTogglePathGuide();
void  robotSetPathGuide(bool on);
bool  robotPathGuide();

// W3-2 追赶演示：由 app 每帧把「鹿身后的位置」喂给机器人。
// 优先级高于贝塞尔路径；清除后恢复原地行走。
void  robotSetChasePose(float x, float z, float yawDeg);
void  robotClearChasePose();
bool  robotChasing();

// 供出图/自检读取机器人行走的曲线（对象内部静态数据）。
const BezierPath& robotPath();
float robotPathSpeed();          // 沿路径的恒定速度（世界单位/秒）

// 测试/出图钩子：锁定机器人的时间轴（--time）。负值解除锁定。
// 锁定时关节直接取该时刻的目标值（不做平滑），用于渲染行走循环的任意相位定格。
void  robotSetTimeOverride(float t);
float robotTimeOverride();       // <0 表示未锁定（供 app 把同一时间轴给动物）

// 按部件名查它在场景里的当前世界坐标（--selftest 断言层级矩阵用）。
// 找不到返回 false。
bool  robotPartPosition(const Scene& scene, const char* partName, float out[3]);

// 场景里有没有机器人（--scene studio 时为 false，输入侧据此忽略 n/b 键）。
bool  robotAlive();
