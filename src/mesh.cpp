#include "mesh.h"
#include "texture.h"
#include "gl_api.h"

#include <cmath>
#include <cstdio>

namespace {

// Z-up → Y-up：绕 X 轴 -90°，(x,y,z) → (x, z, -y)。
// 本兔子模型是 Z-up 且高 40 单位，不转换的话「正视图」看到的是脚底板。
void rotateZUpToYUp(MeshData& m)
{
    const size_t nv = m.positions.size() / 3;
    for (size_t i = 0; i < nv; ++i)
    {
        float& y = m.positions[i * 3 + 1];
        float& z = m.positions[i * 3 + 2];
        const float ny = z, nz = -y;
        y = ny; z = nz;
    }
    for (size_t i = 0; i < m.normals.size() / 3; ++i)
    {
        float& y = m.normals[i * 3 + 1];
        float& z = m.normals[i * 3 + 2];
        const float ny = z, nz = -y;
        y = ny; z = nz;
    }
}

// 旋转后重新计算包围盒 / 包围球（三视图共用缩放要用到）
void recomputeBounds(MeshData& m)
{
    const size_t nv = m.positions.size() / 3;
    if (nv == 0) return;

    float mn[3] = { m.positions[0], m.positions[1], m.positions[2] };
    float mx[3] = { mn[0], mn[1], mn[2] };
    for (size_t i = 1; i < nv; ++i)
    {
        for (int a = 0; a < 3; ++a)
        {
            const float v = m.positions[i * 3 + a];
            if (v < mn[a]) mn[a] = v;
            if (v > mx[a]) mx[a] = v;
        }
    }
    for (int a = 0; a < 3; ++a)
    {
        m.bboxMin[a] = mn[a];
        m.bboxMax[a] = mx[a];
        m.center[a]  = (mn[a] + mx[a]) * 0.5f;
    }

    float maxDistSq = 0.0f;
    for (size_t i = 0; i < nv; ++i)
    {
        const float dx = m.positions[i * 3 + 0] - m.center[0];
        const float dy = m.positions[i * 3 + 1] - m.center[1];
        const float dz = m.positions[i * 3 + 2] - m.center[2];
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > maxDistSq) maxDistSq = d2;
    }
    m.radius = std::sqrt(maxDistSq);
    if (m.radius < 1e-6f) m.radius = 1.0f;
}

// 平移到原点 + 缩放到单位包围球
void normalizeToUnitSphere(MeshData& m)
{
    if (m.radius < 1e-8f) return;
    const float s = 1.0f / m.radius;
    const size_t nv = m.positions.size() / 3;
    for (size_t i = 0; i < nv; ++i)
    {
        m.positions[i * 3 + 0] = (m.positions[i * 3 + 0] - m.center[0]) * s;
        m.positions[i * 3 + 1] = (m.positions[i * 3 + 1] - m.center[1]) * s;
        m.positions[i * 3 + 2] = (m.positions[i * 3 + 2] - m.center[2]) * s;
    }
    recomputeBounds(m);      // 归一化后 center≈0、radius≈1
}

// 判断模型的「上」是哪根轴：取「底面贴着的那根轴」，也就是 min 值相对自身跨度
// 最小的那根。归一化之后这个比例能很好地区分两种常见朝向：
//   老兔子   z[0.02, 40.21]  -> ratio 0.0005  -> Z-up
//   新兔子   y[0.00,  1.03]  -> ratio 0.0000  -> Y-up
// 只用 Y / Z 两种结果：实际模型基本只有这两种朝向，X-up 的按 Z-up 处理。
UpAxis detectUpAxis(const MeshData& d)
{
    float best = 1e30f;
    int   bestAxis = 2;
    for (int a = 0; a < 3; ++a)
    {
        const float ext = d.bboxMax[a] - d.bboxMin[a];
        if (ext < 1e-9f) continue;
        const float ratio = std::fabs(d.bboxMin[a]) / ext;
        if (ratio < best) { best = ratio; bestAxis = a; }
    }
    return (bestAxis == 1) ? UpAxis::Y : UpAxis::Z;
}

} // namespace

