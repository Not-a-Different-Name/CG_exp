#include "forest.h"

#include <cmath>
#include <cstring>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// 确定性伪随机：xorshift32。固定种子保证每次运行生成完全一样的森林。
// ---------------------------------------------------------------------------
struct Rng
{
    unsigned s;
    explicit Rng(unsigned seed) : s(seed ? seed : 1u) {}

    unsigned next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float unit() { return (float)(next() & 0xFFFFFFu) / (float)0x1000000u; }   // [0,1)
    float range(float a, float b) { return a + (b - a) * unit(); }
};

} // namespace

// ---------------------------------------------------------------------------
// 基本形状（在 forest.h 里声明，供动物等其它程序化网格复用）
// ---------------------------------------------------------------------------
void pushTri(MeshData& m, const Vec3& a, const Vec3& b, const Vec3& c, const float col[3])
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

    m.hasNormals = true;
    m.hasColors  = true;
}

void addCylinderSide(MeshData& m, float y0, float y1, float r0, float r1,
                     const float col[3], int sides)
{
    for (int i = 0; i < sides; ++i)
    {
        const float t0 = 2.0f * kPi * (float)i / (float)sides;
        const float t1 = 2.0f * kPi * (float)(i + 1) / (float)sides;

        const Vec3 p0(r0 * std::cos(t0), y0, r0 * std::sin(t0));
        const Vec3 p1(r0 * std::cos(t1), y0, r0 * std::sin(t1));
        const Vec3 q0(r1 * std::cos(t0), y1, r1 * std::sin(t0));
        const Vec3 q1(r1 * std::cos(t1), y1, r1 * std::sin(t1));

        pushTri(m, p0, q0, q1, col);
        pushTri(m, p0, q1, p1, col);
    }
}

void addCone(MeshData& m, float y0, float y1, float r, const float col[3], int sides)
{
    const Vec3 apex(0.0f, y1, 0.0f);
    for (int i = 0; i < sides; ++i)
    {
        const float t0 = 2.0f * kPi * (float)i / (float)sides;
        const float t1 = 2.0f * kPi * (float)(i + 1) / (float)sides;
        const Vec3 p0(r * std::cos(t0), y0, r * std::sin(t0));
        const Vec3 p1(r * std::cos(t1), y0, r * std::sin(t1));

        pushTri(m, p0, apex, p1, col);
        pushTri(m, v3(0.0f, y0, 0.0f), p0, p1, col);      // 底盖朝下
    }
}

void addEllipsoid(MeshData& m, const Vec3& c, float rx, float ry, float rz,
                  const float col[3], int slices, int stacks)
{
    const GLuint base = (GLuint)(m.positions.size() / 3);

    for (int i = 0; i <= stacks; ++i)
    {
        const float phi = kPi * (float)i / (float)stacks;      // 0 = 北极
        for (int j = 0; j <= slices; ++j)
        {
            const float th = 2.0f * kPi * (float)j / (float)slices;
            const float sx = std::sin(phi) * std::cos(th);
            const float sy = std::cos(phi);
            const float sz = std::sin(phi) * std::sin(th);

            m.positions.push_back(c.x + rx * sx);
            m.positions.push_back(c.y + ry * sy);
            m.positions.push_back(c.z + rz * sz);

            const Vec3 n = v3norm(v3(sx / rx, sy / ry, sz / rz));
            m.normals.push_back(n.x);
            m.normals.push_back(n.y);
            m.normals.push_back(n.z);

            m.colors.push_back(col[0]);
            m.colors.push_back(col[1]);
            m.colors.push_back(col[2]);
        }
    }

    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j)
        {
            const GLuint a = base + (GLuint)(i * (slices + 1) + j);
            const GLuint b = a + (GLuint)(slices + 1);
            m.indices.push_back(a);     m.indices.push_back(a + 1); m.indices.push_back(b);
            m.indices.push_back(a + 1); m.indices.push_back(b + 1); m.indices.push_back(b);
        }
    m.hasNormals = true;
    m.hasColors  = true;
}

