#include "bezier.h"
#include "forest.h"        // 形状库：圆柱 / 椭球

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// 每段采样数：24 段折线在 4 单位半径的环上已经看不出棱角，
// 同时弧长误差远小于一步位移（机器人每帧约 0.03 单位）。
constexpr int kSamplesPerSegment = 24;

Vec3 lerp3(const Vec3& a, const Vec3& b, float t)
{
    return v3(a.x + (b.x - a.x) * t,
              a.y + (b.y - a.y) * t,
              a.z + (b.z - a.z) * t);
}

// 三次贝塞尔 de Casteljau：三次线性插值后取最后一点。
Vec3 deCasteljau(const Vec3 p[4], float u)
{
    const Vec3 a = lerp3(p[0], p[1], u);
    const Vec3 b = lerp3(p[1], p[2], u);
    const Vec3 c = lerp3(p[2], p[3], u);
    const Vec3 d = lerp3(a, b, u);
    const Vec3 e = lerp3(b, c, u);
    return lerp3(d, e, u);
}

float dist2D(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

// 把一段竖直单位圆柱（y=0..1）摆到 a->b 的连线上。
void addLink(MeshData& out, const Vec3& a, const Vec3& b,
             float radius, const float col[3])
{
    const Vec3 delta = b - a;
    const float len = v3len(delta);
    if (len < 1e-5f) return;
    const Vec3 dir = delta * (1.0f / len);

    float rot[16], scale[16], rs[16], trans[16], m[16];
    const float dot = std::max(-1.0f, std::min(1.0f, dir.y));
    const float angle = std::acos(dot) * 180.0f / kPi;
    const Vec3 axis = v3cross(v3(0.0f, 1.0f, 0.0f), dir);
    mat4FromAxisAngle(rot, angle, axis.x, axis.y, axis.z);
    mat4FromScale(scale, 1.0f, len, 1.0f);
    mat4Multiply(rs, rot, scale);
    mat4FromTranslation(trans, a.x, a.y, a.z);
    mat4Multiply(m, trans, rs);

    MeshData part;
    addCylinderSide(part, 0.0f, 1.0f, radius, radius, col, 5);
    appendTransformed(out, part, m);
}

} // namespace

// ---------------------------------------------------------------------------
void BezierPath::buildClosed(const std::vector<Vec3>& anchorPoints)
{
    segments.clear();
    anchors = anchorPoints;
    samples.clear();
    cum.clear();
    total = 0.0f;

    const int n = (int)anchors.size();
    if (n < 2) return;

    // Catmull-Rom -> 三次贝塞尔控制柄：保证曲线穿过锚点、且相邻段切线连续。
    for (int i = 0; i < n; ++i)
    {
        const Vec3& p0 = anchors[(i + n - 1) % n];
        const Vec3& p1 = anchors[i];
        const Vec3& p2 = anchors[(i + 1) % n];
        const Vec3& p3 = anchors[(i + 2) % n];

        Segment s;
        s.p[0] = p1;
        s.p[1] = p1 + (p2 - p0) * (1.0f / 6.0f);
        s.p[2] = p2 - (p3 - p1) * (1.0f / 6.0f);
        s.p[3] = p2;
        segments.push_back(s);
    }

    // 采样成折线并累计弧长（不含闭合点，末点与首点之间由 cum 收尾）。
    for (const Segment& s : segments)
        for (int k = 0; k < kSamplesPerSegment; ++k)
            samples.push_back(deCasteljau(s.p, (float)k / (float)kSamplesPerSegment));

    const int m = (int)samples.size();
    cum.resize((size_t)m + 1);
    cum[0] = 0.0f;
    for (int i = 0; i < m; ++i)
        cum[i + 1] = cum[i] + dist2D(samples[i], samples[(i + 1) % m]);
    total = cum[m];
}

Vec3 BezierPath::evalProportional(float u) const
{
    if (segments.empty()) return v3(0, 0, 0);
    u = std::fmod(u, 1.0f);
    if (u < 0.0f) u += 1.0f;

    const int n = (int)segments.size();
    float local = u * (float)n;
    int idx = (int)local;
    if (idx >= n) idx = n - 1;
    return deCasteljau(segments[idx].p, local - (float)idx);
}

Vec3 BezierPath::evalAtDistance(float s) const
{
    if (samples.empty() || total <= 1e-6f) return v3(0, 0, 0);

    s = std::fmod(s, total);
    if (s < 0.0f) s += total;

    const int m = (int)samples.size();
    // cum 单调递增，找到 s 落在哪一小段
    int lo = 0, hi = m;                         // cum[lo] <= s < cum[hi]
    while (lo + 1 < hi)
    {
        const int mid = (lo + hi) / 2;
        if (cum[mid] <= s) lo = mid;
        else               hi = mid;
    }
    const float segLen = cum[lo + 1] - cum[lo];
    const float t = (segLen > 1e-6f) ? (s - cum[lo]) / segLen : 0.0f;
    return lerp3(samples[lo], samples[(lo + 1) % m], t);
}

Vec3 BezierPath::tangentAtDistance(float s) const
{
    if (samples.empty() || total <= 1e-6f) return v3(0, 0, 1);

    s = std::fmod(s, total);
    if (s < 0.0f) s += total;

    const int m = (int)samples.size();
    int lo = 0, hi = m;
    while (lo + 1 < hi)
    {
        const int mid = (lo + hi) / 2;
        if (cum[mid] <= s) lo = mid;
        else               hi = mid;
    }
    return v3norm(samples[(lo + 1) % m] - samples[lo]);
}

float BezierPath::nearestDistance(const Vec3& p) const
{
    if (samples.empty()) return 0.0f;
    float best = 0.0f, bestD = 1e30f;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const float d = dist2D(samples[i], p);
        if (d < bestD) { bestD = d; best = cum[i]; }
    }
    return best;
}

// ---------------------------------------------------------------------------
void buildBezierVisual(MeshData& out, const BezierPath& path,
                       const float curveCol[3],
                       const float anchorCol[3],
                       const float handleCol[3])
{
    if (path.samples.empty()) return;

    const float lift = 0.04f;                  // 略高于地面，避免深度冲突
    const int   m = (int)path.samples.size();

    // 曲线本体：每 2 个采样点连一段短圆柱（步长减半，三角形仍只有几千）
    for (int i = 0; i < m; i += 2)
    {
        Vec3 a = path.samples[i];
        Vec3 b = path.samples[(i + 2) % m];
        a.y += lift;
        b.y += lift;
        addLink(out, a, b, 0.035f, curveCol);
    }

    // 锚点：曲线必经的大标记
    for (const Vec3& p : path.anchors)
    {
        const Vec3 c = v3(p.x, p.y + lift + 0.02f, p.z);
        addEllipsoid(out, c, 0.11f, 0.11f, 0.11f, anchorCol, 8, 5);
    }

    // 控制柄：每段两个，展示贝塞尔控制多边形（Catmull-Rom 推出的位置）
    for (const auto& s : path.segments)
    {
        for (int h = 1; h <= 2; ++h)
        {
            const Vec3 p = s.p[h];
            const Vec3 c = v3(p.x, p.y + lift + 0.02f, p.z);
            addEllipsoid(out, c, 0.05f, 0.05f, 0.05f, handleCol, 6, 4);
        }
    }
}
