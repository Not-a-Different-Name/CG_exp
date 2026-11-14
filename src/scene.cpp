#include "scene.h"
#include "anim.h"      // roamPath：种树时要避开兔子真正走的路线
#include "forest.h"

#include <cmath>
#include <cstdio>

namespace {

// 森林生成参数（初次建场景与运行期重建共用，保证两次生成完全一致）
constexpr float    kForestHalf   = 34.0f;
constexpr int      kForestTrees  = 150;
constexpr float    kForestClear  = 7.0f;
constexpr unsigned kForestSeed   = 20260924u;

// 采样兔子的漫游路线（种树避让 + 重建时保持一致）。
// 小动物的绕圈路线同样纳入避让，否则树会长在动物散步的路径上、挡住视线。
void buildForestPath(std::vector<Vec3>& path)
{
    path.clear();
    path.reserve(722 + 64);
    for (int i = 0; i <= 720; ++i)
        path.push_back(roamPath(0.02f * (float)i));       // 覆盖 0 ~ 14.4 秒
    path.push_back(v3(0.0f, 0.0f, 0.0f));                 // 林间空地（机器人+默认机位）

    // 鹿与狐狸的绕圈路线（圆心/半径与 animals.cpp 的 kDefs 一致）
    const float circles[][3] = {
        { -3.7f,  0.7f, 1.4f },     // Deer（W3：为机器人贝塞尔环让出空间）
        { -5.5f, -3.0f, 1.5f },     // Fox
    };
    for (const auto& c : circles)
        for (int i = 0; i < 32; ++i)
        {
            const float a = 2.0f * 3.14159265f * (float)i / 32.0f;
            path.push_back(v3(c[0] + c[2] * std::cos(a), 0.0f, c[1] + c[2] * std::sin(a)));
        }
}

// ---------------------------------------------------------------------------
// 实验四的立方体：8 个顶点 + 8 个顶点颜色 + 6 个面。
// 顶点色配合默认的 GL_SMOOTH 会在表面上做双线性插值，得到「五彩斑斓的立方体」。
// ---------------------------------------------------------------------------
const GLfloat kCubeVerts[8][3] = {
    { -1.0f, -1.0f, -1.0f }, {  1.0f, -1.0f, -1.0f },
    {  1.0f,  1.0f, -1.0f }, { -1.0f,  1.0f, -1.0f },
    { -1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f,  1.0f },
    {  1.0f,  1.0f,  1.0f }, { -1.0f,  1.0f,  1.0f }
};

const GLfloat kCubeColors[8][3] = {
    { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f },
    { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f }
};

const int kCubeFaces[6][4] = {
    { 1, 0, 3, 2 }, { 3, 7, 6, 2 }, { 7, 3, 0, 4 },
    { 2, 6, 5, 1 }, { 4, 5, 6, 7 }, { 5, 4, 0, 1 }
};

void polygonQuad(int a, int b, int c, int d)
{
    Vec3 pa=v3(kCubeVerts[a][0],kCubeVerts[a][1],kCubeVerts[a][2]);
    Vec3 pb=v3(kCubeVerts[b][0],kCubeVerts[b][1],kCubeVerts[b][2]);
    Vec3 pc=v3(kCubeVerts[c][0],kCubeVerts[c][1],kCubeVerts[c][2]);
    Vec3 n=v3norm(v3cross(pb-pa,pc-pa));
    glNormal3f(n.x,n.y,n.z);
    glBegin(GL_QUADS);
        glColor3fv(kCubeColors[a]); glVertex3fv(kCubeVerts[a]);
        glColor3fv(kCubeColors[b]); glVertex3fv(kCubeVerts[b]);
        glColor3fv(kCubeColors[c]); glVertex3fv(kCubeVerts[c]);
        glColor3fv(kCubeColors[d]); glVertex3fv(kCubeVerts[d]);
    glEnd();
}

void drawColorCube()
{
    for (int f = 0; f < 6; ++f)
        polygonQuad(kCubeFaces[f][0], kCubeFaces[f][1], kCubeFaces[f][2], kCubeFaces[f][3]);
}

// 地面网格：在 y = groundY 平面上画线。半透明感靠深灰颜色模拟。
void drawGrid(float y, float half, int div, float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_LINES);
    for (int i = 0; i <= div; ++i)
    {
        const float t = -half + (2.0f * half) * (float)i / (float)div;
        glVertex3f(t, y, -half);
        glVertex3f(t, y,  half);
        glVertex3f(-half, y, t);
        glVertex3f( half, y, t);
    }
    glEnd();
}