void Mesh::adopt(MeshData&& data)
{
    release();
    data_ = std::move(data);
    recomputeBounds(data_);
    // 这两个统计量平时由 loadOBJ 填，程序化生成的网格要自己补上，
    // 否则 HUD 和日志里的三角形数会显示成 0
    data_.uniqueVertexCount = (int)(data_.positions.size() / 3);
    data_.triangleCount     = (int)(data_.indices.size() / 3);
    data_.valid             = true;
}

void Mesh::adoptSkin(MeshData&& data, const unsigned char* pixels)
{
    adopt(std::move(data));
    tex_ = uploadSkinPixels(pixels);
    texW_ = texH_ = 64;
}

bool Mesh::loadFromOBJ(const std::string& path, UpAxis up, bool normalize, std::string* errOut)
{
    MeshData loaded;
    if (!loadOBJ(path, loaded, errOut)) return false;
    release();
    data_ = std::move(loaded);

    if (up == UpAxis::Auto) up = detectUpAxis(data_);
    if (up == UpAxis::Z) rotateZUpToYUp(data_);
    recomputeBounds(data_);
    if (normalize) normalizeToUnitSphere(data_);

    if (!data_.diffuseMapPath.empty())
    {
        tex_ = loadTexture2D(data_.diffuseMapPath, &texW_, &texH_);
        if (tex_ == 0)
            std::fprintf(stderr, "[mesh] texture load failed; material colour only\n");
    }

    std::printf("[mesh] %s  (up axis = %s)\n", path.c_str(),
                up == UpAxis::Y ? "Y (no rotation)" : "Z -> rotated to Y");
    std::printf("       vertices %d -> unique %d | faces %d (quads %d) -> triangles %d\n",
                data_.srcVertexCount, data_.uniqueVertexCount,
                data_.srcFaceCount, data_.quadFaceCount, data_.triangleCount);
    std::printf("       bbox x[%.2f,%.2f] y[%.2f,%.2f] z[%.2f,%.2f]  radius %.2f\n",
                data_.bboxMin[0], data_.bboxMax[0],
                data_.bboxMin[1], data_.bboxMax[1],
                data_.bboxMin[2], data_.bboxMax[2], data_.radius);
    return true;
}

void Mesh::applyMaterial() const
{
    const ObjMaterial& mtl = data_.material;

    // 有贴图时把漫反射设为白色：GL_MODULATE 会把纹理乘以材质色，
    // 若沿用 .mtl 的 Kd=0.5882，兔子会整体发灰。
    float kd[4] = { mtl.diffuse[0], mtl.diffuse[1], mtl.diffuse[2], 1.0f };
    if (hasTexture()) kd[0] = kd[1] = kd[2] = 1.0f;

    // Ka 调小：0.5882 的环境光叠加 GL_LIGHT_MODEL_AMBIENT 会让模型发白变平。
    const float ka[4] = { mtl.ambient[0] * 0.35f, mtl.ambient[1] * 0.35f, mtl.ambient[2] * 0.35f, 1.0f };
    // Ke 置零：.mtl 里的 0.1765 自发光会让兔子显得发乳白。
    const float ke[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   ka);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   kd);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  mtl.specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION,  ke);
    glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, mtl.shininess);
}

void Mesh::bindTexture(bool useTexture) const
{
    if (tex_ != 0 && useTexture)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex_);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
    }
}

