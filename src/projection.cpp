#include "projection.h"
#include "camera.h"

#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.0f;

// 取景余量：包围球半径乘这个系数，保证物体四周留一圈空白
constexpr float kFitMargin = 1.06f;

// ---------------------------------------------------------------------------
// 斜平行投影的错切矩阵（列主序 GLfloat[16]）
//
// 推导：斜平行投影 = 正交投影 ∘ 沿视线方向的错切。眼空间里可见点的 z_e = -d (d>0)，
// 错切量正比于深度，于是
//     x' = x - L·cosφ·z_e
//     y' = y - L·sinφ·z_e
// 其中 L = cot α 是斜边的缩短系数：斜等测(cavalier) α=45° → L=1；
// 斜二测(cabinet) α≈63.43° → L=0.5。
//
// 关键：m[10] 必须是 1，不能是 0。教材上那个斜投影矩阵是奇异的（把 z 压平成一个平面），
// 直接拿来用会让所有片元落到同一个 NDC 深度，深度测试彻底失效，整个模型 z-fighting。
// 保持仿射形式投影方向完全一致（屏幕上 x,y 结果相同），但深度缓冲仍然可用。
// ---------------------------------------------------------------------------
void buildObliqueShear(float L, float phiRad, float m[16])
{
    m[0] = 1.0f;  m[1] = 0.0f;  m[2]  = 0.0f;  m[3]  = 0.0f;
    m[4] = 0.0f;  m[5] = 1.0f;  m[6]  = 0.0f;  m[7]  = 0.0f;
    m[8] = -L * std::cos(phiRad);      // 错切列（对应 z_e 的系数）
    m[9] = -L * std::sin(phiRad);
    m[10] = 1.0f;
    m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

} // namespace

bool isParallelProjection(ProjMode mode)
{
    return mode != ProjMode::Perspective;
}

const char* projModeName(ProjMode mode)
{
    switch (mode)
    {
    case ProjMode::Perspective: return "Perspective";
    case ProjMode::Ortho:       return "Orthographic";
    case ProjMode::Cavalier:    return "Oblique cavalier (L=1)";
    case ProjMode::Cabinet:     return "Oblique cabinet (L=0.5)";
    }
    return "?";
}

