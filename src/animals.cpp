#include "animals.h"
#include "scene.h"
#include "bezier.h"       // W3-2：动物的漫游路线同样是三次贝塞尔闭曲线
#include "forest.h"       // 形状库：addCylinderSide / addCone / addEllipsoid / appendTransformed
#include "math3d.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// 几何：面朝 +X，四脚踩 y=0。全部用「局部建好的部件 + 4x4 矩阵摆位」拼装。
// ---------------------------------------------------------------------------
void place(MeshData& out, MeshData& part, float tx, float ty, float tz,
           float pitchZDeg = 0.0f, float scale = 1.0f)
{
    float m[16];
    mat4Identity(m);
    if (pitchZDeg != 0.0f)
    {
        float r[16];
        mat4FromAxisAngle(r, pitchZDeg, 0.0f, 0.0f, 1.0f);
        mat4Multiply(m, r, m);
    }
    if (scale != 1.0f)
    {
        float s[16];
        mat4FromScale(s, scale, scale, scale);
        mat4Multiply(m, s, m);
    }
    m[12] = tx; m[13] = ty; m[14] = tz;
    appendTransformed(out, part, m);
}

// 鹿：棕色长腿，头顶小角，颈部前倾
void buildDeer(MeshData& out)
{
    const float body[3] = { 0.68f, 0.50f, 0.34f };
    const float dark[3] = { 0.48f, 0.35f, 0.23f };
    const float nose[3] = { 0.16f, 0.12f, 0.10f };

    { MeshData p; addEllipsoid(p, v3(0, 0, 0), 0.34f, 0.24f, 0.19f, body, 10, 6);
      place(out, p, 0.0f, 0.66f, 0.0f); }

    {   // 颈：竖圆柱绕 Z 前倾 42°，头在颈顶
        MeshData neck; addCylinderSide(neck, 0.0f, 0.40f, 0.070f, 0.050f, body, 6);
        place(out, neck, 0.30f, 0.74f, 0.0f, -42.0f);
    }
    {   // 头 + 口鼻 + 双耳 + 小角（都在颈部顶端附近）
        const float hx = 0.30f + 0.40f * std::sin(42.0f * kPi / 180.0f);
        const float hy = 0.74f + 0.40f * std::cos(42.0f * kPi / 180.0f);
        { MeshData p; addEllipsoid(p, v3(0, 0, 0), 0.11f, 0.09f, 0.09f, body, 8, 5);
          place(out, p, hx + 0.04f, hy, 0.0f, -10.0f); }
        { MeshData p; addEllipsoid(p, v3(0, 0, 0), 0.07f, 0.045f, 0.05f, nose, 6, 4);
          place(out, p, hx + 0.14f, hy - 0.02f, 0.0f); }
        for (int s = -1; s <= 1; s += 2)
        {
            { MeshData p; addCone(p, 0.0f, 0.12f, 0.035f, dark, 4);
              place(out, p, hx - 0.03f, hy + 0.07f, 0.065f * s, 18.0f); }
            { MeshData p; addCylinderSide(p, 0.0f, 0.16f, 0.014f, 0.010f, dark, 4);
              place(out, p, hx - 0.02f, hy + 0.06f, 0.055f * s, -30.0f); }
        }
    }
    for (int fx = -1; fx <= 1; fx += 2)          // 四条长腿
        for (int fz = -1; fz <= 1; fz += 2)
        {
            MeshData p; addCylinderSide(p, 0.0f, 0.50f, 0.036f, 0.028f, dark, 5);
            place(out, p, 0.21f * fx, 0.0f, 0.115f * fz);
        }
    { MeshData p; addCone(p, 0.0f, 0.16f, 0.045f, body, 5);   // 短尾
      place(out, p, -0.36f, 0.72f, 0.0f, 120.0f); }
}

