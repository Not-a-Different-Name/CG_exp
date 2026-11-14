#include "robot.h"
#include "scene.h"
#include "bezier.h"
#include "math3d.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// 部件清单。枚举顺序即创建顺序。
// ---------------------------------------------------------------------------
enum RobotPartId
{
    P_Torso, P_Head, P_EyeL, P_EyeR, P_Antenna,
    P_ArmL, P_ArmR, P_LegL, P_LegR,
    P_Count
};

// 所有关节角（度）+ 身体起伏（世界单位）的集中描述。
struct RobotJoints
{
    float armLiftL = 0, armLiftR = 0;     // 肩部侧抬，向外为正
    float armSwingL = 0, armSwingR = 0;   // 肩部前后摆
    float legSwingL = 0, legSwingR = 0;   // 髋部前后摆
    float headYaw = 0;                    // 头部偏航
    float bodyBob = 0;                    // 整体重心起伏
};

// 骨架尺寸（世界单位，脚底 y=0，机器人总高约 2.35）。
// 关节位置是「关节链的平移」，部件中心偏移是「杆件沿 -Y 的悬挂偏移」。
constexpr float kHipX = 0.16f,  kHipY = 1.08f;    // 髋关节
constexpr float kLegOffY = -0.54f;                // 腿中心挂在髋下（腿长 1.08，脚底到 0）
constexpr float kShoulderX = 0.36f, kShoulderY = 1.66f;
constexpr float kArmOffY = -0.31f;                // 臂中心挂在肩下（臂长 0.62）
constexpr float kNeckY = 1.82f, kHeadOffY = 0.16f;
constexpr float kTorsoY = 1.42f;

// 机器人面朝 +Z，出生时朝向默认机位方向（appSetup 的森林机位在 +X+Z 象限）。
constexpr float kRootYawDeg = 32.0f;
constexpr float kRootSpawn[3] = { 0.0f, 0.0f, 0.4f };

// 贝塞尔漫步路径：绕林间空地一圈（半径约 2 单位），速度恒定。
constexpr float kPathSpeed = 1.05f;      // 世界单位/秒

struct RobotCtl
{
    bool      exists   = false;
    bool      walking  = false;
    RobotPose pose     = RobotPose::Stand;
    int       root     = -1;                       // "Robot" 根物体（接收整体变换）
    int       part[P_Count] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    RobotJoints j;                                 // 当前关节角（向目标平滑逼近）
    float     lastT = -1.0f;
    float     tOverride = -1.0f;                   // --time：锁定时间轴出图用

    bool      onPath    = false;                   // 沿贝塞尔曲线漫步
    bool      wasWalking = false;                  // 开路径前的走路开关（关路径时还原）
    bool      showPath  = false;                   // 显示曲线/锚点/控制柄
    float     yawDeg    = kRootYawDeg;             // 根朝向（路径行走时随切线转动）
    int       guide     = -1;                      // "Path Guide" 物体
};
RobotCtl g_robot;

// ---------------------------------------------------------------------------
// 长方体部件：中心在原点、半尺寸 (hx,hy,hz)、整块单一颜色。
// 与 forest.cpp 的 pushTri 同一套做法，这里自带一份（各 30 行，不值得跨模块抽公共）。
// ---------------------------------------------------------------------------
void pushBoxTri(MeshData& m, const Vec3& a, const Vec3& b, const Vec3& c, const float col[3])
{
    const Vec3 n = v3norm(v3cross(b - a, c - a));
    const GLuint base = (GLuint)(m.positions.size() / 3);
    const Vec3* pts[3] = { &a, &b, &c };
    for (int i = 0; i < 3; ++i)
    {
        m.positions.push_back(pts[i]->x);
        m.positions.push_back(pts[i]->y);
        m.positions.push_back(pts[i]->z);
        m.normals.push_back(n.x);
        m.normals.push_back(n.y);
        m.normals.push_back(n.z);
        m.colors.push_back(col[0]);
        m.colors.push_back(col[1]);
        m.colors.push_back(col[2]);
    }
    m.indices.push_back(base);
    m.indices.push_back(base + 1);
    m.indices.push_back(base + 2);
}