void applyProjection(ProjMode mode, int winW, int winH, const Camera& cam,
                     float sceneRadius, float obliqueAngleDeg)
{
    if (winH <= 0) winH = 1;
    if (winW <= 0) winW = 1;
    if (sceneRadius < 1e-4f) sceneRadius = 1.0f;

    // 保证「世界单位/像素」在 x、y 两个方向相等 —— 这就是不变形的充要条件
    const float aspect = (float)winW / (float)winH;
    const float fitHalfH = sceneRadius * kFitMargin;

    // 取景半高/半宽。
    //
    // 先按高度取景；但窗口很扁（aspect < 1）时，如果还按高度取景，水平方向能看到的
    // 范围就会小于场景，物体左右会被裁掉。所以此时改成按宽度取景、把竖直范围放大。
    //
    // 无论走哪个分支，halfW / halfH 恒等于 aspect —— 这是不变形的关键。
    auto fitExtents = [&](float& halfW, float& halfH) {
        halfH = (aspect >= 1.0f) ? fitHalfH : fitHalfH / aspect;
        halfW = halfH * aspect;
    };

    switch (mode)
    {
    case ProjMode::Perspective:
    {
        const float dist = cam.distance();
        // near 取「到最近几何的距离」和半径的一个小比例中的较大者，避免相机贴近时被裁掉；
        // glFrustum 要求 near > 0，必须夹住。
        float nearZ = dist - sceneRadius * 1.5f;
        const float minNear = sceneRadius * 0.01f;
        if (nearZ < minNear) nearZ = minNear;

        // 相机在场景**内部**时（森林里追尾相机就是这种情形），上面那个式子会给出
        // 一个远大于相机距离的近平面 —— 例如 dist=28、半径 10 时算出 13，
        // 于是把 13 单位内的树整个切掉，露出树干的内部截面，画面上一道道深色尖刺。
        // 近平面永远不该超过相机到目标的四分之一。
        const float maxNear = dist * 0.12f;
        if (nearZ > maxNear) nearZ = maxNear;

        float farZ = dist + sceneRadius * 4.0f;
        if (farZ <= nearZ) farZ = nearZ + sceneRadius;

        // glFrustum 的四个边界是「在近平面上」量的，所以要乘 nearZ。
        // 与正交同理：宽窗口固定竖直 FOV，扁窗口固定水平 FOV。
        const float tanV = std::tan(cam.fovY * 0.5f * kDeg2Rad);
        float halfW, halfH;
        if (aspect >= 1.0f) { halfH = nearZ * tanV;         halfW = halfH * aspect; }
        else                { halfW = nearZ * tanV;         halfH = halfW / aspect; }

        glFrustum(-halfW, halfW, -halfH, halfH, nearZ, farZ);
        break;
    }

    case ProjMode::Ortho:
    {
        float halfW, halfH;
        fitExtents(halfW, halfH);
        // 正交投影用「永不裁剪」的近远平面：glOrtho 允许 near 为负，而且正交深度
        // 精度是线性的，±1000 对 24 位深度缓冲完全够用。这样六种视图和相机漫游
        // 都不会因为相机跑到物体另一侧而把物体裁掉。
        glOrtho(-halfW, halfW, -halfH, halfH, -1000.0, 1000.0);
        break;
    }

    case ProjMode::Cavalier:
    case ProjMode::Cabinet:
    {
        const float L = (mode == ProjMode::Cavalier) ? 1.0f : 0.5f;

        // 屏幕上看到的斜边夹角 θ 与世界空间的 φ 满足 tanθ = tanφ · aspect。
        // 想让屏幕上正好是 obliqueAngleDeg（斜二测教科书上的 45°），要反解 φ。
        const float phiScreen = obliqueAngleDeg * kDeg2Rad;
        const float phi = std::atan(std::tan(phiScreen) / aspect);

        const float d = cam.distance();

        float baseW, baseH;
        fitExtents(baseW, baseH);

        // 错切会把物体整体平移 L·cosφ·d，并额外撑开 L·|cosφ|·r。
        // 前者靠平移 ortho 窗口抵消（顺带让斜投影不再依赖相机距离 —— 平行投影
        // 本来就与距离无关），后者靠放大窗口覆盖。
        //
        // 放大时必须对宽高用同一个比例：如果 x、y 各加各的，halfW/halfH 就不再等于
        // aspect，物体正对相机的那一面会被拉伸 —— 而"正对相机的面保持真实形状"
        // 恰恰是斜平行投影的定义。
        const float spreadX = L * std::fabs(std::cos(phi)) * sceneRadius;
        const float spreadY = L * std::fabs(std::sin(phi)) * sceneRadius;
        const float grow    = std::max(1.0f,
                              std::max((baseW + spreadX) / baseW, (baseH + spreadY) / baseH));

        const float halfW = baseW * grow;
        const float halfH = baseH * grow;

        const float shiftX = L * std::cos(phi) * d;
        const float shiftY = L * std::sin(phi) * d;

        glOrtho(-halfW + shiftX, halfW + shiftX,
                -halfH + shiftY, halfH + shiftY,
                -1000.0, 1000.0);

        // glMultMatrixf 是右乘：M ← M·S，即先错切再正交，正是斜平行投影的定义。
        // 必须放在投影矩阵上：错切是相对投影平面（眼空间）定义的，放到 modelview
        // 就变成世界空间错切，相机一转斜边方向就错了。
        float shear[16];
        buildObliqueShear(L, phi, shear);
        glMultMatrixf(shear);
        break;
    }
    }
}