void Mesh::preserveSmallTexturePixels() const
{
    if (!tex_ || texW_ > 128 || texH_ > 128) return;
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Mesh::upload() const
{
    if (buffers_[0] || !glapi::GenBuffers || !glapi::BufferData || !glapi::BindBuffer) return;
    glapi::GenBuffers(5, buffers_);
    auto uploadArray = [&](int slot, const std::vector<GLfloat>& values) {
        glapi::BindBuffer(0x8892, buffers_[slot]);
        glapi::BufferData(0x8892, values.size()*sizeof(GLfloat), values.data(), 0x88E4);
    };
    uploadArray(0, data_.positions); uploadArray(1, data_.normals);
    uploadArray(2, data_.uvs); uploadArray(3, data_.colors);
    glapi::BindBuffer(0x8893, buffers_[4]);
    glapi::BufferData(0x8893, data_.indices.size()*sizeof(GLuint), data_.indices.data(), 0x88E4);
    glapi::BindBuffer(0x8892, 0); glapi::BindBuffer(0x8893, 0);
}

void Mesh::drawRaw(bool useTexture) const
{
    if (data_.indices.empty()) return;
    upload();
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    auto pointer = [&](int slot, const std::vector<GLfloat>& values) -> const void* {
        if (!buffers_[0]) return values.data();
        glapi::BindBuffer(0x8892, buffers_[slot]); return nullptr;
    };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, pointer(0, data_.positions));
    if (data_.hasNormals) {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, 0, pointer(1, data_.normals));
    } else { glDisableClientState(GL_NORMAL_ARRAY); glNormal3f(0,1,0); }
    const bool useTex = tex_ && useTexture && data_.hasUVs;
    if (useTex) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, pointer(2, data_.uvs));
    } else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    if (data_.hasColors) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(3, GL_FLOAT, 0, pointer(3, data_.colors));
    } else glDisableClientState(GL_COLOR_ARRAY);
    if (buffers_[0]) glapi::BindBuffer(0x8893, buffers_[4]);
    glDrawElements(GL_TRIANGLES, (GLsizei)data_.indices.size(), GL_UNSIGNED_INT,
                   buffers_[0] ? nullptr : data_.indices.data());
    if (buffers_[0]) { glapi::BindBuffer(0x8892,0); glapi::BindBuffer(0x8893,0); }
    glPopClientAttrib();
}

void Mesh::draw(bool useTexture) const
{
    if (data_.hasColors)
    {
        // 顶点色驱动材质。注意 GL_COLOR_MATERIAL 一开，glMaterialfv 的
        // AMBIENT/DIFFUSE 就被忽略，所以这里不调 applyMaterial，而是把材质设成
        // 中性偏白 —— 否则顶点色会和材质色相乘，整片森林变暗。
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        const float white[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };
        const float ambient[4] = { 0.45f, 0.45f, 0.45f, 1.0f };
        // 高光要压得很低：草地/树叶基本是漫反射材质，给 0.45 的镜面反射
        // 会在地面上烧出一块明显的光斑，非常假
        const float specular[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   white);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  specular);
        glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 8.0f);
    }
    else
    {
        // 用 .mtl 材质时必须关掉 GL_COLOR_MATERIAL，否则材质会被残留的"当前颜色"覆盖
        glDisable(GL_COLOR_MATERIAL);
        // 关掉光照时材质不起作用，模型会沿用一个残留的 glColor —— 显式设成浅灰，
        // 否则可能整只兔子被上一次 glColor 染成奇怪的颜色。
        if (!glIsEnabled(GL_LIGHTING))
            glColor3f(0.82f, 0.80f, 0.78f);
        applyMaterial();
    }

    bindTexture(useTexture);
    drawRaw(useTexture);
    glDisable(GL_TEXTURE_2D);
}

void Mesh::release()
{
    if (buffers_[0] && glapi::DeleteBuffers) glapi::DeleteBuffers(5, buffers_);
    for (auto& buffer : buffers_) buffer = 0;
    if (tex_ != 0)
    {
        glDeleteTextures(1, &tex_);
        tex_ = 0;
    }
    data_ = MeshData{};
}
