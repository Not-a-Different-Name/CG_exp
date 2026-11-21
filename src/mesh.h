#pragma once
// 网格：持有 OBJ 解析结果 + 纹理，并负责绘制。
// 绘制用「客户端顶点数组 + 一次 glDrawElements」，替代指导书里逐三角形的
// glBegin/glEnd —— 对 76704 个三角形这是 1 次 GL 调用 vs 约 46 万次。
#include "obj_loader.h"
#include <GL/freeglut.h>
#include <string>

// 模型的「上」是哪根轴。Auto 会按几何自动判断，判断不出来的场合可以显式指定。
//   - 原来那只兔子（3ds Max 导出）是 Z-up
//   - Stanford Bunny 是 Y-up，而且底面就在 y = 0
enum class UpAxis { Auto, Y, Z };

class Mesh
{
public:
    Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    // 读取 OBJ。up != Y 时把模型旋转成 Y-up。
    // normalize=true 时把模型平移到原点并缩放到单位包围球，这样取景和相机距离
    // 就与模型原始单位无关（老兔子原始半径 25.42、高 40 单位；新兔子只有 ~1 单位高）。
    bool loadFromOBJ(const std::string& path, UpAxis up, bool normalize,
                     std::string* errOut = nullptr);

    // 程序化构造（森林等）：直接塞顶点数据进去，不经过文件
    void adopt(MeshData&& data);
    void adoptSkin(MeshData&& data, const unsigned char* pixels);

    // 用当前材质设定 + 纹理绘制。useTexture 关掉时退化为纯材质着色。
    void draw(bool useTexture) const;
    // 只发 draw call，不碰材质/纹理（由 glPolygonMode 决定填充方式）
    void drawRaw(bool useTexture) const;

    void applyMaterial() const;         // 把 .mtl 材质喂给固定管线
    void bindTexture(bool useTexture) const;

    void release();                  // 删除纹理、释放内存

    const MeshData& data() const { return data_; }
    bool  hasTexture() const { return tex_ != 0; }
    void  preserveSmallTexturePixels() const;
    float radius()     const { return data_.radius; }
    const float* center() const { return data_.center; }

private:
    void upload() const;
    mutable GLuint buffers_[5] = {}; // position, normal, UV, colour, indices
    MeshData data_;
    GLuint   tex_ = 0;
    int      texW_ = 0, texH_ = 0;
};