void addBox(MeshData& m, float hx, float hy, float hz, const float col[3])
{
    // 每个面：中心 = n*half(n轴)，面上两个切向 u/v 满足 u×v = n（外法线），
    // 四角按 (-u-v)(+u-v)(+u+v)(-u+v) 的 CCW 顺序拆两个三角形。
    struct Face { Vec3 n, u, v; float hn, hu, hv; };
    const Face faces[6] = {
        { v3( 1,0,0), v3(0,0,-1), v3(0,1,0),  hx, hz, hy },
        { v3(-1,0,0), v3(0,0, 1), v3(0,1,0),  hx, hz, hy },
        { v3(0, 1,0), v3(1,0, 0), v3(0,0,-1), hy, hx, hz },
        { v3(0,-1,0), v3(1,0, 0), v3(0,0, 1), hy, hx, hz },
        { v3(0,0, 1), v3(1,0, 0), v3(0,1,0),  hz, hx, hy },
        { v3(0,0,-1), v3(-1,0,0), v3(0,1,0),  hz, hx, hy },
    };
    for (const Face& f : faces)
    {
        const Vec3 c = f.n * f.hn;
        const Vec3 u = f.u * f.hu;
        const Vec3 v = f.v * f.hv;
        const Vec3 p00 = c - u - v, p10 = c + u - v, p11 = c + u + v, p01 = c - u + v;
        pushBoxTri(m, p00, p10, p11, col);
        pushBoxTri(m, p00, p11, p01, col);
    }
    m.hasNormals = true;
    m.hasColors  = true;
}

// 把完整的部件世界矩阵拆成 Object 的 pos + xform（两条渲染路径共用同一组合规则）。
void writePart(Object& o, const float m[16])
{
    o.pos[0] = m[12];
    o.pos[1] = m[13];
    o.pos[2] = m[14];
    std::memcpy(o.xform, m, sizeof(float) * 16);
    o.xform[12] = o.xform[13] = o.xform[14] = 0.0f;
}

// ---------------------------------------------------------------------------
// 姿势目标与走路循环（全部是 t 的纯函数）
// ---------------------------------------------------------------------------
RobotJoints poseTarget(RobotPose p, float t)
{
    RobotJoints j;
    switch (p)
    {
    case RobotPose::Wave:
        j.armLiftR = 150.0f + 16.0f * std::sin(t * 6.5f);   // 高举过头左右挥
        j.armLiftL = 6.0f;
        j.headYaw  = 8.0f;                                  // 稍微看向挥手侧
        break;
    case RobotPose::TPose:
        j.armLiftL = j.armLiftR = 90.0f;                    // 双臂水平伸直
        break;
    case RobotPose::WalkFreeze:                             // 行走定格：四肢前后交错
        j.legSwingL = 30.0f;  j.legSwingR = -30.0f;
        j.armSwingL = -22.0f; j.armSwingR = 22.0f;
        j.bodyBob   = 0.03f;
        break;
    case RobotPose::Stand:
    default:
        j.armLiftL = j.armLiftR = 4.0f;                     // 自然下垂略外张
        j.bodyBob  = 0.012f * std::sin(t * 1.6f);           // 呼吸般的微起伏
        break;
    }
    return j;
}

RobotJoints walkTarget(float t)
{
    // 走路循环：双腿相位相反（评分点「两条腿交换向前」），双臂与同侧腿反相摆动，
    // 重心每周期起伏两次（双脚各落地一次）。
    constexpr float kCycleHz = 1.6f;
    const float w = 2.0f * kPi * kCycleHz * t;

    RobotJoints j;
    const float s = std::sin(w);
    j.legSwingL  =  32.0f * s;
    j.legSwingR  = -32.0f * s;
    j.armSwingL  = -24.0f * s;
    j.armSwingR  =  24.0f * s;
    j.armLiftL   = j.armLiftR = 6.0f;
    j.headYaw    = 3.0f * std::sin(w * 0.5f);
    j.bodyBob    = 0.05f * (0.5f - 0.5f * std::cos(2.0f * w));
    return j;
}