// 坐标轴：X 红、Y 绿、Z 蓝
void drawAxes(float len)
{
    glBegin(GL_LINES);
        glColor3f(1.0f, 0.25f, 0.25f); glVertex3f(0, 0, 0); glVertex3f(len, 0, 0);
        glColor3f(0.25f, 1.0f, 0.25f); glVertex3f(0, 0, 0); glVertex3f(0, len, 0);
        glColor3f(0.35f, 0.5f, 1.0f);  glVertex3f(0, 0, 0); glVertex3f(0, 0, len);
    glEnd();
}

} // namespace

void Scene::drawOne(const Object& o, bool useTexture)
{
    if (!o.visible) return;

    glPushMatrix();
    glTranslatef(o.pos[0], o.pos[1], o.pos[2]);
    // 实验五/六：把累积出来的 objectXform 乘进当前模型视图矩阵
    glMultMatrixf(o.xform);
    glScalef(o.size, o.size, o.size);

    switch (o.kind)
    {
    case PrimKind::Mesh:
        if (o.mesh)
        {
            // 关键：先关掉 GL_COLOR_MATERIAL。它会让材质跟随当前颜色，
            // 从而忽略 glMaterialfv 设置的 AMBIENT/DIFFUSE，.mtl 材质就失效了。
            glDisable(GL_COLOR_MATERIAL);
            o.mesh->draw(useTexture);
            glEnable(GL_COLOR_MATERIAL);
        }
        break;

    case PrimKind::Cube:
        glEnable(GL_COLOR_MATERIAL);
        drawColorCube();
        break;

    case PrimKind::Teapot:
        glEnable(GL_COLOR_MATERIAL);
        glColor3fv(o.color);
        glutSolidTeapot(1.0);
        break;

    case PrimKind::Sphere:
        glEnable(GL_COLOR_MATERIAL);
        glColor3fv(o.color);
        glutSolidSphere(1.0, 32, 24);
        break;

    case PrimKind::Ground:
        glEnable(GL_COLOR_MATERIAL);
        glColor3fv(o.color);
        glBegin(GL_QUADS);
            // 绕序必须让法线朝上（+Y），否则会被背面剔除掉，地面完全看不见
            glNormal3f(0, 1, 0);
            glVertex3f(-1, 0, -1); glVertex3f(-1, 0,  1);
            glVertex3f( 1, 0,  1); glVertex3f( 1, 0, -1);
        glEnd();
        break;

    case PrimKind::Grid:
        glDisable(GL_LIGHTING);
        drawGrid(0.0f, 1.0f, 24, 0.35f, 0.35f, 0.40f);
        glEnable(GL_LIGHTING);
        break;

    case PrimKind::Axes:
        glDisable(GL_LIGHTING);
        glLineWidth(2.0f);
        drawAxes(1.0f);
        glLineWidth(1.0f);
        glEnable(GL_LIGHTING);
        break;
    }

    glPopMatrix();
}

void Scene::drawAll(bool useTexture) const
{
    for (const Object& o : objects) drawOne(o, useTexture);
}