void appendTransformed(MeshData& out, const MeshData& part, const float m[16])
{
    const GLuint base = (GLuint)(out.positions.size() / 3);
    const size_t nv = part.positions.size() / 3;
    for (size_t i = 0; i < nv; ++i)
    {
        const Vec3 p = mat4MulPoint(m, v3(part.positions[i * 3], part.positions[i * 3 + 1],
                                          part.positions[i * 3 + 2]));
        out.positions.push_back(p.x);
        out.positions.push_back(p.y);
        out.positions.push_back(p.z);

        const Vec3 d = mat4MulDir(m, v3(part.normals[i * 3], part.normals[i * 3 + 1],
                                        part.normals[i * 3 + 2]));
        const Vec3 n = v3norm(d);
        out.normals.push_back(n.x);
        out.normals.push_back(n.y);
        out.normals.push_back(n.z);

        out.colors.push_back(part.colors[i * 3]);
        out.colors.push_back(part.colors[i * 3 + 1]);
        out.colors.push_back(part.colors[i * 3 + 2]);
    }
    for (GLuint idx : part.indices) out.indices.push_back(base + idx);
    out.hasNormals = true;
    out.hasColors  = true;
}

namespace {

// ---------------------------------------------------------------------------
// 地面：细分网格 + 平滑的草色变化（细分是为了逐顶点雾不出硬边）
// ---------------------------------------------------------------------------
void addGround(MeshData& m, float halfSize, int div)
{
    const GLuint base = static_cast<GLuint>(m.positions.size()/3);
    const float step=2.0f*halfSize/div;
    for(int i=0;i<=div;++i) for(int j=0;j<=div;++j) {
        float x=-halfSize+step*i, z=-halfSize+step*j;
        float g=0.5f+0.5f*std::sin(x*0.23f)*std::cos(z*0.19f);
        float d=0.5f+0.5f*std::sin(x*0.07f+2.1f)*std::sin(z*0.09f-1.3f);
        m.positions.insert(m.positions.end(),{x,0.0f,z});
        m.normals.insert(m.normals.end(),{0.0f,1.0f,0.0f});
        m.colors.insert(m.colors.end(),{0.12f+0.09f*g+0.07f*d,0.25f+0.15f*g+0.03f*d,0.09f+0.06f*g+0.03f*d});
    }
    for(int i=0;i<div;++i) for(int j=0;j<div;++j) {
        GLuint a=base+i*(div+1)+j, b=a+1, c=b+div+1, d=a+div+1;
        m.indices.insert(m.indices.end(),{a,b,c,a,c,d});
    }
    m.hasNormals=true; m.hasColors=true;
}

// ---------------------------------------------------------------------------
// L-system 分形树
//
// 文法（只重写 F，其它符号原样保留）：
//   F   沿当前方向长一段枝（锥台），末端小枝挂叶
//   + - 绕「上」参考轴左/右偏航
//   & ^ 绕「左」参考轴俯/仰
//   / \ 绕前进方向翻滚（让分支在三维里错开，不共面）
//   [ ] 海龟状态入栈 / 出栈（分支）
// ---------------------------------------------------------------------------

// 运行期参数：迭代深度增减与分支角缩放（菜单/按键调整，重建森林生效）
int   g_detailBump  = 0;
float g_angleScale  = 1.0f;

struct LTreeRule
{
    const char* axiom;
    const char* expansion;      // F 的重写结果
    int         iterations;     // 基准迭代代数
    float       angleDeg;       // 分支角
    float       lenShrink;      // 每级分支长度衰减
    float       radShrink;      // 每级分支半径衰减
    float       leafSize;       // 末端叶团尺寸（相对单位树高）
    bool        leafIsCone;     // 针叶用小锥，阔叶用椭球团
    float       trunkCol[3];
    float       leafCol[3];
};

// 三套物种预设：迭代代数在 3 上下，展开后每棵树约 40~70 段枝
const LTreeRule kConifer = {
    "F", "F[&F][&/F]F", 3, 30.0f, 0.86f, 0.66f, 0.085f, true,
    { 0.21f, 0.14f, 0.07f }, { 0.10f, 0.26f, 0.10f }
};
const LTreeRule kBroadleaf = {
    "F", "F[+&F][-&F]F", 3, 40.0f, 0.84f, 0.62f, 0.16f, false,
    { 0.26f, 0.18f, 0.10f }, { 0.15f, 0.35f, 0.12f }
};
const LTreeRule kBush = {
    "[F][+F][-F]", "F[+F][-F]F", 2, 44.0f, 0.80f, 0.70f, 0.13f, false,
    { 0.24f, 0.17f, 0.09f }, { 0.18f, 0.33f, 0.10f }
};

// 字符串重写：迭代 depth 代
std::string lExpand(const LTreeRule& rule, int depth)
{
    std::string s = rule.axiom;
    for (int it = 0; it < depth; ++it)
    {
        std::string next;
        next.reserve(s.size() * 8);
        for (char c : s)
        {
            if (c == 'F') next += rule.expansion;
            else          next += c;
        }
        s.swap(next);
    }
    return s;
}

// 海龟状态：位置 + 正交坐标系（heading 前进方向 / up 俯仰参考 / left 翻滚参考）
struct Turtle
{
    Vec3  pos, heading, up, left;
    float len;          // 当前段长
    float radius;       // 当前段半径
    int   depth;        // 分支层级（决定叶团与尺寸衰减）
};

// 任意朝向的锥台枝干。绕序与 addCylinderSide 同构（外法线，CCW）。
void addSegment(MeshData& m, const Vec3& p0, const Vec3& p1,
                float r0, float r1, const float col[3], int sides)
{
    const Vec3 axis = p1 - p0;
    const float h = v3len(axis);
    if (h < 1e-6f) return;
    const Vec3 d = axis * (1.0f / h);

    Vec3 u = v3cross(d, v3(0.0f, 1.0f, 0.0f));       // 与竖直不共线时的横向基
    if (v3len(u) < 1e-5f) u = v3(1.0f, 0.0f, 0.0f);
    u = v3norm(u);
    const Vec3 v = v3cross(d, u);

    for (int i = 0; i < sides; ++i)
    {
        const float t0 = 2.0f * kPi * (float)i / (float)sides;
        const float t1 = 2.0f * kPi * (float)(i + 1) / (float)sides;

        const Vec3 r0u = u * (r0 * std::cos(t0)) + v * (r0 * std::sin(t0));
        const Vec3 r0w = u * (r0 * std::cos(t1)) + v * (r0 * std::sin(t1));
        const Vec3 r1u = u * (r1 * std::cos(t0)) + v * (r1 * std::sin(t0));
        const Vec3 r1w = u * (r1 * std::cos(t1)) + v * (r1 * std::sin(t1));

        pushTri(m, p0 + r0u, p1 + r1u, p1 + r1w, col);
        pushTri(m, p0 + r0u, p1 + r1w, p0 + r0w, col);
    }
}

// 解释字符串生成一棵「单位高度」的树（底面 y=0，最高点 y=1）
void buildFractalTree(MeshData& out, Rng& rng, const LTreeRule& rule, int depth)
{
    const std::string s = lExpand(rule, depth);
    const float angle = rule.angleDeg * g_angleScale;

    std::vector<Turtle> stack;
    Turtle t;
    t.pos     = v3(0.0f, 0.0f, 0.0f);
    t.heading = v3(0.0f, 1.0f, 0.0f);
    t.up      = v3(0.0f, 0.0f, 1.0f);
    t.left    = v3(1.0f, 0.0f, 0.0f);
    t.len     = 0.34f;                       // 首段后由 bbox 归一化，绝对值不敏感
    t.radius  = 0.05f;
    t.depth   = 0;

    const int   leafDepth = depth;           // 最外层分支挂叶
    const int   sides     = 5;

    auto yaw   = [&](float deg) { const float r = deg * kPi / 180.0f;
                                  quatRotate(r, t.up.x, t.up.y, t.up.z, t.heading.x, t.heading.y, t.heading.z);
                                  quatRotate(r, t.up.x, t.up.y, t.up.z, t.left.x,   t.left.y,   t.left.z); };
    auto pitch = [&](float deg) { const float r = deg * kPi / 180.0f;
                                  quatRotate(r, t.left.x, t.left.y, t.left.z, t.heading.x, t.heading.y, t.heading.z);
                                  quatRotate(r, t.left.x, t.left.y, t.left.z, t.up.x,      t.up.y,      t.up.z); };
    auto roll  = [&](float deg) { const float r = deg * kPi / 180.0f;
                                  quatRotate(r, t.heading.x, t.heading.y, t.heading.z, t.up.x,   t.up.y,   t.up.z);
                                  quatRotate(r, t.heading.x, t.heading.y, t.heading.z, t.left.x, t.left.y, t.left.z); };

    for (char c : s)
    {
        switch (c)
        {
        case 'F':
        {
            // 每棵树在固定规则上叠加少量确定性抖动，同种树不重样
            const float jitterL = 1.0f + rng.range(-0.10f, 0.10f);
            const float jit = 1.0f + rng.range(-0.12f, 0.12f);
            const Vec3 p1 = t.pos + t.heading * (t.len * jitterL);

            const float leafy = (t.depth >= leafDepth - 1) ? 0.45f : 1.0f;   // 末梢细一些
            addSegment(out, t.pos, p1, t.radius * leafy, t.radius * leafy * 0.7f,
                       rule.trunkCol, sides);

            if (t.depth >= leafDepth - 1)
            {
                // 末端叶团：针叶用小锥（贴枝），阔叶用椭球团（裹枝头）
                const float lv = rng.range(0.0f, 0.12f);
                const float lc[3] = { rule.leafCol[0] + lv * 0.5f,
                                      rule.leafCol[1] + lv,
                                      rule.leafCol[2] + lv * 0.3f };
                const Vec3 lcPos = p1 + t.heading * (t.len * 0.4f);
                if (rule.leafIsCone)
                {
                    MeshData blob;
                    const float s = rule.leafSize * 3.2f * jit;
                    addCone(blob, 0.0f, s, s * 0.42f, lc, 4);
                    float m[16];
                    // 小锥沿枝方向立起来：构造 heading 为主轴的旋转
                    const Vec3 h = t.heading;
                    const float a = std::acos(h.y) * 180.0f / kPi;       // 与 +Y 的夹角
                    const Vec3 ax = v3norm(v3cross(v3(0, 1, 0), h));
                    if (v3len(ax) > 1e-5f) mat4FromAxisAngle(m, a, ax.x, ax.y, ax.z);
                    else                  mat4Identity(m);
                    m[12] = lcPos.x; m[13] = lcPos.y; m[14] = lcPos.z;
                    appendTransformed(out, blob, m);
                }
                else
                {
                    const float r0 = rule.leafSize * jit;
                    addEllipsoid(out, lcPos, r0, r0 * 0.85f, r0, lc, 6, 4);
                }
            }
            t.pos = p1;
            break;
        }
        case '+': yaw(  angle * rng.range(0.85f, 1.15f)); break;
        case '-': yaw( -angle * rng.range(0.85f, 1.15f)); break;
        case '&': pitch( angle * rng.range(0.85f, 1.15f)); break;
        case '^': pitch(-angle * rng.range(0.85f, 1.15f)); break;
        case '/': roll( 120.0f * rng.range(0.6f, 1.4f));  break;   // 三分支错开约 120°
        case '\\': roll(-120.0f * rng.range(0.6f, 1.4f)); break;
        case '[':
            stack.push_back(t);
            t.len    *= rule.lenShrink;
            t.radius *= rule.radShrink;
            t.depth  += 1;
            break;
        case ']':
            if (!stack.empty()) { t = stack.back(); stack.pop_back(); }
            break;
        default: break;
        }
    }
}

// 归一化到「高度 1、底面 y=0」，摆树时统一缩放
void normalizeTree(MeshData& tree)
{
    float minY = 1e30f, maxY = -1e30f;
    for (size_t i = 0; i < tree.positions.size(); i += 3)
    {
        minY = std::min(minY, tree.positions[i + 1]);
        maxY = std::max(maxY, tree.positions[i + 1]);
    }
    const float h = maxY - minY;
    if (h < 1e-6f) return;
    for (size_t i = 0; i < tree.positions.size(); i += 3)
        tree.positions[i + 1] = (tree.positions[i + 1] - minY) / h;
}

// 把一棵「单位高度」的树缩放 height 倍后平移到 (x, z)，随机偏航后追加进 out。
void appendScaled(MeshData& out, const MeshData& tree,
                  float x, float z, float height, float yawRad)
{
    const GLuint base = (GLuint)(out.positions.size() / 3);
    const float cs = std::cos(yawRad), sn = std::sin(yawRad);

    const size_t nv = tree.positions.size() / 3;
    for (size_t i = 0; i < nv; ++i)
    {
        const float px = tree.positions[i * 3 + 0] * height;
        const float py = tree.positions[i * 3 + 1] * height;
        const float pz = tree.positions[i * 3 + 2] * height;

        out.positions.push_back(px * cs + pz * sn + x);
        out.positions.push_back(py);
        out.positions.push_back(-px * sn + pz * cs + z);

        out.normals.push_back(tree.normals[i * 3 + 0]);
        out.normals.push_back(tree.normals[i * 3 + 1]);
        out.normals.push_back(tree.normals[i * 3 + 2]);

        out.colors.push_back(tree.colors[i * 3 + 0]);
        out.colors.push_back(tree.colors[i * 3 + 1]);
        out.colors.push_back(tree.colors[i * 3 + 2]);
    }

    for (GLuint idx : tree.indices)
        out.indices.push_back(base + idx);

    out.hasNormals = true;
    out.hasColors  = true;
}

// 点到路径采样点的最近距离平方（XZ 平面）
float distSqToPath(const std::vector<Vec3>& path, float x, float z)
{
    float best = 1e30f;
    for (const Vec3& p : path)
    {
        const float dx = p.x - x;
        const float dz = p.z - z;
        const float d2 = dx * dx + dz * dz;
        if (d2 < best) best = d2;
    }
    return best;
}

bool spotBlocked(const std::vector<Vec3>& path, float clearSq, float x, float z)
{
    return distSqToPath(path, x, z) < clearSq;
}

} // namespace