// 狐狸：橙红身体、大耳、蓬松白尖尾
void buildFox(MeshData& out)
{
    const float fur[3]   = { 0.83f, 0.42f, 0.16f };
    const float white[3] = { 0.94f, 0.92f, 0.88f };
    const float dark[3]  = { 0.20f, 0.15f, 0.12f };

    { MeshData p; addEllipsoid(p, v3(0, 0, 0), 0.27f, 0.145f, 0.14f, fur, 10, 6);
      place(out, p, 0.0f, 0.31f, 0.0f); }
    { MeshData p; addEllipsoid(p, v3(0, 0, 0), 0.10f, 0.075f, 0.08f, fur, 8, 5);
      place(out, p, 0.30f, 0.40f, 0.0f, -8.0f); }
    { MeshData p; addCone(p, 0.0f, 0.11f, 0.038f, white, 5);       // 尖吻
      place(out, p, 0.40f, 0.385f, 0.0f, -95.0f); }
    for (int s = -1; s <= 1; s += 2)                                // 大耳
    {
        MeshData p; addCone(p, 0.0f, 0.13f, 0.045f, dark, 4);
        place(out, p, 0.27f, 0.47f, 0.055f * s, 16.0f);
    }
    for (int fx = -1; fx <= 1; fx += 2)                             // 短腿
        for (int fz = -1; fz <= 1; fz += 2)
        {
            MeshData p; addCylinderSide(p, 0.0f, 0.22f, 0.026f, 0.020f, dark, 4);
            place(out, p, 0.17f * fx, 0.0f, 0.085f * fz);
        }
    { MeshData p; addEllipsoid(p, v3(0, 0, 0), 0.17f, 0.065f, 0.065f, fur, 8, 5);
      place(out, p, -0.34f, 0.33f, 0.0f, 8.0f); }                   // 蓬松尾
    { MeshData p; addEllipsoid(p, v3(0, 0, 0), 0.055f, 0.055f, 0.055f, white, 6, 4);
      place(out, p, -0.49f, 0.36f, 0.0f); }                         // 白尾尖
}

// ---------------------------------------------------------------------------
// 行为参数
//
// 走路路线从「圆」升级成三次贝塞尔闭曲线（W3-2）：锚点手摆成不规则环，
// 更像动物自己踩出来的小径；同样用弧长参数化，速度恒定。
// ---------------------------------------------------------------------------
const Vec3 kDeerAnchors[] = {
    v3(-2.45f, 0.0f,  0.80f),
    v3(-3.05f, 0.0f,  1.95f),
    v3(-4.45f, 0.0f,  1.90f),
    v3(-5.05f, 0.0f,  0.75f),
    v3(-4.45f, 0.0f, -0.55f),
    v3(-3.05f, 0.0f, -0.65f),
};

const Vec3 kFoxAnchors[] = {
    v3(-4.20f, 0.0f, -2.90f),
    v3(-4.75f, 0.0f, -1.90f),
    v3(-5.75f, 0.0f, -2.05f),
    v3(-6.20f, 0.0f, -3.00f),
    v3(-5.55f, 0.0f, -3.75f),
    v3(-4.55f, 0.0f, -3.65f),
};

struct AnimalDef
{
    const char* name;
    const Vec3* anchors;
    int         anchorCount;
    float       walkSpeed;      // 弧长速度（世界单位/秒）
    float       fleeSpeed;      // 被追时的逃跑速度
    float       phase0;         // 起始弧长偏移
    float       grazePitchDeg;  // 吃草俯身角
};

const AnimalDef kDefs[] = {
    // 路线与 scene.cpp buildForestPath 的避让圆一致（树不挡动物）。
    // 鹿在机器人左前方，狐狸在更左后方；两圈互不重叠。
    { "Deer", kDeerAnchors, 6, 0.38f, 1.60f, 0.0f,  9.0f },
    { "Fox",  kFoxAnchors,  6, 0.50f, 1.80f, 2.0f, 12.0f },
};

// 路线在 create 时构建一次（锚点数组是常量）。
BezierPath g_loop[2];
bool       g_loopsBuilt = false;

// 「走 11 秒 → 吃草 4 秒」：返回从 0 到 t 实际行走的弧长（吃草时冻结）。
// alwaysWalk=true 用于逃跑：不休息、按逃跑速度一直跑。
float walkedDistance(float t, float speed, bool alwaysWalk)
{
    constexpr float kWalk = 11.0f, kCycle = 15.0f;
    if (alwaysWalk) return speed * t;

    const float n    = std::floor(t / kCycle);
    const float rest = t - n * kCycle;                       // 本周期内时间
    const float walk = n * kWalk + std::min(rest, kWalk);
    return speed * walk;
}

