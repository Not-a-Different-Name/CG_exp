// 三维模型查看器 —— 计算机图形学总结实验
//
//   三维场景绘制：实验四的立方体 + GLUT 图元 + 实验十的 OBJ 模型导入
//   几何变换    ：实验五/六的 objectXform 累积变换 + 鼠标键盘交互
//   观察变换    ：实验七的 gluLookAt 三视图 + 相机漫游 + 斜平行投影
//   要求 3      ：全投影模式保宽高比 reshape，以及干净退出
//
// 构建见同目录 CMakeLists.txt；操作说明见 README.md。
//
// 命令行（写实验报告时可以批量出图）：
//   viewer.exe [model.obj] [--shot out.bmp] [--proj persp|ortho|cav|cab]
//              [--view front|back|left|right|top|bottom|free]
//              [--render solid|wire|points] [--quad] [--no-hud]
#include "app.h"
#include "animals.h"
#include "bezier.h"
#include "forest.h"
#include "input.h"
#include "robot.h"

#include <GL/freeglut.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>

namespace {

bool fileExists(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

std::string dirOf(const std::string& path)
{
    const size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? std::string() : path.substr(0, pos + 1);
}

std::string parentDir(const std::string& dir)
{
    if (dir.size() < 2) return std::string();
    std::string d = dir;
    while (d.size() > 1 && (d.back() == '/' || d.back() == '\\')) d.pop_back();
    const size_t pos = d.find_last_of("/\\");
    return (pos == std::string::npos) ? std::string() : d.substr(0, pos + 1);
}

// 可执行文件在 build/Release/ 下，而模型在仓库根的 Models/ 下。
// 从 exe 所在目录逐级向上找，这样从 VS 里启动、从命令行启动、双击启动都能找到。
std::string findDefaultModel()
{
    char exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
        return std::string();

    const std::string rel = "Models/bunny_10k.obj";   // 默认模型（Stanford Bunny 10k）
    std::string dir = dirOf(exePath);

    for (int i = 0; i < 6 && !dir.empty(); ++i)
    {
        const std::string cand = dir + rel;
        if (fileExists(cand)) return cand;
        dir = parentDir(dir);
    }
    return std::string();
}

// ---- 命令行 ----
struct Options
{
    std::string modelPath;
    std::string shotPath;
    std::string proj, view, render;
    int  winW = 1280, winH = 800;
    bool classic = false, noShadows = false;
    bool quad     = false;
    bool noHud    = false;
    bool showHelp = false;

    std::string upAxis;        // --up y|z|auto，空表示 auto
    std::string sceneName;     // --scene studio|forest
    std::string robotPose;     // --pose stand|wave|tpose|walk，出图用（直接定格到位）
    float       robotTime = -1.0f;   // --time 秒：锁定机器人时间轴（行走相位定格）
    bool        robotWalk = false;   // --robot-walk：开启行走循环（配合 --time 出序列图）
    bool        robotPath = false;   // --path：沿贝塞尔曲线漫步（配合 --time 定格）
    bool        pathGuide = false;   // --path-guide：显示曲线与控制点

    // 出图用的机位覆盖（自由相机）：--eye 3,4,5  --target 0,0.5,0
    bool        hasEye = false, hasTarget = false;
    float       eye[3] = { 0, 0, 0 }, target[3] = { 0, 0, 0 };

    // 视频录制
    std::string recordPath;
    std::string animName;      // 空 -> 用默认动画 turntable
    float       seconds = 6.0f;
    int         fps     = 30;
};

Options parseArgs(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&](std::string& dst) { if (i + 1 < argc) dst = argv[++i]; };
        if      (a == "--shot")   next(o.shotPath);
        else if (a == "--proj")   next(o.proj);
        else if (a == "--view")   next(o.view);
        else if (a == "--render") next(o.render);
        else if (a == "--classic") o.classic = true;
        else if (a == "--no-shadows") o.noShadows = true;
        else if (a == "--quad")   o.quad  = true;
        else if (a == "--no-hud") o.noHud = true;
        else if (a == "--show-help") o.showHelp = true;
        else if (a == "--up")      next(o.upAxis);
        else if (a == "--scene")   next(o.sceneName);
        else if (a == "--pose")    next(o.robotPose);
        else if (a == "--robot-walk") o.robotWalk = true;
        else if (a == "--path")       o.robotPath = true;
        else if (a == "--path-guide") o.pathGuide = true;
        else if (a == "--eye" || a == "--target")
        {
            std::string s; next(s);
            float v[3];
            if (std::sscanf(s.c_str(), "%f,%f,%f", &v[0], &v[1], &v[2]) == 3)
            {
                float* dst = (a == "--eye") ? o.eye : o.target;
                dst[0] = v[0]; dst[1] = v[1]; dst[2] = v[2];
                (a == "--eye" ? o.hasEye : o.hasTarget) = true;
            }
            else
                std::printf("[main] %s expects x,y,z (got '%s')\n", a.c_str(), s.c_str());
        }
        else if (a == "--time")
        {
            std::string s; next(s);
            const double v = atof(s.c_str());
            if (v >= 0.0) o.robotTime = (float)v;
        }
        else if (a == "--record")  next(o.recordPath);
        else if (a == "--anim")    next(o.animName);
        else if (a == "--fps")
        {
            std::string s; next(s);
            const int v = atoi(s.c_str());
            if (v > 0 && v <= 240) o.fps = v;
        }
        else if (a == "--seconds")
        {
            std::string s; next(s);
            const double v = atof(s.c_str());
            if (v > 0.05 && v <= 600.0) o.seconds = (float)v;
        }
        else if (a == "--size")
        {
            // --size 1600x300 —— 用来验证「窗口变化图形不变形」
            std::string s; next(s);
            const size_t x = s.find_first_of("xX*");
            if (x != std::string::npos)
            {
                const int w = atoi(s.substr(0, x).c_str());
                const int h = atoi(s.substr(x + 1).c_str());
                if (w > 0 && h > 0) { o.winW = w; o.winH = h; }
            }
        }
        else if (!a.empty() && a[0] != '-') o.modelPath = a;
    }
    if (o.modelPath.empty()) o.modelPath = findDefaultModel();
    return o;
}

