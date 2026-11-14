#include "input.h"
#include "app.h"
#include "forest.h"
#include "math3d.h"
#include "robot.h"

#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// 拖动时鼠标位移 → 变换量的换算基准
constexpr float kTransWorldPerWindow = 2.0f;    // 拖过整个窗口约等于移动 2 个场景半径
constexpr float kRotDegPerWindow     = 180.0f;
constexpr float kScalePerWindow      = 2.0f;

inline bool shiftDown() { return (glutGetModifiers() & GLUT_ACTIVE_SHIFT) != 0; }

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// 相机被手动操作过之后，视图预设要让位给自由相机。
//
// 同时停掉动画预览：预热播放时每帧都会覆写相机，用户拖半天看不到反应。
// 手动一介入就交还控制权，预览可以随时按空格再开。
// appStopPreview() 内部对「本来就没在播」直接返回，所以这里可以放心反复调。
inline void leavePreset()
{
    g_app.viewPreset = ViewPreset::Free;
    appStopPreview();
}

// ---------------------------------------------------------------------------
// 实验五/六：由鼠标位移构造一次增量变换矩阵
// ---------------------------------------------------------------------------
void buildObjectDelta(TransMode mode, float dx, float dy, float delta[16])
{
    const float winW = (float)std::max(1, g_app.winW);
    const float winH = (float)std::max(1, g_app.winH);
    const float r    = g_app.scene.radius();

    switch (mode)
    {
    case TransMode::Translate:
    {
        // 窗口坐标系的 y 轴与世界坐标系相反，所以 dy 取负
        const float tx =  dx * (kTransWorldPerWindow * r / winW);
        const float ty = -dy * (kTransWorldPerWindow * r / winH);
        mat4FromTranslation(delta, tx, ty, 0.0f);
        break;
    }

    case TransMode::Rotate:
    {
        // 拖动方向决定旋转轴，拖动距离决定角度（轨球式手感）
        const float angle = std::sqrt(dx * dx + dy * dy) * (kRotDegPerWindow / std::max(winW, winH));
        float ax = dy, ay = dx, az = 0.0f;
        // 两个分量都接近 0 时轴退化，mat4FromAxisAngle 会返回单位阵，正好表示无旋转
        mat4FromAxisAngle(delta, angle, ax, ay, az);
        break;
    }

    case TransMode::Scale:
    {
        // 指导书提醒：轴向量本身不加 1，调用时才加 —— 否则鼠标没动时缩放会归零
        const float sx = clampf(1.0f + dx * (kScalePerWindow / winW), 0.05f, 20.0f);
        const float sy = clampf(1.0f - dy * (kScalePerWindow / winH), 0.05f, 20.0f);
        const float sz = std::sqrt(sx * sy);       // 保持体积大致不变
        mat4FromScale(delta, sx, sy, sz);
        break;
    }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// 统一动作入口：键盘与菜单共用同一套编号
// ---------------------------------------------------------------------------
void inputAction(int action)
{
    switch (action)
    {
    // ---- 变换模式 ----
    case M_TRANS_T: g_app.transMode = TransMode::Translate; break;
    case M_TRANS_R: g_app.transMode = TransMode::Rotate;    break;
    case M_TRANS_S: g_app.transMode = TransMode::Scale;     break;
    case M_RESET_OBJ: appResetObjects(); break;
    case M_SELECT_NEXT:
        g_app.scene.selected = g_app.scene.nextSelectable(g_app.scene.selected);
        appUpdateTitle();
        break;

    // ---- 投影 ----
    case M_PROJ_PERSP: appSetProjection(ProjMode::Perspective); break;
    case M_PROJ_ORTHO: appSetProjection(ProjMode::Ortho);       break;
    case M_PROJ_CAV:   appSetProjection(ProjMode::Cavalier);    break;
    case M_PROJ_CAB:   appSetProjection(ProjMode::Cabinet);     break;
    case M_OBL_ANGLE_DOWN: g_app.obliqueAngleDeg = clampf(g_app.obliqueAngleDeg - 5.0f, 5.0f, 85.0f); break;
    case M_OBL_ANGLE_UP:   g_app.obliqueAngleDeg = clampf(g_app.obliqueAngleDeg + 5.0f, 5.0f, 85.0f); break;
    case M_FOV_DOWN: g_app.camera.fovY = clampf(g_app.camera.fovY - 5.0f, 5.0f, 120.0f); break;
    case M_FOV_UP:   g_app.camera.fovY = clampf(g_app.camera.fovY + 5.0f, 5.0f, 120.0f); break;

    // ---- 视图 ----
    case M_VIEW_FRONT:  appSetViewPreset(ViewPreset::Front);  break;
    case M_VIEW_BACK:   appSetViewPreset(ViewPreset::Back);   break;
    case M_VIEW_LEFT:   appSetViewPreset(ViewPreset::Left);   break;
    case M_VIEW_RIGHT:  appSetViewPreset(ViewPreset::Right);  break;
    case M_VIEW_TOP:    appSetViewPreset(ViewPreset::Top);    break;
    case M_VIEW_BOTTOM: appSetViewPreset(ViewPreset::Bottom); break;
    case M_VIEW_FREE:   leavePreset(); break;
    case M_RESET_CAM:   appResetCamera(); break;
    case M_TOGGLE_QUAD: g_app.quadView = !g_app.quadView; break;

    // ---- 渲染 ----
    case M_RENDER_SOLID: g_app.renderMode = RenderMode::Solid;     break;
    case M_RENDER_WIRE:  g_app.renderMode = RenderMode::Wireframe; break;
    case M_RENDER_POINT: g_app.renderMode = RenderMode::Points;    break;
    case M_TOGGLE_CULL:  g_app.cullFace    = !g_app.cullFace;    break;
    case M_TOGGLE_SHADE: g_app.smoothShade = !g_app.smoothShade; break;
    case M_TOGGLE_LIGHT: g_app.lighting    = !g_app.lighting;    break;
    case M_TOGGLE_TEX:   g_app.textureOn   = !g_app.textureOn;   break;

    // ---- 显示 ----
    case M_TOGGLE_GRID:   g_app.showGrid   = !g_app.showGrid;   break;
    case M_TOGGLE_AXES:   g_app.showAxes   = !g_app.showAxes;   break;
    case M_TOGGLE_GROUND: g_app.showGround = !g_app.showGround; break;
    case M_TOGGLE_HUD:    g_app.showHUD    = !g_app.showHUD;    break;

    // ---- 其他 ----
    case M_HELP:       g_app.showHelp = !g_app.showHelp; break;
    case M_SCREENSHOT: appSaveScreenshot("screenshot.bmp"); break;
    case M_QUIT:       glutLeaveMainLoop(); return;

    // ---- 机器人（2026 实验主角：局部几何变换 / 关节动画加分项）----
    case M_ROBOT_STAND:      robotSetPose(RobotPose::Stand);      break;
    case M_ROBOT_WAVE:       robotSetPose(RobotPose::Wave);       break;
    case M_ROBOT_TPOSE:      robotSetPose(RobotPose::TPose);      break;
    case M_ROBOT_WALKFREEZE: robotSetPose(RobotPose::WalkFreeze); break;
    case M_ROBOT_TOGGLE_WALK: robotToggleWalk();                  break;

    // ---- 森林分形参数（L-system 迭代深度 / 分支角，调整后即时重建）----
    case M_FOREST_DETAIL_UP:   forestSetDetail(std::min(1, forestDetail() + 1));
                               g_app.scene.rebuildForest(); break;
    case M_FOREST_DETAIL_DOWN: forestSetDetail(std::max(-1, forestDetail() - 1));
                               g_app.scene.rebuildForest(); break;
    case M_FOREST_ANGLE_UP:    forestAdjustAngle(1.12f);
                               g_app.scene.rebuildForest(); break;
    case M_FOREST_ANGLE_DOWN:  forestAdjustAngle(1.0f / 1.12f);
                               g_app.scene.rebuildForest(); break;
    case M_FOREST_REBUILD:     g_app.scene.rebuildForest(); break;

    default: break;
    }

    glutPostRedisplay();
}

// ---------------------------------------------------------------------------
void inputKeyboard(unsigned char key, int /*x*/, int /*y*/)
{
    switch (key)
    {
    case 27:  inputAction(M_QUIT); return;                    // ESC

    // 变换模式（s 让给相机漫游，缩放用 y —— 指导书的建议）
    case 't': inputAction(M_TRANS_T); return;
    case 'r': inputAction(M_TRANS_R); return;
    case 'y': inputAction(M_TRANS_S); return;

    case '0':
        // Shift+0 复位相机，单独按 0 复位物体变换
        if (shiftDown()) inputAction(M_RESET_CAM);
        else             inputAction(M_RESET_OBJ);
        return;

    case '\t': inputAction(M_SELECT_NEXT); return;            // Tab

    // 绘制模式循环
    case '1':
        g_app.renderMode = (g_app.renderMode == RenderMode::Solid)     ? RenderMode::Wireframe
                         : (g_app.renderMode == RenderMode::Wireframe) ? RenderMode::Points
                                                                       : RenderMode::Solid;
        break;

    // 投影
    case '3': inputAction(M_PROJ_PERSP); return;
    case 'o': inputAction(M_PROJ_ORTHO); return;
    case '7': inputAction(M_PROJ_CAV);   return;
    case '9': inputAction(M_PROJ_CAB);   return;
    case '-': inputAction(M_OBL_ANGLE_DOWN); return;
    case '=': inputAction(M_OBL_ANGLE_UP);   return;
    case '[': inputAction(M_FOV_DOWN); return;
    case ']': inputAction(M_FOV_UP);   return;

    // 正平行投影三视图
    case '2': inputAction(M_VIEW_FRONT);  return;
    case '8': inputAction(M_VIEW_BACK);   return;
    case '4': inputAction(M_VIEW_LEFT);   return;
    case '6': inputAction(M_VIEW_RIGHT);  return;
    case '5': inputAction(M_VIEW_TOP);    return;

    case 'v': inputAction(M_TOGGLE_QUAD); return;

    // 机器人：姿势切换与走路循环（数字键已被视图/投影占满，用字母）
    case 'n':
        if (robotAlive()) { robotNextPose(); glutPostRedisplay(); }
        return;
    case 'b':
        if (robotAlive()) { robotToggleWalk(); glutPostRedisplay(); }
        return;

    // 森林分形参数：f 循环迭代深度 (0 → +1 → -1 → 0)，u/j 调分支角（调整即重建）
    case 'f':
        if (g_app.sceneKind == SceneKind::Forest)
        {
            if (forestDetail() == 0) inputAction(M_FOREST_DETAIL_UP);
            else
            {
                forestSetDetail(forestDetail() > 0 ? -1 : 0);
                g_app.scene.rebuildForest();
                glutPostRedisplay();
            }
        }
        return;
    case 'u':
        if (g_app.sceneKind == SceneKind::Forest) inputAction(M_FOREST_ANGLE_DOWN);
        return;
    case 'j':
        if (g_app.sceneKind == SceneKind::Forest) inputAction(M_FOREST_ANGLE_UP);
        return;

    // 空格：播放 / 暂停动画预览。森林场景里默认就是兔子漫步。
    case ' ': appTogglePreview(); return;

    // 相机漫游
    case 'w': leavePreset(); g_app.camera.fly( 0.30f,  0.00f, 0.0f); break;
    case 's': leavePreset(); g_app.camera.fly(-0.30f,  0.00f, 0.0f); break;
    case 'a': leavePreset(); g_app.camera.fly( 0.00f, -0.30f, 0.0f); break;
    case 'd': leavePreset(); g_app.camera.fly( 0.00f,  0.30f, 0.0f); break;
    case 'q': leavePreset(); g_app.camera.fly( 0.00f,  0.00f, -0.30f); break;
    case 'e': leavePreset(); g_app.camera.fly( 0.00f,  0.00f,  0.30f); break;

    // 渲染与显示开关
    case 'c': inputAction(M_TOGGLE_CULL);   return;
    case 'm': inputAction(M_TOGGLE_SHADE);  return;
    case 'l': inputAction(M_TOGGLE_LIGHT);  return;
    case 'T': inputAction(M_TOGGLE_TEX);    return;
    case 'g': inputAction(M_TOGGLE_GRID);   return;
    case 'x': inputAction(M_TOGGLE_AXES);   return;
    case 'p': inputAction(M_TOGGLE_GROUND); return;
    case 'h': inputAction(M_HELP);          return;

    default: return;
    }

    glutPostRedisplay();
}

void inputSpecial(int key, int /*x*/, int /*y*/)
{
    switch (key)
    {
    case GLUT_KEY_F2: g_app.enhanced = !g_app.enhanced; appPostRedisplay(); return;
    case GLUT_KEY_F3: g_app.shadows = !g_app.shadows; appPostRedisplay(); return;
    case GLUT_KEY_F12: inputAction(M_SCREENSHOT); return;
    case GLUT_KEY_F1:  inputAction(M_HELP);       return;

    // 方向键也能平移相机，方便没有中键的鼠标
    case GLUT_KEY_LEFT:  leavePreset(); g_app.camera.fly(0.0f, -0.30f, 0.0f); break;
    case GLUT_KEY_RIGHT: leavePreset(); g_app.camera.fly(0.0f,  0.30f, 0.0f); break;
    case GLUT_KEY_UP:    leavePreset(); g_app.camera.fly(0.30f, 0.0f,  0.0f); break;
    case GLUT_KEY_DOWN:  leavePreset(); g_app.camera.fly(-0.30f, 0.0f, 0.0f); break;
    default: return;
    }
    glutPostRedisplay();
}

// ---------------------------------------------------------------------------
void inputMouseButton(int button, int state, int x, int y)
{
    if (state == GLUT_DOWN)
    {
        g_app.lastX = x;
        g_app.lastY = y;

        if (button == GLUT_LEFT_BUTTON)
        {
            // Shift+左键 = 平移相机（中键留给菜单，右键留给旋转相机）
            g_app.dragMode = shiftDown() ? AppState::DragMode::Pan
                                         : AppState::DragMode::Object;
        }
        else if (button == GLUT_RIGHT_BUTTON)
        {
            g_app.dragMode = AppState::DragMode::Camera;
        }
        else
        {
            g_app.dragMode = AppState::DragMode::None;
        }
    }
    else if (state == GLUT_UP)
    {
        g_app.dragMode = AppState::DragMode::None;
    }
}

void inputMouseMotion(int x, int y)
{
    if (g_app.dragMode == AppState::DragMode::None) return;

    const float dx = (float)(x - g_app.lastX);
    const float dy = (float)(y - g_app.lastY);
    g_app.lastX = x;
    g_app.lastY = y;

    if (dx == 0.0f && dy == 0.0f) return;

    const float winW = (float)std::max(1, g_app.winW);
    const float winH = (float)std::max(1, g_app.winH);

    switch (g_app.dragMode)
    {
    case AppState::DragMode::Object:
    {
        // 同理：预览播放时兔子每帧都被动画摆回原位，手动变换会被立刻覆盖
        appStopPreview();

        float delta[16];
        buildObjectDelta(g_app.transMode, dx, dy, delta);
        appAccumulateObjectTransform(delta);
        break;
    }

    case AppState::DragMode::Camera:
        // 绕参考点旋转视角（实验七的 quatRotate 分支）
        leavePreset();
        g_app.camera.orbit(dy * (180.0f / winH), dx * (180.0f / winW));
        break;

    case AppState::DragMode::Pan:
    {
        leavePreset();
        const float s = g_app.camera.distance() * 2.0f;
        g_app.camera.pan(-dx * (s / winW), dy * (s / winH));
        break;
    }

    default:
        break;
    }

    glutPostRedisplay();
}

void inputMouseWheel(int /*wheel*/, int direction, int /*x*/, int /*y*/)
{
    leavePreset();
    // freeglut: direction > 0 表示向前滚（远离用户）
    g_app.camera.dolly(direction > 0 ? 1.10f : (1.0f / 1.10f));
    glutPostRedisplay();
}

// ---------------------------------------------------------------------------
void inputMenu(int choice)
{
    inputAction(choice);
}

void inputBuildMenu()
{
    // 菜单文字一律用 ASCII：freeglut 的菜单/位图字体只能画 ASCII，
    // 中文会变成空白或乱码。
    const int mTrans = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Translate        (t)",       M_TRANS_T);
    glutAddMenuEntry("Rotate           (r)",       M_TRANS_R);
    glutAddMenuEntry("Scale            (y)",       M_TRANS_S);
    glutAddMenuEntry("Reset object transform (0)", M_RESET_OBJ);
    glutAddMenuEntry("Select next object  (Tab)",  M_SELECT_NEXT);

    const int mProj = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Perspective            (3)", M_PROJ_PERSP);
    glutAddMenuEntry("Orthographic           (o)", M_PROJ_ORTHO);
    glutAddMenuEntry("Oblique cavalier  L=1  (7)", M_PROJ_CAV);
    glutAddMenuEntry("Oblique cabinet   L=0.5(9)", M_PROJ_CAB);
    glutAddMenuEntry("Oblique angle  -       (-)", M_OBL_ANGLE_DOWN);
    glutAddMenuEntry("Oblique angle  +       (=)", M_OBL_ANGLE_UP);
    glutAddMenuEntry("FOV -                  ([)", M_FOV_DOWN);
    glutAddMenuEntry("FOV +                  (])", M_FOV_UP);

    const int mView = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Front   (2)", M_VIEW_FRONT);
    glutAddMenuEntry("Back    (8)", M_VIEW_BACK);
    glutAddMenuEntry("Left    (4)", M_VIEW_LEFT);
    glutAddMenuEntry("Right   (6)", M_VIEW_RIGHT);
    glutAddMenuEntry("Top     (5)", M_VIEW_TOP);
    glutAddMenuEntry("Bottom",      M_VIEW_BOTTOM);
    glutAddMenuEntry("Free view",   M_VIEW_FREE);
    glutAddMenuEntry("Reset camera (Shift+0)", M_RESET_CAM);
    glutAddMenuEntry("4-viewport layout  (v)", M_TOGGLE_QUAD);

    const int mRender = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Solid",                   M_RENDER_SOLID);
    glutAddMenuEntry("Wireframe",               M_RENDER_WIRE);
    glutAddMenuEntry("Points",                  M_RENDER_POINT);
    glutAddMenuEntry("Back-face culling   (c)", M_TOGGLE_CULL);
    glutAddMenuEntry("Smooth/Flat shading (m)", M_TOGGLE_SHADE);
    glutAddMenuEntry("Lighting            (l)", M_TOGGLE_LIGHT);
    glutAddMenuEntry("Texture             (T)", M_TOGGLE_TEX);

    const int mShow = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Grid    (g)", M_TOGGLE_GRID);
    glutAddMenuEntry("Axes    (x)", M_TOGGLE_AXES);
    glutAddMenuEntry("Ground  (p)", M_TOGGLE_GROUND);
    glutAddMenuEntry("HUD",         M_TOGGLE_HUD);

    const int mRobot = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Pose: stand       ",  M_ROBOT_STAND);
    glutAddMenuEntry("Pose: wave        ",  M_ROBOT_WAVE);
    glutAddMenuEntry("Pose: T-pose      ",  M_ROBOT_TPOSE);
    glutAddMenuEntry("Pose: walk freeze ",  M_ROBOT_WALKFREEZE);
    glutAddMenuEntry("Walk animation (b)",  M_ROBOT_TOGGLE_WALK);

    const int mForest = glutCreateMenu(inputMenu);
    glutAddMenuEntry("L-system depth +   (f)", M_FOREST_DETAIL_UP);
    glutAddMenuEntry("L-system depth -   (f)", M_FOREST_DETAIL_DOWN);
    glutAddMenuEntry("Branch angle -     (u)", M_FOREST_ANGLE_DOWN);
    glutAddMenuEntry("Branch angle +     (j)", M_FOREST_ANGLE_UP);
    glutAddMenuEntry("Rebuild forest         ", M_FOREST_REBUILD);

    glutCreateMenu(inputMenu);
    glutAddSubMenu("Transform",   mTrans);
    glutAddSubMenu("Projection",  mProj);
    glutAddSubMenu("View",        mView);
    glutAddSubMenu("Render",      mRender);
    glutAddSubMenu("Show",        mShow);
    glutAddSubMenu("Robot",       mRobot);
    glutAddSubMenu("Forest (fractal)", mForest);
    glutAddMenuEntry("Save screenshot (F12)", M_SCREENSHOT);
    glutAddMenuEntry("Toggle help (h)",       M_HELP);
    glutAddMenuEntry("Quit (ESC)",            M_QUIT);

    // 指导书原话：右键拖动用来旋转相机，所以菜单绑到鼠标中键
    glutAttachMenu(GLUT_MIDDLE_BUTTON);
}
