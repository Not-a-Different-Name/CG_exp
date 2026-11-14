#include "anim.h"
#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// 蹦跳步态参数（森林漫游用）
constexpr float kHopFreq   = 2.2f;    // 每秒跳几下
constexpr float kHopHeight = 0.52f;   // 跳跃高度，模型已归一化（兔子高约 1.5 单位）
constexpr float kCamDist   = 4.6f;    // 追尾相机在兔子后方的距离
constexpr float kCamHeight = 1.75f;   // 追尾相机高度

Object* findObjectByName(const char* name)
{
    for (Object& o : g_app.scene.objects)
        if (o.name == name) return &o;
    return nullptr;
}

// 优先拿导入的模型，没有就退回彩色立方体
Object* findSpinTarget()
{
    for (Object& o : g_app.scene.objects)
        if (o.kind == PrimKind::Mesh) return &o;
    return findObjectByName("Color Cube");
}

// 场景里第一个导入的模型（森林场景里就是兔子）
Object* findFirstMesh()
{
    for (Object& o : g_app.scene.objects)
        if (o.kind == PrimKind::Mesh && o.mesh) return &o;
    return nullptr;
}

inline float smoothOutAndBack(float local01)
{
    // local 0 -> 0, 0.5 -> 1, 1 -> 0，两端速度为 0，看起来不突兀
    return std::sin(local01 * kPi);
}

} // namespace

bool parseAnimKind(const std::string& s, AnimKind& out)
{
    if (s == "turntable" || s == "turn" || s == "spin") out = AnimKind::Turntable;
    else if (s == "orbit"  || s == "circle")            out = AnimKind::Orbit;
    else if (s == "projections" || s == "proj" || s == "projection") out = AnimKind::Projections;
    else if (s == "transforms" || s == "transform" || s == "xform")  out = AnimKind::Transforms;
    else if (s == "roam" || s == "walk" || s == "hop")  out = AnimKind::Roam;
    else if (s == "none")                               out = AnimKind::None;
    else return false;
    return true;
}

Vec3 roamPath(float t)
{
    // 两个不同频率的正弦叠加：轨迹平滑、不闭合，看起来像在悠闲地逛而不是绕圈
    const float x = 13.0f * std::sin(0.115f * t) + 5.0f * std::sin(0.283f * t + 1.3f);
    const float z = 13.0f * std::cos(0.097f * t) + 5.0f * std::cos(0.241f * t + 2.1f);
    return v3(x, 0.0f, z);
}

const char* animKindName(AnimKind k)
{
    switch (k)
    {
    case AnimKind::None:        return "none";
    case AnimKind::Turntable:   return "turntable (model spins about Y)";
    case AnimKind::Orbit:       return "orbit (camera circles the model)";
    case AnimKind::Projections: return "projections (perspective/ortho/cavalier/cabinet)";
    case AnimKind::Transforms:  return "transforms (translate/rotate/scale the cube)";
    case AnimKind::Roam:        return "roam (bunny hopping through the forest)";
    }
    return "?";
}