struct AnimalsCtl
{
    bool  exists = false;
    int   obj[2] = { -1, -1 };
    float heading[2][2] = { { 1, 0 }, { 1, 0 } };   // 当前前进方向（单位向量）
    bool  chase = false;                             // W3-2 追赶演示
    bool  hasTime = false;
    float lastTime = 0.0f;
    float deerDistanceOffset = 0.0f;
    char  status[64] = "";
};
AnimalsCtl g_animals;

} // namespace

// ---------------------------------------------------------------------------
bool animalsCreate(Scene& scene)
{
    if (g_animals.exists) return true;

    if (!g_loopsBuilt)
    {
        for (int i = 0; i < 2; ++i)
        {
            const AnimalDef& d = kDefs[i];
            g_loop[i].buildClosed(std::vector<Vec3>(d.anchors, d.anchors + d.anchorCount));
        }
        g_loopsBuilt = true;
    }

    for (int i = 0; i < 2; ++i)
    {
        MeshData data;
        if (i == 0) buildDeer(data);
        else        buildFox(data);

        auto mesh = std::make_shared<Mesh>();
        mesh->adopt(std::move(data));
        scene.meshes.push_back(mesh);

        Object o;
        o.name       = kDefs[i].name;
        o.kind       = PrimKind::Mesh;
        o.mesh       = mesh;
        o.selectable = false;
        mat4Identity(o.xform);
        scene.objects.push_back(o);
        g_animals.obj[i] = (int)scene.objects.size() - 1;
    }

    g_animals.exists = true;
    std::printf("[animals] created: Deer + Fox (procedural low-poly, ~%.1fk tris)\n",
                (scene.objects[g_animals.obj[0]].mesh->data().triangleCount +
                 scene.objects[g_animals.obj[1]].mesh->data().triangleCount) / 1000.0f);
    return true;
}

void animalsUpdate(Scene& scene, float t)
{
    if (!g_animals.exists) return;
    g_animals.hasTime = true;
    g_animals.lastTime = t;

    // 机器人位置（「警觉」判定）。场景里没有机器人时视为很远。
    bool  hasRobot = false;
    float rx = 0.0f, rz = 0.0f;
    for (const Object& o : scene.objects)
        if (o.name == "Robot") { hasRobot = true; rx = o.pos[0]; rz = o.pos[2]; break; }

    const char* state[2] = { "walking", "walking" };

    for (int i = 0; i < 2; ++i)
    {
        const AnimalDef& d = kDefs[i];
        Object& o = scene.objects[g_animals.obj[i]];

        // 狐狸的作息整体错开 4 秒，避免两只动物同步低头吃草
        const float bt = t + (i == 1 ? 4.0f : 0.0f);

        // 追赶演示：鹿被追时一直跑（不进入吃草窗口）
        const bool fleeing = g_animals.chase && (i == 0);
        const float speed  = fleeing ? d.fleeSpeed : d.walkSpeed;
        const float dist   = d.phase0 + walkedDistance(bt, speed, fleeing) +
                             (i == 0 ? g_animals.deerDistanceOffset : 0.0f);

        const Vec3 p   = g_loop[i].evalAtDistance(dist);
        const Vec3 tan = g_loop[i].tangentAtDistance(dist);
        const float hx = tan.x, hz = tan.z;          // 前进方向（单位向量）
        const float yawDeg = std::atan2(-hz, hx) * 180.0f / kPi;

        g_animals.heading[i][0] = hx;
        g_animals.heading[i][1] = hz;

        // 吃草窗口：俯身 + 高度略降；机器人靠得太近则警觉（抬头、不吃草）
        constexpr float kWalk = 11.0f, kCycle = 15.0f;
        const bool alert   = hasRobot && !fleeing &&
                             std::sqrt((p.x - rx) * (p.x - rx) + (p.z - rz) * (p.z - rz)) < 1.35f;
        const bool grazing = !fleeing && !alert && std::fmod(bt, kCycle) > kWalk;

        float pitchDeg = 0.0f, bob = 0.0f;
        if (grazing)
        {
            // 模型朝 +X，绕 Z 的负角才会低头。
            pitchDeg = -d.grazePitchDeg * (0.75f + 0.25f * std::sin(bt * 2.3f));
            state[i] = "grazing";
        }
        else
        {
            // 步频与走过的弧长挂钩：每 ~1 单位起伏一次
            bob = 0.016f * (0.5f - 0.5f * std::cos(6.0f * dist));
            if (fleeing)   state[i] = "fleeing";
            else if (alert) state[i] = "alert";
        }

        float mYaw[16], mPitch[16];
        mat4FromAxisAngle(mYaw,   yawDeg,   0.0f, 1.0f, 0.0f);
        mat4FromAxisAngle(mPitch, pitchDeg, 0.0f, 0.0f, 1.0f);
        mat4Multiply(o.xform, mYaw, mPitch);

        // 整体俯身会使一侧脚落到地面以下；按实际网格最低点补偿高度。
        if (grazing)
        {
            float minY = 0.0f;
            const auto& vertices = o.mesh->data().positions;
            for (size_t v = 0; v < vertices.size(); v += 3)
                minY = std::min(minY, mat4MulPoint(mPitch,
                    v3(vertices[v], vertices[v + 1], vertices[v + 2])).y);
            bob = -minY;
        }

        o.pos[0] = p.x;
        o.pos[1] = bob;
        o.pos[2] = p.z;
    }

    std::snprintf(g_animals.status, sizeof(g_animals.status), "deer %s / fox %s",
                  state[0], state[1]);
}
// ---------------------------------------------------------------------------
// W3-2 追赶演示：鹿切换成逃跑状态，机器人跟在鹿身后 1.25 单位。
// ---------------------------------------------------------------------------
bool animalsChase() { return g_animals.exists && g_animals.chase; }