bool Scene::build(const std::string& modelPath, UpAxis up, SceneKind kind, std::string* errOut)
{
    objects.clear();
    meshes.clear();
    selected = 0;

    // ---- 导入的复杂三维模型（加分项：导入 3D 模型数据文件）----
    std::shared_ptr<Mesh> model;
    if (!modelPath.empty())
    {
        model = std::make_shared<Mesh>();
        std::string err;
        if (model->loadFromOBJ(modelPath, up, /*normalize=*/true, &err))
        {
            meshes.push_back(model);
            const MeshData& d = model->data();
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "OBJ: %d verts / %d faces -> %d unique verts, %d triangles%s",
                d.srcVertexCount, d.srcFaceCount, d.uniqueVertexCount, d.triangleCount,
                model->hasTexture() ? " [textured]" : "");
            modelStatus_ = buf;
        }
        else
        {
            model.reset();
            // HUD 用 GLUT 位图字体绘制，只能显示 ASCII，所以这里不留中文
            modelStatus_ = "model load failed: " + err;
            if (errOut) *errOut = err;
        }
    }

    // ---- 森林场景 ----
    if (kind == SceneKind::Forest)
    {
        std::vector<Vec3> path;
        buildForestPath(path);

        MeshData forestData;
        buildForest(forestData, kForestHalf, kForestTrees, path, kForestClear, kForestSeed);

        auto forestMesh = std::make_shared<Mesh>();
        forestMesh->adopt(std::move(forestData));
        meshes.push_back(forestMesh);

        std::printf("[scene] forest: %d vertices, %d triangles\n",
                    (int)(forestMesh->data().positions.size() / 3),
                    forestMesh->data().triangleCount);

        if (model)
        {
            Object o;
            o.name = "Bunny";
            o.kind = PrimKind::Mesh;
            o.mesh = model;
            o.size = 1.0f;
            // 森林地面在 y = 0，而模型原点在包围球中心，所以要垫到脚底刚好贴地。
            // 2026 实验的主角是机器人（robot.cpp，站在林间空地原点），
            // 兔子挪到空地边缘作陪衬；漫游动画播放时会沿 roamPath 走，不受此影响。
            // W3：机器人贝塞尔环半径 1.7，兔子从 (2.6,-1.6) 后移到环外，
            // 否则机器人经过右侧锚点时会从兔子身体里穿过去。
            o.pos[0] = 3.3f;
            o.pos[1] = -model->data().bboxMin[1];
            o.pos[2] = -2.6f;
            o.useMeshMaterial = true;
            mat4Identity(o.xform);
            objects.push_back(o);                             // 先加兔子：findFirstMesh 要拿到它
        }

        {
            Object o;                                         // 整片森林合成一个物体，一次画完
            o.name = "Forest";
            o.kind = PrimKind::Mesh;
            o.mesh = forestMesh;
            o.size = 1.0f;
            o.selectable = false;
            mat4Identity(o.xform);
            objects.push_back(o);
        }

        radius_  = 10.0f;      // 供自由相机取景与投影 near/far 使用
        selected = nextSelectable(-1);
        return true;
    }

    // ---- 展示台场景 ----
    // ---- 组装场景 ----
    // 地面高度对齐模型的脚底而不是写死：模型归一化后各轴范围并不相同
    // （本兔子 y ∈ [-0.79, 0.79]），写死 -1.0 会让它悬空。
    const float groundY = model ? (model->data().bboxMin[1] - 0.004f) : -1.0f;

    if (model)
    {
        Object o;
        o.name = "Rabbit (OBJ)";
        o.kind = PrimKind::Mesh;
        o.mesh = model;
        o.size = 1.0f;
        o.pos[0] = 0.0f; o.pos[1] = 0.0f; o.pos[2] = 0.0f;
        o.useMeshMaterial = true;
        mat4Identity(o.xform);
        objects.push_back(o);
    }

    {
        Object o;                                   // 实验四立方体
        o.name = "Color Cube";
        o.kind = PrimKind::Cube;
        o.size = 0.21f;
        o.pos[0] =  0.86f; o.pos[1] = groundY + 0.21f; o.pos[2] = 0.62f;
        mat4Identity(o.xform);
        objects.push_back(o);
    }
    {
        Object o;                                   // 茶壶
        o.name = "Teapot";
        o.kind = PrimKind::Teapot;
        o.size = 0.21f;
        o.pos[0] = -0.88f; o.pos[1] = groundY + 0.21f; o.pos[2] = 0.58f;
        o.color[0] = 0.85f; o.color[1] = 0.55f; o.color[2] = 0.20f;
        mat4Identity(o.xform);
        objects.push_back(o);
    }
    {
        Object o;                                   // 球
        o.name = "Sphere";
        o.kind = PrimKind::Sphere;
        o.size = 0.16f;
        o.pos[0] = -0.06f; o.pos[1] = groundY + 0.16f; o.pos[2] = 1.02f;
        o.color[0] = 0.30f; o.color[1] = 0.62f; o.color[2] = 0.85f;
        mat4Identity(o.xform);
        objects.push_back(o);
    }
    {
        Object o;                                   // 地面平台（参考实验七）
        o.name = "Ground";
        o.kind = PrimKind::Ground;
        o.size = 3.0f;
        o.pos[0] = 0.0f; o.pos[1] = groundY; o.pos[2] = 0.0f;
        o.color[0] = 0.30f; o.color[1] = 0.31f; o.color[2] = 0.34f;
        o.selectable = false;
        mat4Identity(o.xform);
        objects.push_back(o);
    }
    {
        Object o;                                   // 网格
        o.name = "Grid";
        o.kind = PrimKind::Grid;
        o.size = 2.6f;
        o.pos[0] = 0.0f; o.pos[1] = groundY + 0.002f; o.pos[2] = 0.0f;
        o.selectable = false;
        mat4Identity(o.xform);
        objects.push_back(o);
    }
    {
        Object o;                                   // 坐标轴
        o.name = "Axes";
        o.kind = PrimKind::Axes;
        o.size = 1.0f;
        o.pos[0] = 0.0f; o.pos[1] = groundY + 0.002f; o.pos[2] = 0.0f;
        o.selectable = false;
        mat4Identity(o.xform);
        objects.push_back(o);
    }

    // ---- 场景半径：用于自适应取景 ----
    // 只统计「内容」物体（模型 + 图元）。地面 / 网格 / 坐标轴是背景元素，本来就该
    // 延伸到画面之外；把它们算进来会把半径撑大（实测会从 1.5 涨到 3.7），
    // 相机被迫后退，模型在画面里缩成一小团。
    float r = 1.0f;
    for (const Object& o : objects)
    {
        if (!o.visible) continue;
        if (o.kind == PrimKind::Ground || o.kind == PrimKind::Grid || o.kind == PrimKind::Axes)
            continue;

        float objR = o.size;
        if (o.kind == PrimKind::Mesh && o.mesh) objR = o.mesh->radius() * o.size;
        else if (o.kind == PrimKind::Cube)      objR = o.size * 1.732f;   // 立方体半对角线

        const float extent = std::sqrt(o.pos[0] * o.pos[0] + o.pos[1] * o.pos[1] + o.pos[2] * o.pos[2]) + objR;
        if (extent > r) r = extent;
    }
    radius_ = r;

    selected = nextSelectable(-1);
    return true;
}

