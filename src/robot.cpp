#include "robot.h"
#include "scene.h"
#include "bezier.h"
#include "math3d.h"
#include "texture.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
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
using PartMeshes = std::array<std::shared_ptr<Mesh>, P_Count>;
enum class CharacterLook { BuiltIn, Skin, Model };

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
    PartMeshes originalMeshes;
    PartMeshes skinMeshes;
    std::shared_ptr<Mesh> importedModel;
    std::string skinLabel;
    std::string modelLabel;
    CharacterLook look = CharacterLook::BuiltIn;
    int importedObject = -1;
    RobotJoints j;                                 // 当前关节角（向目标平滑逼近）
    float     lastT = -1.0f;
    float     tOverride = -1.0f;                   // --time：锁定时间轴出图用

    bool      onPath    = false;                   // 沿贝塞尔曲线漫步
    float     pathTime  = 0.0f;
    float     pathTick  = -1.0f;
    bool      chase     = false;                   // 追赶演示：外部指定的位姿优先
    float     chaseX = 0.0f, chaseZ = 0.0f, chaseYaw = 0.0f;
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

void addBox(MeshData& m, float hx, float hy, float hz, const float col[3],
            Vec3 offset = v3(0.0f, 0.0f, 0.0f))
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
        const Vec3 c = offset + f.n * f.hn;
        const Vec3 u = f.u * f.hu;
        const Vec3 v = f.v * f.hv;
        const Vec3 p00 = c - u - v, p10 = c + u - v, p11 = c + u + v, p01 = c - u + v;
        pushBoxTri(m, p00, p10, p11, col);
        pushBoxTri(m, p00, p11, p01, col);
    }
    m.hasNormals = true;
    m.hasColors  = true;
}

// 把任意受支持的皮肤规格归一化成 64x64 RGBA（行 0 = 图像顶部）：
//   64x64 现代皮肤：原样使用；
//   64x32 旧版皮肤：左臂/左腿在旧格式里不存在，由右臂/右腿水平镜像补到
//     64x64 的左肢区域 (32,48)/(16,48)，overlay 区保持透明；
//   HD 皮肤（128x128 等）：每个 64 网格块取中心像素，最近邻降采样。
void normalizeSkin(const std::vector<unsigned char>& src, int w, int h,
                   std::vector<unsigned char>& out)
{
    std::vector<unsigned char> pixels((size_t)64 * 64 * 4, 0);
    if (w == 64 && h == 64)
    {
        pixels = src;
    }
    else if (w == 64 && h == 32)
    {
        std::copy(src.begin(), src.end(), pixels.begin());
        auto mirror = [&](int sx, int sy, int rw, int rh, int dx, int dy)
        {
            for (int row = 0; row < rh; ++row)
                for (int col = 0; col < rw; ++col)
                {
                    const int s = ((sy + row) * 64 + sx + col) * 4;
                    const int d = ((dy + row) * 64 + dx + (rw - 1 - col)) * 4;
                    for (int c = 0; c < 4; ++c) pixels[d + c] = src[s + c];
                }
        };
        mirror(40, 16, 12, 16, 32, 48);   // 右臂 net -> 左臂
        mirror( 0, 16, 12, 16, 16, 48);   // 右腿 net -> 左腿
    }
    else // HD
    {
        const int s = w / 64;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
            {
                const int sx = x * s + s / 2, sy = y * s + s / 2;
                const size_t si = ((size_t)sy * w + sx) * 4;
                const size_t di = ((size_t)y * 64 + x) * 4;
                for (int c = 0; c < 4; ++c) pixels[di + c] = src[si + c];
            }
    }
    out.swap(pixels);
}

struct SkinBox
{
    RobotPartId part;
    int x, y, overlayX, overlayY;
    int width, height, depth;
    float hx, hy, hz;
};

