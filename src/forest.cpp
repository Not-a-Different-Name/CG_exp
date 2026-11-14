#include "forest.h"

#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// 确定性伪随机：xorshift32。固定种子保证每次运行生成完全一样的森林，录像才可复现。
// （不能用 rand()：各 C 运行库的实现不同，换个编译器森林就变了。）
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

// ---------------------------------------------------------------------------
// 往 MeshData 里塞一个三角形（平面法线）。绕序必须 CCW，否则会被背面剔除。
// ---------------------------------------------------------------------------
void pushTri(MeshData& m,
             const Vec3& a, const Vec3& b, const Vec3& c,
             const float col[3])
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

// 竖直圆柱侧面（可锥形：r0 在 y0，r1 在 y1）。
// 绕序：在 θ=0 处取 p0=(r,y0,0)，p1≈(r,y0,r·dθ)，q0=(r,y1,0)，q1≈(r,y1,r·dθ)，
// 算 (p0,q0,q1) 的叉积得 +X，正是该处的外法线方向。
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

// 圆锥：底面圆 (y0, r) 收到顶点 (y1)。
void addCone(MeshData& m, float y0, float y1, float r, const float col[3], int sides)
{
    const Vec3 apex(0.0f, y1, 0.0f);
    for (int i = 0; i < sides; ++i)
    {
        const float t0 = 2.0f * kPi * (float)i / (float)sides;
        const float t1 = 2.0f * kPi * (float)(i + 1) / (float)sides;
        const Vec3 p0(r * std::cos(t0), y0, r * std::sin(t0));
        const Vec3 p1(r * std::cos(t1), y0, r * std::sin(t1));

        // 侧面：(p0, apex, p1) 叉积得 (h·rdθ, r²dθ, 0)，朝外且略微朝上 —— 正确。
        // 反过来写成 (p0, p1, apex) 会变成朝内，整个锥体被剔除。
        pushTri(m, p0, apex, p1, col);
        pushTri(m, v3(0.0f, y0, 0.0f), p0, p1, col);      // 底盖朝下
    }
}

// 椭球（树叶团）。用解析法线做平滑着色，比平面法线更像一团叶子。
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

            // 椭球在 (rx·sx, ry·sy, rz·sz) 处的法线正比于 (sx/rx, sy/ry, sz/rz)
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
    {
        for (int j = 0; j < slices; ++j)
        {
            const GLuint a = base + (GLuint)(i * (slices + 1) + j);
            const GLuint b = a + (GLuint)(slices + 1);
            m.indices.push_back(a);     m.indices.push_back(a + 1); m.indices.push_back(b);
            m.indices.push_back(a + 1); m.indices.push_back(b + 1); m.indices.push_back(b);
        }
    }
    m.hasNormals = true;
    m.hasColors  = true;
}