void Scene::resetTransforms()
{
    for (Object& o : objects)
        if (o.selectable) mat4Identity(o.xform);
}

bool Scene::rebuildForest()
{
    // 找到森林物体（build 时整片森林烘焙成一个 Mesh）
    std::shared_ptr<Mesh> forestMesh;
    for (Object& o : objects)
        if (o.name == "Forest" && o.mesh) { forestMesh = o.mesh; break; }
    if (!forestMesh) return false;

    std::vector<Vec3> path;
    buildForestPath(path);

    MeshData data;
    buildForest(data, kForestHalf, kForestTrees, path, kForestClear, kForestSeed);
    forestMesh->adopt(std::move(data));      // adopt 会释放旧 GL 缓冲，下帧自动重传

    std::printf("[scene] forest rebuilt: %d vertices, %d triangles (%s)\n",
                (int)(forestMesh->data().positions.size() / 3),
                forestMesh->data().triangleCount, forestParamsText());
    return true;
}

int Scene::nextSelectable(int idx) const
{
    const int n = (int)objects.size();
    if (n == 0) return -1;
    for (int k = 1; k <= n; ++k)
    {
        const int i = ((idx + k) % n + n) % n;
        if (objects[i].selectable && objects[i].visible) return i;
    }
    return -1;
}

const Object* Scene::selectedObject() const
{
    if (selected < 0 || selected >= (int)objects.size()) return nullptr;
    return &objects[selected];
}