std::shared_ptr<Mesh> buildSkinBox(const SkinBox& box, const std::vector<unsigned char>& source)
{
    MeshData data;
    const float white[] = {1.0f, 1.0f, 1.0f};
    addBox(data, box.hx, box.hy, box.hz, white);
    data.hasColors = false;
    data.colors.clear();
    data.hasUVs = true;

    // Cuboid net in the same face order as addBox. Flip the vertical order
    // between UVs and bitmap rows, with row zero uploaded at v = 0.
    const int w = box.width, h = box.height, d = box.depth;
    struct Face { int x, y, w, h; };
    const Face faces[] = {
        {d+w,d,d,h}, {0,d,d,h}, {d,0,w,d}, {d+w,0,w,d},
        {d,d,w,h}, {2*d+w,d,w,h}
    };
    std::vector<unsigned char> pixels = source;
    for (const Face& face : faces)
    {
        const int x = box.x + face.x, y = box.y + face.y;
        for (int row = 0; row < face.h; ++row)
            for (int col = 0; col < face.w; ++col)
            {
                const int base = ((y + row) * 64 + x + col) * 4;
                const int outer = ((box.overlayY + face.y + row) * 64 +
                                   box.overlayX + face.x + col) * 4;
                const int alpha = source[outer + 3];
                for (int channel = 0; channel < 3; ++channel)
                {
                    const int color = source[base + 3] ? source[base + channel] : 166;
                    pixels[base + channel] = (color * (255 - alpha) + source[outer + channel] * alpha) / 255;
                }
                pixels[base + 3] = 255;
            }
        const float left = (x + 0.001f) / 64.0f, right = (x + face.w - 0.001f) / 64.0f;
        const float top = (y + 0.001f) / 64.0f, bottom = (y + face.h - 0.001f) / 64.0f;
        data.uvs.insert(data.uvs.end(), {left,bottom, right,bottom, right,top,
                                         left,bottom, right,top, left,top});
    }
    auto mesh = std::make_shared<Mesh>();
    mesh->adoptSkin(std::move(data), pixels.data(), 64, 64);
    return mesh;
}

void applyCharacter(Scene& scene)
{
    RobotCtl& robot = g_robot;
    const bool skinActive = robot.look == CharacterLook::Skin;
    const bool modelActive = robot.look == CharacterLook::Model;
    for (int i = 0; i < P_Count; ++i)
    {
        Object& o = scene.objects[robot.part[i]];
        o.mesh = skinActive ? robot.skinMeshes[i] : robot.originalMeshes[i];
        o.visible = o.mesh && !modelActive;
    }
    Object& imported = scene.objects[robot.importedObject];
    imported.mesh = robot.importedModel;
    imported.visible = modelActive && imported.mesh;
}