// --shot 模式下窗口会被隐藏：截图从后缓冲读，不需要窗口真的可见，
// 这样批处理出图不会满屏闪窗口。

// ---------------------------------------------------------------------------
// 自检：交互逻辑（变换矩阵、四元数旋转、相机漫游）没法靠截图验证，
// 这里用断言把它们跑一遍。`viewer.exe --selftest`，不需要窗口。
// ---------------------------------------------------------------------------
int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failures;
}

bool nearf(float a, float b, float eps = 1e-2f) { return std::fabs(a - b) < eps; }

int runSelfTest()
{
    // 无缓冲输出：自检若崩溃，已跑过的断言也能看到（管道下默认全缓冲会丢日志）
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== self-test: math / camera logic ===\n");

    {
        float I[16]; mat4Identity(I);
        float A[16]; mat4FromTranslation(A, 1.0f, 2.0f, 3.0f);
        float R[16]; mat4Multiply(R, I, A);
        check(nearf(R[12], 1) && nearf(R[13], 2) && nearf(R[14], 3),
              "mat4Multiply with identity is a no-op");
    }
    {
        // 右手系：+X 绕 +Y 转 90 度应落到 -Z
        float M[16]; mat4FromAxisAngle(M, 90.0f, 0.0f, 1.0f, 0.0f);
        const Vec3 p = mat4MulDir(M, v3(1, 0, 0));
        check(nearf(p.x, 0) && nearf(p.y, 0) && nearf(p.z, -1),
              "mat4FromAxisAngle: +Y 90deg maps +X to -Z");
    }
    {
        float x = 1, y = 0, z = 0;
        quatRotate(3.14159265f * 0.5f, 0.0f, 1.0f, 0.0f, x, y, z);
        check(nearf(x, 0) && nearf(y, 0) && nearf(z, -1),
              "quatRotate: +Y 90deg maps +X to -Z (same as matrix form)");
    }
    {
        Camera c; c.reset(1.5f);
        const float d0 = c.distance();
        c.orbit(17.0f, -23.0f);
        check(nearf(c.distance(), d0), "Camera::orbit preserves distance to target");
    }
    {
        Camera c; c.reset(1.5f);
        const Vec3 e0 = c.eye;
        c.orbit(360.0f, 0.0f);
        check(nearf(c.eye.x, e0.x) && nearf(c.eye.y, e0.y) && nearf(c.eye.z, e0.z),
              "Camera::orbit 360deg returns to the start position");
    }
    {
        Camera c; c.reset(1.5f);
        const float d0 = c.distance();
        c.dolly(0.5f);
        check(nearf(c.distance(), d0 * 0.5f), "Camera::dolly(0.5) halves the distance");
    }
    {
        Camera c; c.reset(1.5f);
        c.setPreset((int)ViewPreset::Top, 1.5f);
        // 顶视图的 up 必须偏离视线方向，否则 gluLookAt 的基退化（叉积为零、画面空白）
        check(nearf(c.up.z, -1.0f), "Top preset uses up=(0,0,-1)");
        check(v3len(c.right()) > 0.9f, "Top preset: right vector is well-defined (not degenerate)");
    }
    {
        Camera c; c.reset(1.5f);
        float d = -1.0f;
        bool same = true;
        for (int i = (int)ViewPreset::Front; i <= (int)ViewPreset::Bottom; ++i)
        {
            c.setPreset(i, 1.5f);
            if (d < 0) d = c.distance();
            else if (!nearf(c.distance(), d)) same = false;
        }
        check(same, "all six view presets share one camera distance (common 三视图 scale)");
    }
    {
        Camera c; c.reset(1.5f);
        const Vec3 e0 = c.eye, f0 = c.forward();
        const float d0 = c.distance();
        c.fly(1.0f, 0.0f, 0.0f);
        const Vec3 moved = c.eye - e0;
        check(v3dot(v3norm(moved), f0) > 0.99f,
              "Camera::fly(+fwd) moves the eye along the view direction");
        check(nearf(c.distance(), d0),
              "Camera::fly translates (preserves distance) rather than dollying");
    }

    // ---- 机器人层级建模的几何不变量（交互变换没法截图，用断言钉死）----
    // 绕 Y 的根朝向不改变高度，所以这些 y 值与朝向无关，是干净的断言目标。
    {
        std::printf("  -- robot hierarchy --\n");
        Scene s;
        robotCreate(s);

        // T-pose：手臂水平伸直 -> 臂中心与肩同高（kShoulderY）
        robotSetPose(RobotPose::TPose, /*snap=*/true);
        robotSetTimeOverride(0.0f);
        robotUpdate(s, 0.0f);
        float p[3];
        bool ok = robotPartPosition(s, "Robot Arm L", p);
        check(ok && nearf(p[1], 1.66f, 1e-3f), "T-pose: arm center sits at shoulder height");

        // 站立：手臂自然下垂 -> 臂中心 = 肩高 - 半臂长
        robotSetPose(RobotPose::Stand, true);
        robotUpdate(s, 0.0f);
        ok = robotPartPosition(s, "Robot Arm L", p);
        check(ok && nearf(p[1], 1.66f - 0.31f, 1e-3f), "stand: arm hangs below shoulder by half arm");

        // 站立：脚底贴地 -> 腿中心 = 髋高 - 半腿长（0.54），即脚底正好在 y=0
        ok = robotPartPosition(s, "Robot Leg L", p);
        check(ok && nearf(p[1], 0.54f, 1e-3f) && nearf(p[1] - 0.54f, 0.0f, 1e-3f),
              "stand: leg center at hip-half-leg => feet on the ground");

        // 行走定格：左腿前摆 30 度 -> 臂长杆绕髋转动，中心下降 0.54*(1-cos30)
        // （该姿势还带 0.03 的重心下沉 bodyBob，一并计入期望值）
        robotSetPose(RobotPose::WalkFreeze, true);
        robotUpdate(s, 0.0f);
        ok = robotPartPosition(s, "Robot Leg L", p);
        check(ok && nearf(p[1], 0.03f + 1.08f - 0.54f * std::cos(30.0f * 3.14159265f / 180.0f), 1e-3f),
              "walk-freeze: forward leg pivots about the hip (center drops by 1-cos)");

        // 机器人沿贝塞尔路径行走：根位置 = eval(speed * t)，且全程留在空地里。
        // 注意 robotCreate 是全局单例，必须复用上面这个 Scene（再建一个空 Scene
        // 调 robotUpdate 会拿旧下标越界 —— 第一版自检就是这样段错误的）。
        std::printf("  -- bezier path walk --\n");
        robotSetPathWalk(true);
        robotSetTimeOverride(3.0f);
        robotUpdate(s, 3.0f);

        const Vec3 want = robotPath().evalAtDistance(robotPathSpeed() * 3.0f);
        ok = robotPartPosition(s, "Robot", p);
        check(ok && nearf(p[0], want.x, 1e-3f) && nearf(p[2], want.z, 1e-3f),
              "path walk: robot root follows the curve at s = speed * t");

        float maxR = 0.0f;
        for (const Vec3& q : robotPath().samples)
            maxR = std::max(maxR, std::sqrt(q.x * q.x + q.z * q.z));
        check(maxR < 7.0f, "robot path stays inside the forest clearing (r < 7)");

        robotSetPathWalk(false);
        robotSetTimeOverride(-1.0f);
    }

    // ---- W3-1 贝塞尔闭曲线 + 弧长参数化漫步 ----
    {
        std::printf("  -- bezier path (W3-1) --\n");

        // 锚点间距刻意不均匀：若按参数 u 匀速走，步长会忽长忽短。
        std::vector<Vec3> anchors = {
            v3( 2.0f, 0.0f,  0.0f),
            v3( 0.5f, 0.0f,  1.2f),
            v3(-1.9f, 0.0f,  0.5f),
            v3(-1.0f, 0.0f, -1.8f),
        };
        BezierPath path;
        path.buildClosed(anchors);

        check(path.segments.size() == anchors.size() && path.total > 8.0f,
              "closed bezier: one cubic segment per anchor, nonzero arc length");

        // 每段起点/终点就是锚点 => 曲线穿过全部锚点
        bool hits = true;
        for (size_t i = 0; i < anchors.size(); ++i)
        {
            const Vec3 q = path.evalProportional((float)i / (float)anchors.size());
            if (v3len(q - anchors[i]) > 1e-3f) hits = false;
        }
        check(hits, "curve passes through every anchor (Catmull-Rom handles)");

        // 弧长参数化：等弧长距离求值，相邻点间距必须≈常量
        float maxErr = 0.0f;
        const float step = 0.15f;
        for (int i = 0; i < 20; ++i)
        {
            const float s = 0.9f * path.total * (float)i / 20.0f;
            const Vec3 a = path.evalAtDistance(s);
            const Vec3 b = path.evalAtDistance(s + step);
            const float d = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
            maxErr = std::max(maxErr, std::fabs(d - step));
        }
        check(maxErr < 5e-3f,
              "arc-length parameterization => constant speed (|step-0.15| < 0.005)");

    }

    // ---- L-system 分形森林：确定性与迭代单调性 ----
    {
        std::printf("  -- fractal forest (L-system) --\n");
        std::vector<Vec3> path{ v3(0.0f, 0.0f, 0.0f) };
        MeshData a, b, c;
        buildForest(a, 20, 4, path, 2.0f, 7u);
        buildForest(b, 20, 4, path, 2.0f, 7u);
        check(a.indices.size() == b.indices.size(),
              "same seed => identical forest geometry (reproducible)");

        forestSetDetail(+1);
        buildForest(c, 20, 4, path, 2.0f, 7u);
        forestSetDetail(0);
        check(c.indices.size() > a.indices.size(),
              "+1 L-system iteration grows geometry (fractal self-similarity)");
    }

    // ---- 小动物行为：贴地、在各自路线圆内、位置随时间推进 ----
    {
        std::printf("  -- animals (deer & fox) --\n");
        Scene s;
        animalsCreate(s);

        animalsUpdate(s, 3.0f);
        float p[3];
        bool ok = animalPartPosition(s, "Deer", p);
        check(ok && std::fabs(p[1]) < 0.1f, "deer stays on the ground (|y| < 0.1)");
        const float deerX0 = p[0], deerZ0 = p[2];
        check(std::fabs(deerX0 - (-3.7f)) <= 1.4f + 0.01f &&
              std::fabs(deerZ0 - 0.7f)    <= 1.4f + 0.01f,
              "deer stays inside its walking circle");

        animalsUpdate(s, 40.0f);
        ok = animalPartPosition(s, "Deer", p);
        const bool moved = ok && (std::fabs(p[0] - deerX0) > 0.05f || std::fabs(p[2] - deerZ0) > 0.05f);
        check(moved, "deer advances along its route over time");

        // 吃草窗口内位置冻结（走-停节律）。fmod(57,15)=12 > 11，正在吃草。
        animalsUpdate(s, 57.0f);
        float q[3];
        animalPartPosition(s, "Deer", q);
        animalsUpdate(s, 58.0f);
        animalPartPosition(s, "Deer", p);
        const bool frozen = (std::fmod(57.0f, 15.0f) > 11.0f) &&
                            std::fabs(p[0] - q[0]) < 1e-3f && std::fabs(p[2] - q[2]) < 1e-3f;
        check(frozen, "deer freezes while grazing (walk 11s / graze 4s)");
    }

    std::printf("=== %s (%d failure(s)) ===\n", g_failures ? "FAILED" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}

} // namespace