// ---------------------------------------------------------------------------
// 运行期分形参数
// ---------------------------------------------------------------------------
void forestSetDetail(int bump)  { g_detailBump = bump; }
int  forestDetail()             { return g_detailBump; }
void forestAdjustAngle(float scale) { g_angleScale = std::max(0.5f, std::min(g_angleScale * scale, 1.8f)); }
const char* forestParamsText()
{
    static char buf[64];
    std::snprintf(buf, sizeof(buf), "depth%s, angle x%.2f",
                  g_detailBump == 0 ? "3" : (g_detailBump > 0 ? "3+1" : "3-1"),
                  g_angleScale);
    return buf;
}

// ---------------------------------------------------------------------------
// 整片森林：地面 + 分形树 + 花草
// ---------------------------------------------------------------------------
void buildForest(MeshData& out,
                 float halfSize,
                 int   treeCount,
                 const std::vector<Vec3>& path,
                 float clearance,
                 unsigned seed)
{
    out = MeshData{};
    Rng rng(seed);

    addGround(out, halfSize, 48);

    // 拒绝采样：太靠近漫步路线的点丢弃
    const float clearSq = clearance * clearance;
    const float margin  = 2.5f;
    const int   maxTries = treeCount * 30;

    MeshData tree;
    int placed = 0;

    for (int tries = 0; tries < maxTries && placed < treeCount; ++tries)
    {
        const float x = rng.range(-halfSize + margin, halfSize - margin);
        const float z = rng.range(-halfSize + margin, halfSize - margin);
        if (spotBlocked(path, clearSq, x, z)) continue;

        tree = MeshData{};

        const float pick = rng.unit();
        const LTreeRule* rule;
        float height;
        int depth;
        if (pick < 0.42f)      { rule = &kConifer;   height = rng.range(11.0f, 22.0f); }
        else if (pick < 0.75f) { rule = &kBroadleaf; height = rng.range(8.0f, 16.0f);  }
        else                   { rule = &kBush;      height = rng.range(1.8f, 3.6f);   }
        depth = std::max(1, rule->iterations + g_detailBump);

        buildFractalTree(tree, rng, *rule, depth);
        normalizeTree(tree);
        appendScaled(out, tree, x, z, height, rng.range(0.0f, 2.0f * kPi));
        ++placed;
    }

    // ---- 花草（同样避开路线；草簇 + 花 = 茎 + 花头）----
    static const float kFlowerCols[][3] = {
        { 0.95f, 0.62f, 0.72f },   // 粉
        { 0.95f, 0.93f, 0.82f },   // 白
        { 0.96f, 0.84f, 0.30f },   // 黄
    };

    const int grassCount = 220;
    for (int i = 0; i < grassCount; ++i)
    {
        const float x = rng.range(-halfSize + 1.0f, halfSize - 1.0f);
        const float z = rng.range(-halfSize + 1.0f, halfSize - 1.0f);
        if (spotBlocked(path, 2.2f * 2.2f, x, z)) continue;

        const int blades = 3 + (int)rng.unit() * 3;         // 3~5 根一簇
        for (int b = 0; b < blades; ++b)
        {
            const float h = rng.range(0.10f, 0.30f);
            const float g = rng.range(0.0f, 0.10f);
            const float col[3] = { 0.14f + g * 0.5f, 0.30f + g, 0.10f + g * 0.4f };

            MeshData blade;
            addCone(blade, 0.0f, h, rng.range(0.012f, 0.022f), col, 3);
            float m[16];
            mat4Identity(m);
            m[12] = x + rng.range(-0.12f, 0.12f);
            m[13] = 0.0f;
            m[14] = z + rng.range(-0.12f, 0.12f);
            appendTransformed(out, blade, m);
        }
    }

    const int flowerCount = 70;
    for (int i = 0; i < flowerCount; ++i)
    {
        const float x = rng.range(-halfSize + 1.0f, halfSize - 1.0f);
        const float z = rng.range(-halfSize + 1.0f, halfSize - 1.0f);
        if (spotBlocked(path, 2.2f * 2.2f, x, z)) continue;

        const float stemH = rng.range(0.22f, 0.34f);
        const float stem[3] = { 0.20f, 0.38f, 0.14f };

        MeshData flower;
        addCylinderSide(flower, 0.0f, stemH, 0.010f, 0.006f, stem, 3);
        addEllipsoid(flower, v3(0.0f, stemH + 0.03f, 0.0f), 0.035f, 0.026f, 0.035f,
                     kFlowerCols[i % 3], 6, 4);

        float m[16];
        mat4Identity(m);
        m[12] = x; m[13] = 0.0f; m[14] = z;
        appendTransformed(out, flower, m);
    }
}