void applyAnimation(AnimKind kind, float t, float totalSeconds)
{
    if (totalSeconds <= 0.0f) totalSeconds = 1.0f;
    const float u = std::min(1.0f, t / totalSeconds);        // 归一化进度 0..1

    // 几何变换演示只留立方体 + 地面/网格/坐标轴作空间参照。
    // 兔子半径 1.0（两单位宽）、离相机又只有两三个单位，不管把相机挪到哪一侧都会
    // 占掉一大块画面；要让立方体的旋转和缩放看清楚，只能把它藏起来。
    // 每个动画分支都显式设置可见性，保证同一 kind 下结果可复现。
    const bool cubeOnly = (kind == AnimKind::Transforms);
    for (Object& o : g_app.scene.objects)
    {
        if (!cubeOnly)
        {
            o.visible = true;
        }
        else
        {
            o.visible = (o.kind == PrimKind::Ground || o.kind == PrimKind::Grid ||
                         o.kind == PrimKind::Axes   || o.name == "Color Cube");
        }
    }

    switch (kind)
    {
    // -----------------------------------------------------------------------
    case AnimKind::Turntable:
    {
        Object* o = findSpinTarget();
        if (o) mat4FromAxisAngle(o->xform, 360.0f * u, 0.0f, 1.0f, 0.0f);

        // 相机固定在默认站位，让模型自己在转
        g_app.viewPreset = ViewPreset::Free;
        g_app.camera.reset(g_app.scene.radius());
        break;
    }

    // -----------------------------------------------------------------------
    case AnimKind::Orbit:
    {
        Camera& c = g_app.camera;
        const float R = g_app.scene.radius() * 2.8f;
        const float a = 2.0f * kPi * u;

        c.target = v3(0.0f, 0.0f, 0.0f);
        c.eye    = v3(R * std::sin(a), 0.42f * R, R * std::cos(a));
        c.up     = v3(0.0f, 1.0f, 0.0f);
        c.fovY   = 45.0f;

        g_app.viewPreset = ViewPreset::Free;
        break;
    }

    // -----------------------------------------------------------------------
    case AnimKind::Projections:
    {
        // 四段等长，依次是 透视 / 正平行 / 斜等测 / 斜二测
        const int phase = std::min(3, (int)(u * 4.0f));

        // 平行投影（含斜平行）从正视图方向看最能体现"投影"的差别；
        // 透视则用自由相机，有纵深感。
        if (phase == 0)
        {
            g_app.projMode   = ProjMode::Perspective;
            g_app.viewPreset = ViewPreset::Free;
        }
        else
        {
            g_app.projMode   = (phase == 1) ? ProjMode::Ortho
                             : (phase == 2) ? ProjMode::Cavalier
                                            : ProjMode::Cabinet;
            g_app.viewPreset = ViewPreset::Front;
            g_app.camera.setPreset((int)ViewPreset::Front, g_app.scene.radius());
        }
        break;
    }

    // -----------------------------------------------------------------------
    case AnimKind::Transforms:
    {
        Object* cube = findObjectByName("Color Cube");
        if (!cube) cube = findSpinTarget();
        if (!cube) break;

        // 把相机拉到立方体附近并正对着它。场景级取景（相机距离 ~4.4）下立方体
        // 只占画面高度的 11%，旋转和缩放的幅度根本看不出来。
        const Vec3 base = v3(cube->pos[0], cube->pos[1], cube->pos[2]);
        Camera& c = g_app.camera;
        // 归一化的观察方向，这样 D 就是真正的相机距离。用常规的四分之三视角。
        const Vec3 dir = v3norm(v3(0.35f, 0.42f, 1.00f));
        const float D = 2.3f;
        c.target = base;
        c.eye    = base + dir * D;
        c.up     = v3(0.0f, 1.0f, 0.0f);
        c.fovY   = 45.0f;
        g_app.viewPreset = ViewPreset::Free;
        g_app.projMode   = ProjMode::Perspective;

        // 三段：平移 → 旋转 → 缩放
        const int   phase = std::min(2, (int)(u * 3.0f));
        const float local = std::min(1.0f, std::max(0.0f, u * 3.0f - (float)phase));

        // 物体自身的尺寸作为幅度基准，换模型不用改常数
        const float amp = cube->size;

        float m[16];
        mat4Identity(m);
        switch (phase)
        {
        case 0:   // 平移：往 +X 出去再回来，同时略微抬起，避免和地面视觉上粘住
        {
            const float k = smoothOutAndBack(local);
            mat4FromTranslation(m, amp * 2.0f * k, amp * 0.7f * k, 0.0f);
            break;
        }
        case 1:   // 旋转：绕 Y 轴整整一圈
            mat4FromAxisAngle(m, 360.0f * local, 0.0f, 1.0f, 0.0f);
            break;
        default:  // 缩放：从地面往上长
        {
            // 立方体的 xform 作用在「已经按 size 缩放过的」坐标上，底面在 -1*size。
            // 放大约 1.8 倍后底面会掉到 -1.8*size，也就是陷进地面里；这里同步抬升
            // size*(s-1) 让底面保持在原处。
            const float s = 1.0f + 0.8f * smoothOutAndBack(local);
            mat4FromScale(m, s, s, s);
            m[13] = cube->size * (s - 1.0f);
            break;
        }
        }
        std::memcpy(cube->xform, m, sizeof(m));

        // 让 HUD 的 "Selected" 跟着指向立方体：画面里只剩它，写着 Rabbit 会误导
        for (size_t i = 0; i < g_app.scene.objects.size(); ++i)
            if (&g_app.scene.objects[i] == cube) g_app.scene.selected = (int)i;
        break;
    }

    // -----------------------------------------------------------------------
    case AnimKind::Roam:
    {
        Object* bunny = findFirstMesh();
        if (!bunny || !bunny->mesh) break;

        // 水平位置沿漫步曲线走
        const Vec3 p = roamPath(t);

        // 朝向取路径切线。模型自身朝 -X（顶视图确认过）：绕 Y 转 θ 后模型前向是
        // (-cosθ, 0, sinθ)，令它等于单位切线 h，即 θ = atan2(h.z, -h.x)。
        const Vec3 ahead = roamPath(t + 0.06f);
        const Vec3 h = v3norm(v3(ahead.x - p.x, 0.0f, ahead.z - p.z));
        if (v3len(h) < 1e-6f) break;
        const float yawDeg = std::atan2(h.z, -h.x) * 180.0f / kPi;

        // 蹦跳弧线用抛物线（4k(1-k)）而不是正弦 —— 更接近真实的弹道，
        // 落地那一下的顿挫感明显得多。k: 0 在地面，1 在最高点。
        const float phase = t * kHopFreq;
        const float frac  = phase - std::floor(phase);
        const float k     = 4.0f * frac * (1.0f - frac);
        const float lift  = kHopHeight * k;

        // 腾空拉长、落地压扁（动画十二原则的 squash & stretch），用 1/sqrt 保持体积
        const float sy  = 1.0f + 0.26f * (k - 0.32f);
        const float sxz = 1.0f / std::sqrt(sy);

        // 起跳抬头、下落低头。
        //
        // 注意轴：模型朝 -X，所以"俯仰"（机头上下）是绕 **Z** 轴转，不是绕 X 轴 ——
        // 绕 X 转只会让它左右翻滚。绕 Z 转 θ 时前向 (-1,0,0) 变成 (-cosθ,-sinθ,0)，
        // 即 θ>0 低头、θ<0 抬头。
        const float pitchDeg = 13.0f * (frac * 2.0f - 1.0f);   // 起跳 -13(抬头) → 落地 +13(低头)

        float mYaw[16], mPitch[16], mSquash[16], tmp[16];
        mat4FromAxisAngle(mYaw,   yawDeg,   0.0f, 1.0f, 0.0f);
        mat4FromAxisAngle(mPitch, pitchDeg, 0.0f, 0.0f, 1.0f);
        mat4FromScale(mSquash, sxz, sy, sxz);
        mat4Multiply(tmp, mYaw, mPitch);
        mat4Multiply(bunny->xform, tmp, mSquash);

        // 模型原点在包围球中心而不是脚底，得垫高 standY 才踩在地上；
        // 压扁时脚还会往上跑，乘 sy 抵回来让脚始终贴着地面。
        const float standY = -bunny->mesh->data().bboxMin[1];
        bunny->pos[0] = p.x;
        bunny->pos[1] = lift + standY * sy;
        bunny->pos[2] = p.z;

        // 追尾相机：斜后上方，看向兔子前方一点，让兔子落在画面中下部
        Camera& c = g_app.camera;
        c.eye    = v3(p.x - h.x * kCamDist, kCamHeight + lift * 0.30f, p.z - h.z * kCamDist);
        c.target = v3(p.x + h.x * 2.2f, 0.85f + lift * 0.55f, p.z + h.z * 2.2f);
        c.up     = v3(0.0f, 1.0f, 0.0f);
        c.fovY   = 45.0f;

        g_app.viewPreset = ViewPreset::Free;
        g_app.projMode   = ProjMode::Perspective;

        // HUD 指着兔子
        for (size_t i = 0; i < g_app.scene.objects.size(); ++i)
            if (&g_app.scene.objects[i] == bunny) g_app.scene.selected = (int)i;
        break;
    }

    // -----------------------------------------------------------------------
    case AnimKind::None:
    default:
        break;
    }
}
