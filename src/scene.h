#pragma once
// 三维场景：若干个物体 + 每个物体自己的累积变换矩阵 objectXform（实验五）。
#include "mesh.h"
#include "math3d.h"
#include "types.h"

#include <memory>
#include <string>
#include <vector>

// 物体类型。Mesh 之外的几种用 GLUT 预定义图元 / 手写顶点绘制。
enum class PrimKind
{
    Mesh,       // 导入的 OBJ 模型
    Cube,       // 实验四的手写立方体（8 顶点 6 面 + 顶点色）
    Teapot,     // glutSolidTeapot
    Sphere,     // glutSolidSphere
    Ground,     // 地面平台
    Grid,       // 地面网格线
    Axes        // 坐标轴
};

struct Object
{
    std::string           name;
    PrimKind              kind        = PrimKind::Cube;
    std::shared_ptr<Mesh> mesh;                    // kind == Mesh 时有效
    float                 xform[16];               // objectXform：累积变换矩阵
    float                 size        = 1.0f;
    float                 pos[3]      = { 0, 0, 0 };
    float                 color[3]    = { 1, 1, 1 };
    bool                  visible     = true;
    bool                  selectable  = true;
    bool                  useMeshMaterial = false; // 用 .mtl 材质还是 glColor
};

class Scene
{
public:
    // 建场景。modelPath 为空时跳过模型，只留图元。
    bool build(const std::string& modelPath, UpAxis up, SceneKind kind,
               std::string* errOut = nullptr);

    void  drawAll(bool useTexture) const;
    static void drawOne(const Object& o, bool useTexture);

    void  resetTransforms();                       // 所有物体变换复位为单位阵

    // 森林场景运行期重建（分形参数调整后调用）：复用 "Forest" 物体已持有的 Mesh，
    // 只重新生成几何数据。返回是否成功。
    bool  rebuildForest();

    float radius() const { return radius_; }       // 场景半径，用于自适应取景
    const std::string& modelStatus() const { return modelStatus_; }

    // 在场景中把第 idx 个「可选中的物体」加 1，返回新的下标（供 Tab 循环切换）
    int  nextSelectable(int idx) const;
    const Object* selectedObject() const;

    std::vector<Object>                 objects;
    std::vector<std::shared_ptr<Mesh>>  meshes;
    int                                 selected = 0;

private:
    float       radius_      = 2.2f;
    std::string modelStatus_ = "(no model loaded)";
};