// 四肢关节链：Mbase · T(关节) · R(关节角) · T(杆件偏移)。
// 左臂侧抬绕 Z 的符号与右臂相反（左臂在 -X 侧，+Rz 会把手臂甩向身体内侧）。
void composeLimb(float out[16], const float Mbase[16],
                 float jx, float jy, float liftDeg, float swingDeg,
                 float offY, bool mirrorLift)
{
    float Tj[16], Rz[16], Rx[16], To[16], tmp[16];
    mat4FromTranslation(Tj, jx, jy, 0.0f);
    mat4FromAxisAngle(Rz, mirrorLift ? -liftDeg : liftDeg, 0.0f, 0.0f, 1.0f);
    mat4FromAxisAngle(Rx, swingDeg, 1.0f, 0.0f, 0.0f);
    mat4FromTranslation(To, 0.0f, offY, 0.0f);

    mat4Multiply(tmp, Mbase, Tj);
    float tmp2[16];
    mat4Multiply(tmp2, tmp, Rz);
    mat4Multiply(tmp, tmp2, Rx);
    mat4Multiply(out, tmp, To);
}

// ---------------------------------------------------------------------------
// 贝塞尔漫步路径的锚点（y 恒为 0，曲线贴地）。
// 顺序绕空地一圈；相邻锚点间距不完全相等，正好能体现「按弧长而不是按 u
// 参数化」的必要性 —— 用 u 均匀走会在长短段之间忽快忽慢。
// ---------------------------------------------------------------------------
BezierPath& pathStorage()
{
    static BezierPath p;
    static bool built = false;
    if (!built)
    {
        // 圆心略偏 +Z（0, 0.4）：避开右侧兔子（3.3,-2.6）与左侧鹿圈
        //（-4.2,0.6,r1.5），同时整圈仍在默认机位画面内。
        const Vec3 anchors[] = {
            v3( 1.70f, 0.0f,  0.40f),
            v3( 1.20f, 0.0f,  1.60f),
            v3( 0.00f, 0.0f,  2.10f),
            v3(-1.20f, 0.0f,  1.60f),
            v3(-1.70f, 0.0f,  0.40f),
            v3(-1.20f, 0.0f, -0.80f),
            v3( 0.00f, 0.0f, -1.30f),
            v3( 1.20f, 0.0f, -0.80f),
        };
        p.buildClosed(std::vector<Vec3>(anchors, anchors + sizeof(anchors) / sizeof(anchors[0])));
        built = true;
    }
    return p;
}

} // namespace