// ---------------------------------------------------------------------------
// 地面：细分网格 + 平滑的草色变化。
// 必须细分：固定管线的雾是**逐顶点**算的，一整块大平面只有四个角有雾值，
// 远处会突变成一条硬边。
// ---------------------------------------------------------------------------
void addGround(MeshData& m, float halfSize, int div)
{
    // Shared vertices give adjacent cells identical colours, avoiding visible tile seams.
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
// 三种树型。都按「高度 1、底面在 y = 0、中心在原点」生成，
// 摆放时用 appendScaled 统一缩放平移。
// ---------------------------------------------------------------------------
void buildConifer(MeshData& t, Rng& rng)
{
    const float trunk[3] = { 0.20f, 0.13f, 0.07f };
    addCylinderSide(t, 0.0f, 0.30f, 0.030f, 0.020f, trunk, 6);

    const float b = rng.range(0.0f, 0.08f);
    const float g1[3] = { 0.10f + b, 0.24f + b, 0.09f + b * 0.5f };
    const float g2[3] = { 0.08f + b, 0.21f + b, 0.08f + b * 0.5f };

    addCone(t, 0.22f, 0.62f, 0.190f, g1, 8);
    addCone(t, 0.48f, 0.84f, 0.140f, g1, 8);
    addCone(t, 0.71f, 1.00f, 0.088f, g2, 8);
}

void buildRoundTree(MeshData& t, Rng& rng)
{
    const float trunk[3] = { 0.24f, 0.16f, 0.09f };
    addCylinderSide(t, 0.0f, 0.46f, 0.034f, 0.026f, trunk, 6);

    const float v = rng.range(0.0f, 0.45f);
    const float g1[3] = { 0.13f + v, 0.29f + v, 0.10f + v * 0.4f };
    const float g2[3] = { 0.10f + v, 0.25f + v, 0.08f + v * 0.4f };

    addEllipsoid(t, v3(rng.range(-0.02f, 0.02f), 0.62f, rng.range(-0.02f, 0.02f)),
                 0.24f, 0.21f, 0.24f, g1, 8, 5);
    addEllipsoid(t, v3(rng.range(-0.05f, 0.05f), 0.79f, rng.range(-0.05f, 0.05f)),
                 0.17f, 0.15f, 0.17f, g2, 8, 4);
    addEllipsoid(t, v3(rng.range(-0.05f, 0.05f), 0.53f, rng.range(-0.05f, 0.05f)),
                 0.16f, 0.14f, 0.16f, g2, 8, 4);
}

void buildBush(MeshData& t, Rng& rng)
{
    const float v = rng.range(0.0f, 0.5f);
    const float g1[3] = { 0.12f + v, 0.28f + v, 0.09f + v * 0.4f };
    const float g2[3] = { 0.09f + v, 0.24f + v, 0.07f + v * 0.4f };

    addEllipsoid(t, v3(0.0f, 0.45f, 0.0f), 0.42f, 0.45f, 0.42f, g1, 8, 5);
    addEllipsoid(t, v3(rng.range(-0.10f, 0.10f), 0.62f, rng.range(-0.10f, 0.10f)),
                 0.30f, 0.30f, 0.30f, g2, 8, 4);
}

// 把一棵「单位高度」的树缩放 height 倍后平移到 (x, z)，索引偏移后追加进 out。
// 均匀缩放不改变法线方向，所以法线可以直接照搬。
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

        // 绕 Y 转一圈，让同一种树看起来不重样
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

} // namespace

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

    // 拒绝采样：随机撒点，太靠近兔子路线的丢掉，否则兔子会从树干里穿过去。
    // 用尝试次数上限而不是 while(true)，参数不合理时也不会死循环。
    const float clearSq = clearance * clearance;
    const float margin  = 2.5f;
    const int   maxTries = treeCount * 30;

    MeshData tree;
    int placed = 0;

    for (int tries = 0; tries < maxTries && placed < treeCount; ++tries)
    {
        const float x = rng.range(-halfSize + margin, halfSize - margin);
        const float z = rng.range(-halfSize + margin, halfSize - margin);
        if (distSqToPath(path, x, z) < clearSq) continue;

        tree = MeshData{};                       // 复用同一个临时缓冲

        const float pick = rng.unit();
        float height;
        if (pick < 0.42f)
        {
            buildConifer(tree, rng);
            height = rng.range(11.0f, 22.0f);
        }
        else if (pick < 0.75f)
        {
            buildRoundTree(tree, rng);
            height = rng.range(8.0f, 16.0f);
        }
        else
        {
            buildBush(tree, rng);
            height = rng.range(1.8f, 3.6f);      // 灌木本来就矮
        }

        // 针叶树保持竖直；阔叶树稍微倾斜一点更自然
        // （倾斜要绕水平轴，这里简单起见只做随机偏航 + 轻微高度差异）
        appendScaled(out, tree, x, z, height, rng.range(0.0f, 2.0f * kPi));
        ++placed;
    }
}
