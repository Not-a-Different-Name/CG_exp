#pragma once
// 键盘 / 鼠标 / 滚轮 / 右键菜单（实验六）。
// 键位安排遵循指导书的提醒：缩放键用 y 而不是 s，避免与 WASD 相机漫游的 s 冲突。

// 菜单项编号。子菜单用不同的区间，父菜单只做分发。
enum MenuId
{
    M_NONE = 0,

    // 变换
    M_TRANS_T = 100, M_TRANS_R, M_TRANS_S,
    M_RESET_OBJ,
    M_SELECT_NEXT,

    // 投影
    M_PROJ_PERSP = 200, M_PROJ_ORTHO, M_PROJ_CAV, M_PROJ_CAB,
    M_OBL_ANGLE_DOWN, M_OBL_ANGLE_UP,
    M_FOV_DOWN, M_FOV_UP,

    // 视图
    M_VIEW_FRONT = 300, M_VIEW_BACK, M_VIEW_LEFT, M_VIEW_RIGHT,
    M_VIEW_TOP, M_VIEW_BOTTOM, M_VIEW_FREE,
    M_RESET_CAM,
    M_TOGGLE_QUAD,

    // 渲染
    M_RENDER_SOLID = 400, M_RENDER_WIRE, M_RENDER_POINT,
    M_TOGGLE_CULL, M_TOGGLE_SHADE, M_TOGGLE_LIGHT, M_TOGGLE_TEX,

    // 显示
    M_TOGGLE_GRID = 500, M_TOGGLE_AXES, M_TOGGLE_GROUND, M_TOGGLE_HUD,

    // 其他
    M_SCREENSHOT = 600, M_HELP, M_QUIT,

    // 机器人（2026 实验主角）
    M_ROBOT_STAND = 700, M_ROBOT_WAVE, M_ROBOT_TPOSE, M_ROBOT_WALKFREEZE,
    M_ROBOT_TOGGLE_WALK,
    M_ROBOT_TOGGLE_PATH,          // W3-1：贝塞尔曲线漫步
    M_ROBOT_TOGGLE_PATHGUIDE,     // 曲线/控制点可视化

    // 森林分形参数（2026 加分项：分形算法）
    M_FOREST_DETAIL_UP = 720, M_FOREST_DETAIL_DOWN,
    M_FOREST_ANGLE_DOWN, M_FOREST_ANGLE_UP,
    M_FOREST_REBUILD
};

void inputKeyboard(unsigned char key, int x, int y);
void inputSpecial(int key, int x, int y);
void inputMouseButton(int button, int state, int x, int y);
void inputMouseMotion(int x, int y);
void inputMouseWheel(int wheel, int direction, int x, int y);

void inputBuildMenu();
void inputMenu(int choice);

// 供菜单直接复用键盘动作
void inputAction(int action);