// ---------------------------------------------------------------------------
// 创建
// ---------------------------------------------------------------------------
void robotCreate(Scene& scene)
{
    if (g_robot.exists) return;
    RobotCtl& R = g_robot;

    // (半尺寸, 颜色)。明亮低多边形配色：暖橙躯干 + 奶油头 + 钢蓝四肢。
    struct PartDef { const char* name; float hx, hy, hz; float col[3]; };
    const PartDef defs[P_Count] = {
        { "Robot Torso",  0.28f, 0.36f, 0.17f, { 0.93f, 0.35f, 0.22f } },
        { "Robot Head",   0.19f, 0.17f, 0.18f, { 0.96f, 0.90f, 0.78f } },
        { "Robot Eye L",  0.035f, 0.05f, 0.015f, { 0.05f, 0.06f, 0.08f } },
        { "Robot Eye R",  0.035f, 0.05f, 0.015f, { 0.05f, 0.06f, 0.08f } },
        { "Robot Antenna", 0.025f, 0.10f, 0.025f, { 0.98f, 0.78f, 0.22f } },
        { "Robot Arm L",  0.08f, 0.31f, 0.08f, { 0.35f, 0.47f, 0.60f } },
        { "Robot Arm R",  0.08f, 0.31f, 0.08f, { 0.35f, 0.47f, 0.60f } },
        { "Robot Leg L",  0.11f, 0.54f, 0.12f, { 0.23f, 0.27f, 0.33f } },
        { "Robot Leg R",  0.11f, 0.54f, 0.12f, { 0.23f, 0.27f, 0.33f } },
    };

    for (int i = 0; i < P_Count; ++i)
    {
        MeshData d;
        addBox(d, defs[i].hx, defs[i].hy, defs[i].hz, defs[i].col);
        auto mesh = std::make_shared<Mesh>();
        mesh->adopt(std::move(d));
        scene.meshes.push_back(mesh);

        Object o;
        o.name      = defs[i].name;
        o.kind      = PrimKind::Mesh;
        o.mesh      = mesh;
        o.selectable = false;                          // Tab 只在 兔子/Robot根 之间切换
        mat4Identity(o.xform);
        scene.objects.push_back(o);
        R.part[i] = (int)scene.objects.size() - 1;
    }

    // 根物体：不绘制（mesh 为空 drawOne 直接跳过），只承载用户的整体变换。
    {
        Object o;
        o.name  = "Robot";
        o.kind  = PrimKind::Mesh;                      // mesh 留空 -> 不画
        o.pos[0] = kRootSpawn[0]; o.pos[1] = kRootSpawn[1]; o.pos[2] = kRootSpawn[2];
        mat4Identity(o.xform);
        scene.objects.push_back(o);
        R.root = (int)scene.objects.size() - 1;
    }

    // 贝塞尔路径可视化：曲线本体 + 锚点 + 控制柄合成一个网格，默认隐藏。
    {
        MeshData d;
        const float curveCol [3] = { 0.25f, 0.85f, 0.95f };
        const float anchorCol[3] = { 1.00f, 0.78f, 0.20f };
        const float handleCol[3] = { 0.62f, 0.72f, 0.88f };
        buildBezierVisual(d, pathStorage(), curveCol, anchorCol, handleCol);

        auto mesh = std::make_shared<Mesh>();
        mesh->adopt(std::move(d));
        scene.meshes.push_back(mesh);

        Object o;
        o.name       = "Path Guide";
        o.kind       = PrimKind::Mesh;
        o.mesh       = mesh;
        o.selectable = false;
        o.visible    = false;
        mat4Identity(o.xform);
        scene.objects.push_back(o);
        R.guide = (int)scene.objects.size() - 1;
    }

    R.j     = poseTarget(RobotPose::Stand, 0.0f);
    R.lastT = -1.0f;
    R.exists = true;
    std::printf("[robot] created: %d box parts, height ~2.35, path length %.2f\n",
                P_Count, pathStorage().total);
}

