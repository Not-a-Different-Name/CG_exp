#pragma once
// 实验十：OBJ / MTL 模型导入。
//
// 与指导书示例的差别（都是为了让兔子这种真实模型能跑起来）：
//   1. 指导书只支持三角形面，本模型 38352 个面**全部是四边形**，必须扇形三角化；
//   2. 指导书逐三角形 glBegin/glEnd 绘制，7.6 万面每帧约 46 万次立即模式调用，
//      这里改成去重后的统一顶点缓冲 + glDrawElements，一次调用画完；
//   3. 指导书用平滑法线生成（带 90 度夹角阈值），本模型自带 38353 个 vn，直接用。
#include <string>
#include <vector>
// 用 <GL/freeglut.h>（而不是裸 <GL/gl.h>）：它会先定义 WIN32_LEAN_AND_MEAN /
// NOMINMAX 再引入 <windows.h>，从而给 <GL/gl.h> 提供 WINGDIAPI 和 APIENTRY。
// 直接包含 <GL/gl.h> 会因缺少这两个宏而报 C2182/C2086。指导书的 OBJImporter.h
// 也是以 <GL/glut.h> 开头，思路一致。
#include <GL/freeglut.h>

// .mtl 里的一个材质
struct ObjMaterial
{
    std::string name;
    float ambient[4]  = { 0.2f, 0.2f, 0.2f, 1.0f };
    float diffuse[4]  = { 0.8f, 0.8f, 0.8f, 1.0f };
    float specular[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float emission[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float shininess   = 65.0f;          // 已从 [0,1000] 映射到 OpenGL 的 [0,128]
    std::string diffuseMap;             // map_Kd，相对 .mtl 所在目录
};

// 去重后的渲染数据，可直接交给 glDrawElements
struct MeshData
{
    std::vector<GLfloat> positions;     // 3 floats / 顶点
    std::vector<GLfloat> normals;       // 3
    std::vector<GLfloat> uvs;           // 2
    std::vector<GLfloat> colors;        // 3，可选：程序化生成的网格用（OBJ 不会填）
    std::vector<GLuint>  indices;       // 3 / 三角形
    bool hasNormals = false;
    bool hasUVs     = false;
    bool hasColors  = false;

    ObjMaterial material;
    std::string diffuseMapPath;         // 拼好的贴图路径（可能为空）

    // 归一化用
    float bboxMin[3] = { 0, 0, 0 };
    float bboxMax[3] = { 0, 0, 0 };
    float center[3]  = { 0, 0, 0 };
    float radius     = 1.0f;            // 包围球半径（相对外接中心）

    // 统计信息，用于自检与实验报告
    int srcVertexCount   = 0;
    int srcFaceCount     = 0;
    int uniqueVertexCount = 0;
    int triangleCount    = 0;
    int quadFaceCount    = 0;
    int otherFaceCount   = 0;           // 非三角形/四边形的面

    bool valid = false;
};

// 读取 .obj（并按其 mtllib 读取 .mtl）。失败时返回 false，原因写入 errOut。
bool loadOBJ(const std::string& path, MeshData& out, std::string* errOut = nullptr);