void releaseImportedMeshes(Scene& scene, CharacterLook look)
{
    RobotCtl& robot = g_robot;
    for (auto& mesh : scene.meshes)
        if (mesh && (look == CharacterLook::Model ? mesh == robot.importedModel :
                    std::find(robot.skinMeshes.begin(), robot.skinMeshes.end(), mesh) != robot.skinMeshes.end()))
        {
            mesh->release();
            mesh.reset();
        }
    scene.meshes.erase(std::remove(scene.meshes.begin(), scene.meshes.end(), nullptr), scene.meshes.end());
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
        { "Robot Torso",  0.28f, 0.36f, 0.17f, { 0.98f, 0.43f, 0.19f } },
        { "Robot Head",   0.22f, 0.17f, 0.18f, { 0.98f, 0.94f, 0.80f } },
        { "Robot Eye L",  0.031f, 0.044f, 0.010f, { 0.40f, 0.94f, 0.85f } },
        { "Robot Eye R",  0.031f, 0.044f, 0.010f, { 0.40f, 0.94f, 0.85f } },
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
        const float dark[3] = { 0.08f, 0.14f, 0.17f };
        const float ivory[3] = { 0.98f, 0.94f, 0.80f };
        if (i == P_Head)
            addBox(d, 0.184f, 0.083f, 0.022f, dark, v3(0.0f, 0.028f, 0.182f));
        if (i == P_Torso)
        {
            addBox(d, 0.172f, 0.032f, 0.012f, ivory, v3(0.0f, -0.118f, 0.17f));
            addBox(d, 0.167f, 0.215f, 0.047f, dark, v3(0.0f, 0.012f, -0.190f));
        }
        auto mesh = std::make_shared<Mesh>();
        mesh->adopt(std::move(d));
        R.originalMeshes[i] = mesh;
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

    {
        Object o;
        o.name = "Imported Character";
        o.kind = PrimKind::Mesh;
        o.selectable = false;
        o.visible = false;
        mat4Identity(o.xform);
        R.importedObject = (int)scene.objects.size();
        scene.objects.push_back(std::move(o));
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

bool robotImportCharacter(Scene& scene, const std::string& path, bool slim, std::string& error)
{
    RobotCtl& robot = g_robot;
    if (!robot.exists) { error = "Open the forest scene to import a character."; return false; }
    const auto file = std::filesystem::u8path(path);
    std::string extension = file.extension().u8string();
    for (char& c : extension) c = (char)std::tolower((unsigned char)c);
    PartMeshes skin;
    std::shared_ptr<Mesh> model;
    bool slimUsed = slim;
    int  skinWUsed = 64, skinHUsed = 64;
    if (extension == ".png")
    {
        std::vector<unsigned char> pixels;
        int skinW = 0, skinH = 0;
        if (!readSkinPixels(path, pixels, &skinW, &skinH))
        {
            error = "Expected a 64x64, 64x32 legacy or HD skin PNG.";
            return false;
        }
        // 旧版 64x32 只有 Classic 手臂；归一化后再按 64x64 布局取块。
        const bool legacy = (skinW == 64 && skinH == 32);
        const bool slimLayout = slim && !legacy;
        slimUsed = slimLayout;
        skinWUsed = skinW;
        skinHUsed = skinH;
        std::vector<unsigned char> skinPixels;
        normalizeSkin(pixels, skinW, skinH, skinPixels);
        const int arm = slimLayout ? 3 : 4;
        const float armHalf = slimLayout ? 0.074f : 0.097f;
        const SkinBox boxes[] = {
            {P_Head, 0,0, 32,0, 8,8,8, .24f,.20f,.215f},
            {P_Torso, 16,16, 16,32, 8,12,4, .275f,.36f,.146f},
            {P_ArmL, 40,16, 40,32, arm,12,4, armHalf,.31f,.114f},
            {P_ArmR, 32,48, 48,48, arm,12,4, armHalf,.31f,.114f},
            {P_LegL, 0,16, 0,32, 4,12,4, .121f,.54f,.133f},
            {P_LegR, 16,48, 0,48, 4,12,4, .121f,.54f,.133f},
        };
        for (const SkinBox& box : boxes)
        {
            skin[box.part] = buildSkinBox(box, skinPixels);
            if (!skin[box.part]->hasTexture())
            {
                for (const auto& mesh : skin) if (mesh) mesh->release();
                error = "Could not create the skin texture.";
                return false;
            }
        }
        std::printf("[character] PNG: %dx%d%s -> 64x64 %s skin, 6 parts, NEAREST filtering\n",
                    skinW, skinH, legacy ? " (legacy)" : (skinW > 64 ? " (HD)" : ""),
                    slimLayout ? "Slim (3px arms)" : "Classic (4px arms)");
    }
    else if (extension == ".obj")
    {
        model = std::make_shared<Mesh>();
        if (!model->loadFromOBJ(path, UpAxis::Y, false, &error)) return false;
        const MeshData& data = model->data();
        const float height = data.bboxMax[1] - data.bboxMin[1];
        if (!std::isfinite(height) || height < 1e-5f)
        {
            model->release();
            error = "The OBJ needs a nonzero height on the Y axis.";
            return false;
        }
        model->preserveSmallTexturePixels();
        scene.objects[robot.importedObject].size = 2.35f / height;
        // 实验规范 W3-3 要求记录加载信息：面数 / 法线 / 上轴。
        std::printf("[character] OBJ: %d verts (%d unique), %d faces -> %d tris, "
                    "normals=%s, uv=%s, up=Y, height %.2f -> scale %.3f\n",
                    data.srcVertexCount, data.uniqueVertexCount, data.srcFaceCount,
                    data.triangleCount, data.hasNormals ? "yes" : "generated",
                    data.hasUVs ? "yes" : "no", height, 2.35f / height);
    }
    else { error = "Choose a 64 x 64 PNG skin or a Y-up OBJ."; return false; }

    robot.look = model ? CharacterLook::Model : CharacterLook::Skin;
    releaseImportedMeshes(scene, robot.look);
    if (model)
    {
        robot.modelLabel = "OBJ  " + file.filename().u8string();
        robot.importedModel = std::move(model);
        scene.meshes.push_back(robot.importedModel);
    }
    else
    {
        robot.skinLabel = std::string(slimUsed ? "Slim " : "") +
                          std::to_string(skinWUsed) + "x" + std::to_string(skinHUsed) + " " +
                          file.filename().u8string();
        robot.skinMeshes = std::move(skin);
        for (const auto& mesh : robot.skinMeshes) if (mesh) scene.meshes.push_back(mesh);
    }
    applyCharacter(scene);
    std::printf("[character] imported %s: %s\n", extension.c_str(), path.c_str());
    return true;
}

bool robotToggleCharacter(Scene& scene)
{
    RobotCtl& robot = g_robot;
    if (robot.skinLabel.empty() && robot.modelLabel.empty()) return false;
    if (robot.look == CharacterLook::BuiltIn && !robot.skinLabel.empty())
        robot.look = CharacterLook::Skin;
    else if (robot.look != CharacterLook::Model && !robot.modelLabel.empty())
        robot.look = CharacterLook::Model;
    else
        robot.look = CharacterLook::BuiltIn;
    applyCharacter(scene);
    return true;
}

// W3-3 实验规范要求的 M 键：只在「拼装机器人 ↔ 外部导入模型」之间切换。
// K 是三态循环（内置→PNG→OBJ），M 是二态替换，答辩演示时更不容易按错。
bool robotToggleModel(Scene& scene)
{
    RobotCtl& robot = g_robot;
    if (!robot.exists) return false;
    if (!robotHasImportedCharacter()) return false;

    if (robot.look == CharacterLook::Model)
        robot.look = CharacterLook::BuiltIn;
    else if (!robot.modelLabel.empty())
        robot.look = CharacterLook::Model;
    else if (robot.look == CharacterLook::Skin)
        robot.look = CharacterLook::BuiltIn;      // 没有 OBJ：PNG ↔ 内置
    else
        robot.look = CharacterLook::Skin;

    applyCharacter(scene);
    std::printf("[character] look -> %s (%s)\n",
                robot.look == CharacterLook::Model ? "OBJ model" :
                robot.look == CharacterLook::Skin  ? "PNG skin" : "built-in robot",
                robotCharacterSource().c_str());
    return true;
}

bool robotHasImportedCharacter()
{
    return g_robot.exists && (!g_robot.skinLabel.empty() || !g_robot.modelLabel.empty());
}

// 无 GL 的皮肤检查：只做解码 + 归一化，不创建纹理，可作 --skin-info 使用。
bool robotInspectSkin(const std::string& path, bool slim, std::string& out)
{
    std::vector<unsigned char> src;
    int w = 0, h = 0;
    if (!readSkinPixels(path, src, &w, &h))
    {
        out = "unsupported: expected 64x64, 64x32 legacy or HD (64*n) PNG";
        return false;
    }
    const bool legacy = (w == 64 && h == 32);
    const bool hd     = (w > 64);
    std::vector<unsigned char> norm;
    normalizeSkin(src, w, h, norm);

    auto opaque = [&](int x, int y, int rw, int rh)
    {
        int n = 0;
        for (int r = 0; r < rh; ++r)
            for (int c = 0; c < rw; ++c)
                if (norm[((size_t)(y + r) * 64 + x + c) * 4 + 3]) ++n;
        return n;
    };
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "%dx%d %s%s -> 64x64; left arm %dpx, left leg %dpx, overlay arm %dpx",
                  w, h, legacy ? "legacy " : hd ? "HD " : "modern ",
                  slim ? "slim" : "classic",
                  opaque(32, 48, 12, 16), opaque(16, 48, 12, 16), opaque(48, 48, 12, 16));
    out = buf;
    return true;
}

const std::string& robotCharacterSource()
{
    static const std::string original = "Built-in robot";
    if (g_robot.look == CharacterLook::Skin) return g_robot.skinLabel;
    if (g_robot.look == CharacterLook::Model) return g_robot.modelLabel;
    return original;
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
    if (R.chase)
    {
        // 追赶：位置/朝向由 app 从鹿的状态推出（优先于路径）
        Object& root = scene.objects[R.root];
        root.pos[0] = R.chaseX;
        root.pos[1] = 0.0f;
        root.pos[2] = R.chaseZ;
        R.yawDeg = R.chaseYaw;
        R.walking = true;
    }
    else if (R.onPath)
    {
        if (R.tOverride < 0.0f && R.pathTick >= 0.0f && t >= R.pathTick)
            R.pathTime += t - R.pathTick;
        R.pathTick = t;
        const float s = kPathSpeed * (R.tOverride >= 0.0f ? t : R.pathTime);
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

    if (R.importedModel)
    {
        Object& model = scene.objects[R.importedObject];
        const MeshData& data = R.importedModel->data();
        float offset[16], modelMatrix[16];
        mat4FromTranslation(offset, -data.center[0] * model.size,
                            -data.bboxMin[1] * model.size, -data.center[2] * model.size);
        // 用 Mbase 而不是 A：导入模型也跟随走路重心起伏，替换后不会像贴图一样僵在原地。
        mat4Multiply(modelMatrix, Mbase, offset);
        writePart(model, modelMatrix);
    }

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

        mat4FromTranslation(T, -0.09f, 0.188f, 0.210f);
        mat4Multiply(M, H, T);
        writePart(scene.objects[R.part[P_EyeL]], M);
        mat4FromTranslation(T, 0.09f, 0.188f, 0.210f);
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
    robotSetPathWalk(false);
    robotClearChasePose();
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
    // 手动走路开关接管自动路径/追赶，否则下一帧又会被强制设为 walking。
    const bool wasWalking = g_robot.walking;
    robotSetPathWalk(false);
    robotClearChasePose();
    g_robot.walking = wasWalking;
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
    g_robot.pathTick = -1.0f;
    if (on)
    {
        // 记住进入路径前的走路状态，退出时还原（避免 P 键偷偷改掉 b 键的状态）
        g_robot.wasWalking = g_robot.walking;
        g_robot.walking    = true;
        g_robot.chase      = false;                // 路径与追赶互斥
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

// ---------------------------------------------------------------------------
// W3-2 追赶演示
// ---------------------------------------------------------------------------
void robotSetChasePose(float x, float z, float yawDeg)
{
    if (!g_robot.exists) return;
    g_robot.chase     = true;
    g_robot.onPath    = false;                 // 追赶优先，退出路径模式
    g_robot.chaseX    = x;
    g_robot.chaseZ    = z;
    g_robot.chaseYaw  = yawDeg;
    g_robot.walking   = true;
    g_robot.pose      = RobotPose::WalkFreeze;
}

void robotClearChasePose()
{
    if (!g_robot.chase) return;
    g_robot.chase   = false;
    g_robot.walking = false;                   // 追完停下
    std::printf("[robot] chase pose cleared\n");
}

bool robotChasing() { return g_robot.exists && g_robot.chase; }

void robotSetTimeOverride(float t)
{
    g_robot.tOverride = t;
    g_robot.lastT = -1.0f;
    g_robot.pathTick = -1.0f;
}

float robotTimeOverride() { return g_robot.tOverride; }

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