void animalsSetChase(bool on)
{
    if (!g_animals.exists || on == g_animals.chase) return;
    // 只改变后续速度，不把 speed*t 的历史路程整体换算，否则切换会瞬移。
    if (g_animals.hasTime)
    {
        const AnimalDef& deer = kDefs[0];
        const float t = g_animals.lastTime;
        g_animals.deerDistanceOffset +=
            walkedDistance(t, g_animals.chase ? deer.fleeSpeed : deer.walkSpeed, g_animals.chase) -
            walkedDistance(t, on ? deer.fleeSpeed : deer.walkSpeed, on);
    }
    g_animals.chase = on;
    std::printf("[animals] chase demo: %s\n", on ? "on (deer flees)" : "off");
}

void animalsToggleChase() { animalsSetChase(!g_animals.chase); }

bool animalsChaseTarget(const Scene& scene, float outPos[3], float* yawDeg)
{
    if (!g_animals.exists || g_animals.obj[0] < 0 ||
        g_animals.obj[0] >= (int)scene.objects.size())
        return false;

    const Object& deer = scene.objects[g_animals.obj[0]];
    const float hx = g_animals.heading[0][0], hz = g_animals.heading[0][1];

    // 站在鹿的正后方，朝同一个方向追
    outPos[0] = deer.pos[0] - hx * 1.25f;
    outPos[1] = 0.0f;
    outPos[2] = deer.pos[2] - hz * 1.25f;

    // 注意：动物模型面朝 +X，机器人模型面朝 +Z，两者的 yaw 公式不同。
    // 动物用 atan2(-hz, hx)；机器人 Ry(θ) 把 +Z 转到 (sinθ, cosθ)，
    // 所以这里必须是 atan2(hx, hz)。用错会让机器人一边追一边背对鹿。
    if (yawDeg) *yawDeg = std::atan2(hx, hz) * 180.0f / kPi;
    return true;
}

bool animalsAlive()               { return g_animals.exists; }
const char* animalsStatusText()   { return g_animals.status; }

bool animalPartPosition(const Scene& scene, const char* name, float out[3])
{
    for (const Object& o : scene.objects)
        if (o.name == name)
        {
            out[0] = o.pos[0]; out[1] = o.pos[1]; out[2] = o.pos[2];
            return true;
        }
    return false;
}