int main(int argc, char** argv)
{
    // 自检不需要窗口，在 glutInit 之前就返回
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--selftest")
            return runSelfTest();

    const Options opt = parseArgs(argc, argv);

    std::printf("=== 3D Model Viewer ===\n");
    if (opt.modelPath.empty())
        std::printf("[main] no OBJ model found; built-in primitives only.\n"
                    "       usage: viewer.exe [path\\to\\model.obj]\n");
    else
        std::printf("[main] model: %s\n", opt.modelPath.c_str());

    glutInit(&argc, argv);
    // GLUT_MULTISAMPLE 若当前像素格式不支持，freeglut 会自动降级
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(opt.winW, opt.winH);
    glutInitWindowPosition(60, 40);
    glutCreateWindow("3D Model Viewer");

    // 要求 3：点窗口 X 关闭时，让 glutMainLoop 正常返回，而不是从回调里 exit()。
    // 默认的 GLUT_ACTION_EXIT 会跳过后面所有清理代码。
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    if      (opt.upAxis == "y" || opt.upAxis == "Y") g_app.modelUp = UpAxis::Y;
    else if (opt.upAxis == "z" || opt.upAxis == "Z") g_app.modelUp = UpAxis::Z;
    else                                             g_app.modelUp = UpAxis::Auto;

    // 默认是森林（见 AppState::sceneKind），所以 --scene 要正反两个方向都能切。
    // 只判断 "forest" 的话，默认改成森林之后 --scene studio 就落空了。
    if (opt.sceneName.empty() || opt.sceneName == "forest")
    {
        g_app.sceneKind = SceneKind::Forest;
        g_app.fog       = true;
    }
    else if (opt.sceneName == "studio")
    {
        g_app.sceneKind = SceneKind::Studio;
        g_app.fog       = false;
    }
    else
    {
        std::printf("[main] unknown --scene '%s' (expected: forest | studio)\n",
                    opt.sceneName.c_str());
        return 1;
    }

    g_app.enhanced = !opt.classic;
    g_app.shadows = !opt.noShadows;
    appSetup(opt.modelPath);
    glutCloseFunc(appCleanup);

    // ---- 命令行覆盖（必须在 glutCreateWindow 之后，appUpdateTitle 需要当前窗口）----
    if      (opt.proj == "persp")                          appSetProjection(ProjMode::Perspective);
    else if (opt.proj == "ortho")                          appSetProjection(ProjMode::Ortho);
    else if (opt.proj == "cav" || opt.proj == "cavalier")  appSetProjection(ProjMode::Cavalier);
    else if (opt.proj == "cab" || opt.proj == "cabinet")   appSetProjection(ProjMode::Cabinet);

    if      (opt.view == "front")  appSetViewPreset(ViewPreset::Front);
    else if (opt.view == "back")   appSetViewPreset(ViewPreset::Back);
    else if (opt.view == "left")   appSetViewPreset(ViewPreset::Left);
    else if (opt.view == "right")  appSetViewPreset(ViewPreset::Right);
    else if (opt.view == "top")    appSetViewPreset(ViewPreset::Top);
    else if (opt.view == "bottom") appSetViewPreset(ViewPreset::Bottom);

    if      (opt.render == "solid")  g_app.renderMode = RenderMode::Solid;
    else if (opt.render == "wire")   g_app.renderMode = RenderMode::Wireframe;
    else if (opt.render == "points") g_app.renderMode = RenderMode::Points;

    // 机器人定格姿势：snap=true 立即到位，headless 单帧出图等不到插值收敛
    if (!opt.robotPose.empty())
    {
        RobotPose p;
        if (robotParsePose(opt.robotPose, p)) robotSetPose(p, /*snap=*/true);
        else
            std::printf("[main] unknown --pose '%s' (expected: stand | wave | tpose | walk)\n",
                        opt.robotPose.c_str());
    }
    if (opt.robotWalk) robotToggleWalk();               // 行走循环（供 --time 出序列定格）
    if (opt.robotPath) robotSetPathWalk(true);          // 贝塞尔路径漫步
    if (opt.pathGuide) robotSetPathGuide(true);         // 曲线/控制点可视化
    if (opt.robotTime >= 0.0f) robotSetTimeOverride(opt.robotTime);

    // 出图机位覆盖（在 appSetup 的默认机位之后生效）
    if (opt.hasEye || opt.hasTarget)
    {
        g_app.viewPreset = ViewPreset::Free;
        if (opt.hasEye)    g_app.camera.eye    = v3(opt.eye[0], opt.eye[1], opt.eye[2]);
        if (opt.hasTarget) g_app.camera.target = v3(opt.target[0], opt.target[1], opt.target[2]);
    }

    if (opt.quad)     g_app.quadView = true;
    if (opt.noHud)    g_app.showHUD  = false;
    if (opt.showHelp) g_app.showHelp = true;

    glutDisplayFunc(appDisplay);
    glutReshapeFunc(appReshape);
    glutKeyboardFunc(inputKeyboard);
    glutSpecialFunc(inputSpecial);
    glutMouseFunc(inputMouseButton);
    glutMotionFunc(inputMouseMotion);
    glutMouseWheelFunc(inputMouseWheel);

    inputBuildMenu();
    appUpdateTitle();

    if (!opt.shotPath.empty())
    {
        // 直接画一帧再截图，**不进主循环**：freeglut 对隐藏窗口不派发 display 事件，
        // 用 glutMainLoop 会一直等下去（实测会挂死）。GL 上下文在 glutCreateWindow
        // 之后就已经是当前的，直接调显示函数渲染 + 从后缓冲读取即可。
        glutHideWindow();                        // 批处理出图时不闪窗口
        // 隐藏窗口永远收不到 reshape 事件（glutReshapeFunc 不会触发），必须显式灌尺寸，
        // 否则 --size 会被静默忽略、winW/winH 停在默认 1280x800。
        //
        // 而且要用 glutGet 问窗口的**真实**尺寸而不是请求值：Windows 会把窗口尺寸
        // 截断到屏幕大小（例如请求 300x1600 实际只有 300x1080），按请求值截图会让
        // glReadPixels 读到 framebuffer 之外，图顶多出一条黑边；aspect 也会算错。
        const int realW = glutGet(GLUT_WINDOW_WIDTH);
        const int realH = glutGet(GLUT_WINDOW_HEIGHT);
        appReshape(realW, realH);
        appRequestShot(opt.shotPath.c_str());
        std::printf("[main] headless shot mode -> %s  (requested %dx%d, actual %dx%d)\n",
                    opt.shotPath.c_str(), opt.winW, opt.winH, realW, realH);
        appDisplay();                            // 内部：画完 -> 截图 -> 退出主循环
        appCleanup();
        return appExitCode();
    }

    // ---- 视频录制 ----
    // 走 glutMainLoop 而不是像 --shot 那样直接调 appDisplay：录制要靠
    // glutPostRedisplay 逐帧驱动，需要一个真正的主循环。
    if (!opt.recordPath.empty())
    {
        RecordOptions ro;
        ro.path    = opt.recordPath;
        ro.fps     = opt.fps;
        ro.seconds = opt.seconds;

        if (opt.animName.empty())
        {
            ro.anim = AnimKind::Turntable;      // 默认：模型自转
        }
        else if (!parseAnimKind(opt.animName, ro.anim))
        {
            std::printf("[main] unknown --anim '%s'; falling back to turntable\n"
                        "       valid: turntable | orbit | projections | transforms\n",
                        opt.animName.c_str());
            ro.anim = AnimKind::Turntable;
        }

        // Offline rendering must not depend on visibility-driven GLUT events.
        glutHideWindow();
        appReshape(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
        appStartRecording(ro);
        while (appRecordingActive()) appDisplay();
        appCleanup();
        return appExitCode();
    }
    else if (!opt.animName.empty())
    {
        // 只给 --anim 不给 --record：以前是静默忽略，现在当作实时预览播放
        AnimKind k = AnimKind::None;
        if (parseAnimKind(opt.animName, k) && k != AnimKind::None)
            appStartPreview(k, opt.seconds);
        else
            std::printf("[main] unknown --anim '%s'\n"
                        "       valid: turntable | orbit | projections | transforms | roam\n",
                        opt.animName.c_str());
    }

    std::printf("[main] entering main loop  (h = help, ESC = quit)\n");
    glutMainLoop();

    // 走到这里说明是正常退出（ESC 或关闭窗口），清理代码会真正执行
    appCleanup();
    std::printf("[main] clean exit\n");
    return appExitCode();
}