// ---------------------------------------------------------------------------
// 每帧更新：关节平滑逼近目标 -> 组层级矩阵 -> 写回部件
// ---------------------------------------------------------------------------
void robotUpdate(Scene& scene, float t)
{
    if (!g_robot.exists) return;
    RobotCtl& R = g_robot;

    if (R.tOverride >= 0.0f)
    {
        // 时间轴锁定（--time 出图）：直接取该时刻目标，跳过平滑
        t = R.tOverride;
        R.j = R.walking ? walkTarget(t) : poseTarget(R.pose, t);
    }
    else
    {
    float dt = (R.lastT < 0.0f) ? 0.0f : (t - R.lastT);
    R.lastT = t;
    if (dt < 0.0f) dt = 0.0f;                          // 时间倒退（新录制开始）时保持现状
    if (dt > 0.05f) dt = 0.05f;

    const RobotJoints target = R.walking ? walkTarget(t) : poseTarget(R.pose, t);
    const float k = 1.0f - std::exp(-10.0f * dt);      // 指数平滑，切姿势不跳变
    struct { float& cur; float tgt; } blend[] = {
        { R.j.armLiftL,  target.armLiftL  }, { R.j.armLiftR,  target.armLiftR  },
        { R.j.armSwingL, target.armSwingL }, { R.j.armSwingR, target.armSwingR },
        { R.j.legSwingL, target.legSwingL }, { R.j.legSwingR, target.legSwingR },
        { R.j.headYaw,   target.headYaw   }, { R.j.bodyBob,   target.bodyBob   },
    };
    for (auto& b : blend) b.cur += (b.tgt - b.cur) * k;
    }

    // ---- 路径可视化开关（同步到物体，headless 单帧出图也能生效）----
    if (R.guide >= 0)
        scene.objects[R.guide].visible = R.showPath;

    // ---- W3-1：沿贝塞尔闭曲线漫步 ----
    // 位置用「弧长」求值（evalAtDistance），所以速度恒定；朝向取切线方向。
    // 机器人模型面朝 +Z，Ry(yaw) 把 +Z 转到 (sin yaw, 0, cos yaw)，
    // 因此 yaw = atan2(tangent.x, tangent.z)。
    if (R.onPath)
    {
        const float s = kPathSpeed * t;
        const Vec3 p   = pathStorage().evalAtDistance(s);
        const Vec3 tan = pathStorage().tangentAtDistance(s);

        Object& root = scene.objects[R.root];
        root.pos[0] = p.x;
        root.pos[1] = 0.0f;
        root.pos[2] = p.z;

        R.yawDeg = std::atan2(tan.x, tan.z) * 180.0f / kPi;
        R.walking = true;                          // 沿路径走，腿当然要动
    }

    // ---- 根：T(出生/用户平移) · 用户累积变换 · Ry(朝向) · T(重心起伏) ----
    Object& root = scene.objects[R.root];
    float Tpos[16], A[16], Ry[16], Tbob[16], Mbase[16];
    mat4FromTranslation(Tpos, root.pos[0], root.pos[1], root.pos[2]);
    mat4Multiply(A, Tpos, root.xform);
    mat4FromAxisAngle(Ry, R.yawDeg, 0.0f, 1.0f, 0.0f);
    mat4Multiply(A, A, Ry);
    mat4FromTranslation(Tbob, 0.0f, R.j.bodyBob, 0.0f);
    mat4Multiply(Mbase, A, Tbob);

    // ---- 躯干 ----
    {
        float Tt[16], M[16];
        mat4FromTranslation(Tt, 0.0f, kTorsoY, 0.0f);
        mat4Multiply(M, Mbase, Tt);
        writePart(scene.objects[R.part[P_Torso]], M);
    }

    // ---- 头 + 天线 + 双眼（头链再挂子部件，展示层级可嵌套）----
    {
        float Tn[16], Rh[16], H[16], T[16], M[16];
        mat4FromTranslation(Tn, 0.0f, kNeckY, 0.0f);
        mat4FromAxisAngle(Rh, R.j.headYaw, 0.0f, 1.0f, 0.0f);
        mat4Multiply(H, Mbase, Tn);
        mat4Multiply(H, H, Rh);

        mat4FromTranslation(T, 0.0f, kHeadOffY, 0.0f);
        mat4Multiply(M, H, T);
        writePart(scene.objects[R.part[P_Head]], M);

        mat4FromTranslation(T, 0.0f, 0.43f, -0.06f);   // 头顶天线（随头转）
        mat4Multiply(M, H, T);
        writePart(scene.objects[R.part[P_Antenna]], M);

        mat4FromTranslation(T, -0.09f, 0.20f, 0.19f);  // 脸在 +Z，双眼微凸出脸面
        mat4Multiply(M, H, T);
        writePart(scene.objects[R.part[P_EyeL]], M);
        mat4FromTranslation(T, 0.09f, 0.20f, 0.19f);
        mat4Multiply(M, H, T);
        writePart(scene.objects[R.part[P_EyeR]], M);
    }

    // ---- 四肢 ----
    {
        float M[16];
        composeLimb(M, Mbase, -kShoulderX, kShoulderY, R.j.armLiftL, R.j.armSwingL, kArmOffY, /*mirrorLift=*/true);
        writePart(scene.objects[R.part[P_ArmL]], M);
        composeLimb(M, Mbase,  kShoulderX, kShoulderY, R.j.armLiftR, R.j.armSwingR, kArmOffY, false);
        writePart(scene.objects[R.part[P_ArmR]], M);

        // 腿没有侧抬角，liftDeg 恒 0，链退化为 T(髋)·Rx(摆)·T(偏移)
        composeLimb(M, Mbase, -kHipX, kHipY, 0.0f, R.j.legSwingL, kLegOffY, true);
        writePart(scene.objects[R.part[P_LegL]], M);
        composeLimb(M, Mbase,  kHipX, kHipY, 0.0f, R.j.legSwingR, kLegOffY, false);
        writePart(scene.objects[R.part[P_LegR]], M);
    }
}

