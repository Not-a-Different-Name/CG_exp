#include "animals.h"
#include "scene.h"
#include "forest.h"       // 形状库：addCylinderSide / addCone / addEllipsoid / appendTransformed
#include "math3d.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

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
            place(out, p, 0.21f * fx, 0.50f, 0.115f * fz);
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
            place(out, p, 0.17f * fx, 0.22f, 0.085f * fz);
        }
    { MeshData p; addEllipsoid(p, v3(0, 0, 0), 0.17f, 0.065f, 0.065f, fur, 8, 5);
      place(out, p, -0.34f, 0.33f, 0.0f, 8.0f); }                   // 蓬松尾
    { MeshData p; addEllipsoid(p, v3(0, 0, 0), 0.055f, 0.055f, 0.055f, white, 6, 4);
      place(out, p, -0.49f, 0.36f, 0.0f); }                         // 白尾尖
}

// ---------------------------------------------------------------------------
// 行为参数
// ---------------------------------------------------------------------------
struct AnimalDef
{
    const char* name;
    float cx, cz;             // 路线圆心
    float radius;
    float angSpeed;           // 弧度/秒（负 = 逆时针反着走）
    float phase0;             // 起始角
    float grazePitchDeg;      // 吃草俯身角
};

const AnimalDef kDefs[] = {
    // 路线圆心/半径与 scene.cpp buildForestPath 的避让圆一致（树不挡动物）。
    // 两条路线都放在默认机位 (2.6,2.2,4.6) 的视野内、且避开兔子站位 (2.6,-1.6)：
    // 鹿在机器人左前方，狐狸在更左后方（右侧视野被兔子占住，狐狸走到右边会被挡）。
    // 旧的鹿圆心 (-4.5,3.2) 落在相机背后，狐狸圆又从兔子身上穿过去 ——
    // 截图里两只动物都看不见（2026-11-14 实测）。
    { "Deer", -3.5f,  0.5f, 1.8f,  0.10f, 0.6f,  9.0f },
    { "Fox",  -5.5f, -3.0f, 1.5f, -0.16f, 2.1f, 12.0f },
};

// 「走 11 秒 → 吃草 4 秒」：返回从 0 到 t 实际行走的弧长对应角（吃草时冻结）
float walkedAngle(float t, float angSpeed)
{
    constexpr float kWalk = 11.0f, kCycle = 15.0f;
    const float n    = std::floor(t / kCycle);
    const float rest = t - n * kCycle;                       // 本周期内时间
    const float walk = n * kWalk + std::min(rest, kWalk);
    return angSpeed * walk;
}

struct AnimalsCtl
{
    bool exists = false;
    int  obj[2] = { -1, -1 };
    char status[48] = "";
};
AnimalsCtl g_animals;

} // namespace

// ---------------------------------------------------------------------------
bool animalsCreate(Scene& scene)
{
    if (g_animals.exists) return true;

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

    for (int i = 0; i < 2; ++i)
    {
        const AnimalDef& d = kDefs[i];
        Object& o = scene.objects[g_animals.obj[i]];

        // 狐狸的作息整体错开 4 秒，避免两只动物同步低头吃草
        const float bt = t + (i == 1 ? 4.0f : 0.0f);

        const float ang  = d.phase0 + walkedAngle(bt, d.angSpeed);
        const float px   = d.cx + d.radius * std::cos(ang);
        const float pz   = d.cz + d.radius * std::sin(ang);

        // 朝向 = 路线切线（模型面朝 +X：Ry(θ) 把 +X 转到 (cosθ,0,-sinθ)）
        const float tang = ang + (d.angSpeed > 0.0f ? kPi * 0.5f : -kPi * 0.5f);
        const float hx   = -std::sin(tang), hz = std::cos(tang);   // 切向单位向量
        const float yawDeg = std::atan2(-hz, hx) * 180.0f / kPi;

        // 吃草窗口：俯身 + 高度略降；行走：轻微起伏
        constexpr float kWalk = 11.0f, kCycle = 15.0f;
        const bool grazing = std::fmod(bt, kCycle) > kWalk;

        float pitchDeg = 0.0f, bob = 0.0f;
        if (grazing)
        {
            pitchDeg = d.grazePitchDeg * (0.75f + 0.25f * std::sin(bt * 2.3f));  // 啄食起伏
            bob = -0.025f;
        }
        else
        {
            bob = 0.016f * (0.5f - 0.5f * std::cos(2.0f * walkedAngle(bt, d.angSpeed)));
        }

        float mYaw[16], mPitch[16];
        mat4FromAxisAngle(mYaw,   yawDeg,   0.0f, 1.0f, 0.0f);
        mat4FromAxisAngle(mPitch, pitchDeg, 0.0f, 0.0f, 1.0f);
        mat4Multiply(o.xform, mYaw, mPitch);

        o.pos[0] = px;
        o.pos[1] = bob;
        o.pos[2] = pz;
    }

    std::snprintf(g_animals.status, sizeof(g_animals.status), "deer %s / fox %s",
                  (std::fmod(t, 15.0f) > 11.0f) ? "grazing" : "walking",
                  (std::fmod(t + 4.0f, 15.0f) > 11.0f) ? "grazing" : "walking");
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