// ---------------------------------------------------------------------------
// 姿势控制
// ---------------------------------------------------------------------------
void robotSetPose(RobotPose pose, bool snap)
{
    if (pose < RobotPose::Stand || pose >= RobotPose::Count) return;
    g_robot.pose = pose;
    g_robot.walking = false;                            // 定格姿势优先于走路循环
    if (snap) g_robot.j = poseTarget(pose, 0.0f);
}

void robotNextPose()
{
    int p = ((int)g_robot.pose + 1) % (int)RobotPose::Count;
    robotSetPose((RobotPose)p, false);
}

RobotPose robotPose()        { return g_robot.pose; }
bool       robotWalking()    { return g_robot.exists && g_robot.walking; }
bool       robotAlive()      { return g_robot.exists; }

const char* robotPoseName(RobotPose p)
{
    switch (p)
    {
    case RobotPose::Wave:      return "wave";
    case RobotPose::TPose:     return "T-pose";
    case RobotPose::WalkFreeze:return "walk-freeze";
    case RobotPose::Stand:
    default:                   return "stand";
    }
}

bool robotParsePose(const std::string& s, RobotPose& out)
{
    if      (s == "stand" || s == "idle")        out = RobotPose::Stand;
    else if (s == "wave")                        out = RobotPose::Wave;
    else if (s == "tpose" || s == "t-pose")      out = RobotPose::TPose;
    else if (s == "walk" || s == "walkfreeze"
          || s == "walk-freeze")                 out = RobotPose::WalkFreeze;
    else return false;
    return true;
}

void robotToggleWalk()
{
    if (!g_robot.exists) return;
    g_robot.walking = !g_robot.walking;
    if (g_robot.walking) g_robot.pose = RobotPose::WalkFreeze;  // 停下时停在行走姿态
}

// ---------------------------------------------------------------------------
// W3-1 贝塞尔路径漫步
// ---------------------------------------------------------------------------
const BezierPath& robotPath() { return pathStorage(); }
float robotPathSpeed()        { return kPathSpeed; }

bool robotOnPath()      { return g_robot.exists && g_robot.onPath; }
bool robotPathGuide()   { return g_robot.exists && g_robot.showPath; }

void robotSetPathWalk(bool on)
{
    if (!g_robot.exists) return;
    if (on == g_robot.onPath) return;

    g_robot.onPath = on;
    if (on)
    {
        // 记住进入路径前的走路状态，退出时还原（避免 P 键偷偷改掉 b 键的状态）
        g_robot.wasWalking = g_robot.walking;
        g_robot.walking    = true;
        g_robot.pose       = RobotPose::WalkFreeze;
    }
    else
    {
        g_robot.walking = g_robot.wasWalking;
    }
    std::printf("[robot] bezier path walk: %s\n", on ? "on" : "off");
}

void robotTogglePath() { robotSetPathWalk(!g_robot.onPath); }

void robotSetPathGuide(bool on)
{
    if (!g_robot.exists) return;
    g_robot.showPath = on;      // 物体可见性在下一帧 robotUpdate 里同步
    std::printf("[robot] path guide: %s\n", on ? "on" : "off");
}

void robotTogglePathGuide() { robotSetPathGuide(!g_robot.showPath); }

void robotSetTimeOverride(float t)
{
    g_robot.tOverride = t;
    g_robot.lastT = -1.0f;
}

bool robotPartPosition(const Scene& scene, const char* partName, float out[3])
{
    for (const Object& o : scene.objects)
    {
        if (o.name == partName)
        {
            out[0] = o.pos[0]; out[1] = o.pos[1]; out[2] = o.pos[2];
            return true;
        }
    }
    return false;
}
