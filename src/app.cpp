#include "app.h"
#include "animals.h"
#include "hud.h"
#include "lighting.h"
#include "projection.h"
#include "recorder.h"
#include "renderer.h"
#include "robot.h"

#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

AppState g_app;

namespace {

// 非空时表示「下一帧画完就截图并退出」（--shot 批处理用）
std::string g_pendingShot;
int g_exitCode = 0;

// ---------------------------------------------------------------------------
// 录制状态
//
// 出帧不依赖真实时间：帧序号 n 对应动画时间 n / fps，渲染多快都不影响视频节奏。
// 所以每帧画完立刻 glutPostRedisplay 请求下一帧，尽可能快地离线渲染。
// ---------------------------------------------------------------------------
struct RecordState
{
    bool        requested = false;      // 已请求，等窗口尺寸就绪
    bool        active    = false;      // ffmpeg 已启动，正在出帧
    std::string path;
    AnimKind    anim    = AnimKind::None;
    float       seconds = 6.0f;
    int         fps     = 30;
    int         total   = 0;
    int         frame   = 0;
};
RecordState g_rec;
bool        g_reshaped = false;         // reshape 至少调用过一次，尺寸才可信

// ---------------------------------------------------------------------------
// 实时预览状态（空格键切换）
//
// 和录制刻意用不同的时间轴：录制取「帧序号 / fps」，是纯函数、可复现；
// 预览取真实时钟，卡了也不影响观感。
// ---------------------------------------------------------------------------
struct PreviewState
{
    bool     active  = false;
    AnimKind anim    = AnimKind::Turntable;
    int      startMs = 0;
    float    seconds = 6.0f;
};
PreviewState g_preview;

// 定时器链里「已有一帧在排队」的标志。输入事件也会 glutPostRedisplay，
// 如果每次 display 都无条件再挂一个定时器，按一次键就会多出一条并行的重绘链，
// 连按几次后帧率成倍上涨（每个 display 又挂一个新定时器，链数只增不减）。
bool g_tickScheduled = false;
void previewTick(int) { g_tickScheduled = false; glutPostRedisplay(); }

void finishRecording()
{
    std::string err;
    if (recStop(&err))
        std::printf("[rec] video written: %s (%d frames @ %d fps)\n",
                    g_rec.path.c_str(), g_rec.total, g_rec.fps);
    else {
        g_exitCode = 1;
        std::printf("[rec] encode FAILED: %s\n", err.c_str());
    }

    g_rec.active    = false;
    g_rec.requested = false;
}

// ---------------------------------------------------------------------------
// 渲染状态
// ---------------------------------------------------------------------------
void setupRenderState()
{
    // 线框 / 点模式下关掉光照：线稿本来就该是纯色，开了光照反而看不清结构
    const bool lit = g_app.lighting && g_app.renderMode == RenderMode::Solid;

    if (lit)
    {
        glEnable(GL_LIGHTING);
        // 图元用 glColor 驱动材质；否则 glColor3f 在光照下会被完全忽略，全变灰
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glEnable(GL_COLOR_MATERIAL);
    }
    else
    {
        glDisable(GL_LIGHTING);
        glDisable(GL_COLOR_MATERIAL);
    }

    glShadeModel(g_app.smoothShade ? GL_SMOOTH : GL_FLAT);

    if (g_app.cullFace)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }

    // 雾：固定管线的雾是逐顶点线性插值的，所以森林的地面必须细分成网格，
    // 一整块大平面上只有四个角有雾值，远处会突变成一条硬边。
    if (g_app.fog)
    {
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_EXP2);
        glFogf(GL_FOG_DENSITY, g_app.fogDensity);
        glFogfv(GL_FOG_COLOR, g_app.fogColor);
    }
    else
    {
        glDisable(GL_FOG);
    }

    switch (g_app.renderMode)
    {
    case RenderMode::Solid:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        break;
    case RenderMode::Wireframe:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(1.0f);
        break;
    case RenderMode::Points:
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glPointSize(2.5f);
        break;
    }
}

// 拾取用的包围球半径：本地网格半径 × 物体缩放 × xform 的缩放分量。
float xformScale(const float m[16])
{
    const float sx = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
    const float sy = std::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
    const float sz = std::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
    return std::max(std::max(sx, sy), std::max(sz, 1e-4f));
}

float objectPickRadius(const Object& o)
{
    float r = o.size;
    if (o.kind == PrimKind::Mesh && o.mesh)      r = o.mesh->radius() * o.size;
    else if (o.kind == PrimKind::Cube)           r = o.size * 1.732f;
    else if (o.kind == PrimKind::Teapot)         r = o.size * 1.5f;
    return r * xformScale(o.xform);
}

// 物体可见性开关（网格/坐标轴/地面各有独立开关）
bool objectVisible(const Object& o)
{
    if (!o.visible) return false;
    switch (o.kind)
    {
    case PrimKind::Grid:   return g_app.showGrid;
    case PrimKind::Axes:   return g_app.showAxes;
    case PrimKind::Ground: return g_app.showGround;
    default:               return true;
    }
}

void drawSceneFiltered(bool subjectsOnly = false)
{
    for (const Object& o : g_app.scene.objects)
        if (objectVisible(o) && !(subjectsOnly && o.name == "Forest"))
            renderer::draw(o, g_app.textureOn);
}

// 选中提示：在地面画一圈金色标记。Tab 或菜单选中的对象一眼可见，
// 不用从 HUD 文字里猜当前选的是谁。根节点 "Robot" 用可见部件的范围做标记。
void drawSelectionMarker(bool subjectsOnly)
{
    const Object* o = g_app.scene.selectedObject();
    if (!o) return;

    float cx = o->pos[0], cz = o->pos[2], r = o->size;
    if (o->name == "Robot")
    {
        float minX = 0, maxX = 0, minZ = 0, maxZ = 0;
        bool found = false;
        for (const Object& p : g_app.scene.objects)
        {
            if (!p.visible || !(p.name.rfind("Robot ", 0) == 0 || p.name == "Imported Character"))
                continue;
            float pr = p.size;
            if (p.kind == PrimKind::Mesh && p.mesh) pr = p.mesh->radius() * p.size;
            const float x0 = p.pos[0] - pr, x1 = p.pos[0] + pr;
            const float z0 = p.pos[2] - pr, z1 = p.pos[2] + pr;
            if (!found) { minX = x0; maxX = x1; minZ = z0; maxZ = z1; found = true; }
            else
            {
                minX = std::min(minX, x0); maxX = std::max(maxX, x1);
                minZ = std::min(minZ, z0); maxZ = std::max(maxZ, z1);
            }
        }
        if (!found) return;
        cx = (minX + maxX) * 0.5f;
        cz = (minZ + maxZ) * 0.5f;
        r  = 0.5f * std::max(maxX - minX, maxZ - minZ) + 0.12f;
    }
    else
    {
        if (!o->visible) return;
        if (o->kind == PrimKind::Mesh && o->mesh) r = o->mesh->radius() * o->size;
        else if (o->kind == PrimKind::Cube)       r = o->size * 1.732f;
    }

    float groundY = 0.02f;
    for (const Object& g : g_app.scene.objects)
        if (g.name == "Ground") { groundY = g.pos[1] + 0.01f; break; }

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);
    glDepthFunc(GL_LEQUAL);
    glColor3f(1.0f, 0.78f, 0.22f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 48; ++i)
    {
        const float a = 2.0f * 3.14159265f * (float)i / 48.0f;
        glVertex3f(cx + r * std::cos(a), groundY, cz + r * std::sin(a));
    }
    glEnd();
    glLineWidth(1.0f);
    if (g_app.fog && !subjectsOnly) glEnable(GL_FOG);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
}

// ---------------------------------------------------------------------------
// 画一个视口。label 非空时在左上角画标题。
// ---------------------------------------------------------------------------
void drawViewport(int x, int y, int w, int h, ProjMode pm, ViewPreset vp, const char* label)
{
    if (w <= 0 || h <= 0) return;

    glViewport(x, y, w, h);

    // 1. 投影矩阵（内部按 aspect 修正，保证不变形）
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    Camera cam = g_app.camera;                 // 副本：视图预设不能污染真实相机
    // 外围树木与地面合在同一网格，正交相机从林外看会完全被树冠遮挡。
    // 平行投影专注空地主体，透视视口仍显示完整环境。
    const bool subjectsOnly = g_app.sceneKind == SceneKind::Forest && isParallelProjection(pm);
    const float fitRadius = subjectsOnly ? 4.0f : g_app.scene.radius();
    if (vp != ViewPreset::Free)
    {
        cam.setPreset((int)vp, fitRadius);
        if (subjectsOnly)
        {
            const Vec3 center = v3(-1.0f, 1.0f, -1.0f);
            cam.eye = cam.eye + center;
            cam.target = center;
        }
    }
    applyProjection(pm, w, h, cam, fitRadius, g_app.obliqueAngleDeg);

    // 2. 观察变换
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    cam.applyLookAt();

    // 3. 光源位置：必须在 lookAt 之后、物体变换之前
    if (g_app.lighting) applyLightPositions();

    // 4. 其余渲染状态 + 场景
    setupRenderState();
    if (subjectsOnly) glDisable(GL_FOG);
    renderer::beginView(cam, g_app.sceneKind, g_app.fog && !subjectsOnly, g_app.fogDensity, g_app.fogColor,
                        g_app.enhanced && g_app.lighting && g_app.renderMode == RenderMode::Solid, g_app.smoothShade);
    drawSceneFiltered(subjectsOnly);
    renderer::endView();
    drawSelectionMarker(subjectsOnly);

    // 5. 视口标题
    if (label && g_app.showHUD)
    {
        hudBegin(w, h);
        hudColor(1.0f, 0.85f, 0.35f);
        hudText(8.0f, (float)h - (y > 0 ? 64.0f : 20.0f), label);
        hudEnd();
    }
}

// ---------------------------------------------------------------------------
// 帮助界面（整屏模态）
// ---------------------------------------------------------------------------
struct HelpPanel
{
    float width;
    float height;
    float left;
    float bottom;
    float top;
    static constexpr float pad = 16.0f;

    float tabWidth() const { return (width - 2.0f * pad) / 3.0f; }
};

HelpPanel helpPanel()
{
    const float width = std::min(690.0f, g_app.winW - 16.0f);
    const float height = std::min(354.0f, g_app.winH - 16.0f);
    const float left = (g_app.winW - width) * 0.5f;
    const float bottom = (g_app.winH - height) * 0.5f;
    return {width, height, left, bottom, bottom + height};
}

void drawHelpScreen()
{
    struct Shortcut { const char* keys; const char* action; };
    static constexpr Shortcut pages[][6] = {
        {
            {"Right drag / wheel", "Look around / zoom"},
            {"WASD / Q E", "Move / descend or climb"},
            {"Tab / click", "Select the next object"},
            {"t / r / y", "Move / rotate / scale with left drag"},
            {"I / K / M", "Import / cycle / swap model"},
            {"Middle click", "Open the full action menu"},
        },
        {
            {"Shift+P", "Pause or resume the patrol"},
            {"Shift+B", "Show or hide the curve and control points"},
            {"Shift+G", "Start or stop the deer chase"},
            {"n / b", "Change pose / step in place"},
            {"f / u / j", "Cycle tree depth / change branch angle"},
            {"Space", "Toggle the rabbit or studio preview"},
        },
        {
            {"v / 2 4 5 6 8", "Four views / switch view"},
            {"3 / o / 7 / 9", "Perspective / ortho / cavalier / cabinet"},
            {"[ ] / - =", "Adjust FOV / oblique angle"},
            {"1 / F2 / F3", "Draw mode / renderer / shadows"},
            {"F12", "Save a screenshot"},
            {"Esc (in scene)", "Exit the viewer"},
        }
    };
    static constexpr const char* tabs[] = {"1  Explore", "2  Patrol", "3  Views"};
    const float width = (float)g_app.winW;
    const float height = (float)g_app.winH;
    const int page = g_app.helpPage;
    const auto [panelW, panelH, left, bottom, top] = helpPanel();
    if (panelW < 90.0f || panelH < 70.0f) return;
    const float pad = HelpPanel::pad;
    const bool splitColumns = panelW >= 630.0f;
    const float rowHeight = splitColumns ? 29.0f : panelH < 310.0f ? 28.0f : 35.0f;

    hudBegin(g_app.winW, g_app.winH);
    hudPanel(0, 0, width, height, 0.63f);
    hudPanel(left, bottom, panelW, panelH, 0.96f);
    hudColor(0.95f, 0.97f, 0.96f);
    hudTitle(left + pad, top - 28.0f, "Controls");
    if (panelW >= 375.0f)
        hudText(left + panelW - pad - hudTextWidth("H / Esc  Back"), top - 27.0f, "H / Esc  Back");

    const float tabW = (panelW - pad * 2.0f) / 3.0f;
    for (int tab = 0; tab < 3; ++tab)
    {
        const float x = left + pad + tab * tabW;
        if (tab == page) hudPanel(x, top - 66.0f, tabW - 4.0f, 26.0f, 1.0f);
        hudColor(tab == page ? 1.0f : 0.67f, tab == page ? 0.80f : 0.77f, 0.66f);
        const std::string label = tabW >= 104.0f ? tabs[tab] : std::to_string(tab + 1);
        hudText(x + 6.0f, top - 57.0f, label);
    }

    float y = top - 94.0f;
    for (const auto& item : pages[page])
    {
        if (y - (splitColumns ? 0.0f : 15.0f) < bottom + 12.0f) break;
        const float actionX = left + (splitColumns ? 207.0f : pad);
        std::string action = item.action;
        const float available = left + panelW - pad - actionX;
        while (!action.empty() && hudTextWidth(action) > available) action.pop_back();

        hudColor(0.58f, 0.84f, 0.75f);
        hudText(left + pad, y, item.keys);
        hudColor(0.93f, 0.96f, 0.94f);
        hudText(actionX, y - (splitColumns ? 0.0f : 15.0f), action);
        y -= rowHeight;
    }
    hudEnd();
}

// ---------------------------------------------------------------------------
// In-scene controls and contextual selection
// ---------------------------------------------------------------------------
void drawOverlay()
{
    // 操作提示到点自动消失（状态条随后恢复显示当前角色来源）。
    if (!g_app.statusMsg.empty() && g_app.statusUntilMs > 0 &&
        glutGet(GLUT_ELAPSED_TIME) >= g_app.statusUntilMs)
    {
        g_app.statusMsg.clear();
        g_app.statusUntilMs = 0;
    }
    if (g_app.showHelp) { drawHelpScreen(); return; }
    if (!g_app.showHUD) return;
    const float width = (float)g_app.winW;
    const float height = (float)g_app.winH;
    const bool forest = g_app.sceneKind == SceneKind::Forest;
    const Object* selection = g_app.scene.selectedObject();
    const bool selectedRobot = selection && selection->name == "Robot";
    const char* mode = g_app.transMode == TransMode::Translate ? "Move" :
                       g_app.transMode == TransMode::Rotate ? "Rotate" : "Scale";
    const char* activity = robotChasing() ? "Tracking deer" :
                           robotOnPath() ? "On patrol" :
                           robotWalking() ? "Walking in place" : "Patrol paused";
    const char* title = forest ? "Forest Patrol" : "Model Studio";
    const std::string help = "H  Controls";
    std::string actions = "Tab  Select   Right drag  Look around";
    if (selectedRobot)
        actions = "Left drag  " + std::string(mode) + "   t Move   r Rotate   y Scale";
    else if (forest)
        actions = std::string("Shift+P  ") + (robotOnPath() ? "Pause" : "Patrol") +
                  (robotPathGuide() ? "   Shift+B  Hide route" : "   Shift+B  Show route") +
                  "   Tab Select   I Import   K/M Switch";

    hudBegin(g_app.winW, g_app.winH);
    if (height >= 105.0f)
    {
        hudPanel(0, height - 46.0f, width, 46.0f, 0.86f);
        hudColor(0.96f, 0.98f, 0.94f);
        hudTitle(16.0f, height - 29.0f, title);
        const std::string status = forest ? activity : projModeName(g_app.projMode);
        if (width >= 185.0f + hudTextWidth(status))
        {
            hudColor(0.60f, 0.84f, 0.75f);
            hudText(164.0f, height - 28.0f, status);
        }
        const std::string source = forest ? robotCharacterSource() : (selection ? selection->name : "");
        std::string label = g_app.statusMsg.empty() ? source : g_app.statusMsg;
        for (char& c : label) if ((unsigned char)c >= 128) c = '?';
        const int labelLimit = std::max(0, (g_app.winW - 340) / 9);
        if (labelLimit >= 8 && (int)label.size() > labelLimit)
            label = label.substr(0, labelLimit - 3) + "...";
        if (labelLimit >= 8 && !label.empty())
        {
            hudColor(g_app.statusMsg.empty() ? 0.77f : 1.0f, 0.83f, 0.72f);
            hudText(width - 16.0f - hudTextWidth(label), height - 28.0f, label);
        }

        if (width >= 130.0f)
        {
            hudPanel(0, 0, width, 31.0f, 0.86f);
            hudColor(0.98f, 0.80f, 0.53f);
            const float helpX = width - 16.0f - hudTextWidth(help);
            hudText(helpX, 11.0f, help);
            if (hudTextWidth(actions) <= helpX - 32.0f)
            {
                hudColor(0.89f, 0.94f, 0.92f);
                hudText(16.0f, 11.0f, actions);
            }
            else if (helpX >= 130.0f)
            {
                hudText(16.0f, 11.0f, "Tab  Select");
            }
        }
    }
    hudEnd();
}

// ---------------------------------------------------------------------------
// 截图：写成 24 位 BMP。OpenGL 的行序（下到上）正好与 BMP 一致，不用翻转。
// ---------------------------------------------------------------------------
bool writeBMP(const char* path, int w, int h, const unsigned char* rgb)
{
    if (w <= 0 || h <= 0) return false;

    const int rowRaw    = w * 3;
    const int rowPadded = (rowRaw + 3) & ~3;          // BMP 每行按 4 字节对齐
    const int dataSize  = rowPadded * h;
    const int fileSize  = 54 + dataSize;

    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;

    unsigned char hdr[54] = { 0 };
    hdr[0] = 'B'; hdr[1] = 'M';
    auto put32 = [](unsigned char* p, unsigned v) {
        p[0] = (unsigned char)(v & 0xFF); p[1] = (unsigned char)((v >> 8) & 0xFF);
        p[2] = (unsigned char)((v >> 16) & 0xFF); p[3] = (unsigned char)((v >> 24) & 0xFF);
    };
    put32(hdr + 2,  (unsigned)fileSize);
    put32(hdr + 10, 54);                               // 像素数据偏移
    put32(hdr + 14, 40);                               // DIB 头大小
    put32(hdr + 18, (unsigned)w);
    put32(hdr + 22, (unsigned)h);
    hdr[26] = 1;                                       // planes
    hdr[28] = 24;                                      // 24 位
    put32(hdr + 34, (unsigned)dataSize);

    std::fwrite(hdr, 1, 54, f);

    std::vector<unsigned char> row((size_t)rowPadded, 0);
    for (int y = 0; y < h; ++y)
    {
        const unsigned char* src = rgb + (size_t)y * rowRaw;
        for (int x = 0; x < w; ++x)
        {
            row[x * 3 + 0] = src[x * 3 + 2];           // BMP 是 BGR
            row[x * 3 + 1] = src[x * 3 + 1];
            row[x * 3 + 2] = src[x * 3 + 0];
        }
        std::fwrite(row.data(), 1, (size_t)rowPadded, f);
    }
    std::fclose(f);
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
void appSetup(const std::string& modelPath)
{
    g_app.modelPath = modelPath;

    std::string err;
    g_app.scene.build(modelPath, g_app.modelUp, g_app.sceneKind, &err);
    g_app.modelStatus = g_app.scene.modelStatus();

    // 2026 实验主角：机器人在森林场景登场（Studio 展示台保持原样）
    if (g_app.sceneKind == SceneKind::Forest)
    {
        robotCreate(g_app.scene);
        animalsCreate(g_app.scene);      // 小动物：鹿 + 狐狸（兔子归 roam 动画管）

        // 主角默认选中：启动后 t/r/y 与左键拖动直接作用在机器人上，
        // 底部提示和地面选中圈也立刻指向它（Tab 仍可切到兔子等物体）。
        for (int i = 0; i < (int)g_app.scene.objects.size(); ++i)
            if (g_app.scene.objects[i].name == "Robot")
            {
                g_app.scene.selected = i;
                break;
            }
    }

    appResetCamera();

    // 清屏色跟进雾色：否则地平线处会看到背景色和雾色对不上的硬边
    if (g_app.fog) glClearColor(g_app.fogColor[0], g_app.fogColor[1], g_app.fogColor[2], 1.0f);
    else           glClearColor(0.11f, 0.12f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // GL_COLOR_MATERIAL 的默认追踪方式是 AMBIENT_AND_DIFFUSE，
    // 显式设置一次，避免依赖默认值
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // 预览的默认动画：森林里当然是看兔子漫步，展示台就看模型自转
    g_preview.anim    = (g_app.sceneKind == SceneKind::Forest) ? AnimKind::Roam
                                                              : AnimKind::Turntable;
    g_preview.seconds = 6.0f;

    initLighting();
    renderer::initialize();

    // 打印显卡 / 驱动信息。性能不对劲时第一眼就该看这里 ——
    // 如果 renderer 是 "GDI Generic" 之类，说明在用软件光栅化，
    // 慢是环境问题，跟程序无关。
    if (const GLubyte* rend = glGetString(GL_RENDERER))
        std::printf("[gl] renderer : %s\n", (const char*)rend);
    if (const GLubyte* ver = glGetString(GL_VERSION))
        std::printf("[gl] version  : %s\n", (const char*)ver);

    std::printf("[app] scene ready, radius %.3f\n", g_app.scene.radius());
    std::printf("[app] %s\n", g_app.modelStatus.c_str());
}

void appReshape(int w, int h)
{
    // 高度为 0 的窗口会让 aspect 变成 inf，必须兜底
    if (h <= 0) h = 1;
    if (w <= 0) w = 1;

    g_app.winW = w;
    g_app.winH = h;
    g_reshaped = true;

    glViewport(0, 0, w, h);

    // 投影矩阵每帧在 appDisplay 里按当前窗口尺寸重算，这里只保证 reshape 之后
    // 立刻重绘时状态也是对的（例如窗口最小化再恢复）。
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    applyProjection(g_app.projMode, w, h, g_app.camera, g_app.scene.radius(),
                    g_app.obliqueAngleDeg);
    glMatrixMode(GL_MODELVIEW);
}

void appDisplay()
{
    // ---- 录制：启动子进程 + 按帧序号推进动画 ----
    if (g_rec.requested && !g_rec.active)
    {
        // 尺寸要等 reshape 过来才可信，否则会按错误的宽高读帧缓冲
        if (!g_reshaped) { glutPostRedisplay(); return; }

        g_rec.total = (int)(g_rec.seconds * (float)g_rec.fps + 0.5f);
        if (g_rec.total < 1) g_rec.total = 1;

        std::string err;
        if (recStart(g_rec.path, g_app.winW, g_app.winH, g_rec.fps, &err))
        {
            g_rec.active = true;
            g_rec.frame  = 0;
            std::printf("[rec] recording %s | %dx%d @ %d fps | %d frames (%.1fs) | anim: %s\n",
                        g_rec.path.c_str(), g_app.winW, g_app.winH, g_rec.fps,
                        g_rec.total, g_rec.seconds, animKindName(g_rec.anim));
        }
        else
        {
            g_exitCode = 1;
            std::printf("[rec] cannot start: %s\n", err.c_str());
            g_rec.requested = false;
            glutLeaveMainLoop();
            return;
        }
    }

    // 动画时间是「帧序号 / fps」，与真实耗时无关 —— 这台机器快慢都一样出片
    if (g_rec.active)
    {
        applyAnimation(g_rec.anim, (float)g_rec.frame / (float)g_rec.fps, g_rec.seconds);
    }
    else if (g_preview.active)
    {
        float t = (float)(glutGet(GLUT_ELAPSED_TIME) - g_preview.startMs) / 1000.0f;

        // 漫游是条不闭合的漫游曲线，让它一直走下去；
        // 其余几种都是「在 seconds 秒内走完一遍」的闭环，取模循环播放。
        if (g_preview.anim != AnimKind::Roam && g_preview.seconds > 0.0f)
            t = std::fmod(t, g_preview.seconds);

        applyAnimation(g_preview.anim, t, g_preview.seconds);
    }

    // 机器人关节动画与小动物行为：与录制/预览共用同一时间轴
    //（录制=帧号/fps，其余=真实时钟），且在阴影 pass 之前更新。
    if (robotAlive())
    {
        float rt = g_rec.active ? (float)g_rec.frame / (float)g_rec.fps
                                : (float)glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

        // --time 锁定的是「整个场景」的时间轴（机器人 + 动物），否则无头出图时
        // 动物仍按真实时钟走，同一组 --time 会得到不同的动物位置。
        if (robotTimeOverride() >= 0.0f) rt = robotTimeOverride();

        // 先更新动物，再让机器人取「鹿身后」的追赶位姿，最后统一摆机器人层级。
        // （动物只能用上一帧的机器人位置做「警觉」判定，33ms 的延迟看不出来。）
        animalsUpdate(g_app.scene, rt);
        if (animalsChase())
        {
            float cp[3], yaw = 0.0f;
            if (animalsChaseTarget(g_app.scene, cp, &yaw))
                robotSetChasePose(cp[0], cp[2], yaw);
        }
        robotUpdate(g_app.scene, rt);
    }

    renderer::shadowPass(g_app.scene, g_app.sceneKind, g_app.showGround,
                         g_app.enhanced && g_app.shadows && g_app.lighting && g_app.renderMode == RenderMode::Solid);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (g_app.quadView)
    {
        // 四视口：左上前视图 / 右上顶视图 / 左下左视图 / 右下透视
        // 每个视口各自按自己的尺寸算 aspect，所以都不会变形
        const int hw = g_app.winW / 2;
        const int hh = g_app.winH / 2;
        const int rw = g_app.winW - hw;
        const int rh = g_app.winH - hh;

        drawViewport(0,  hh, hw, rh, ProjMode::Ortho, ViewPreset::Front, "Front view");
        drawViewport(hw, hh, rw, rh, ProjMode::Ortho, ViewPreset::Top,   "Top view");
        drawViewport(0,  0,  hw, hh, ProjMode::Ortho, ViewPreset::Left,  "Left view");
        // 右下角固定用透视 + 自由相机，和三视图形成对照
        drawViewport(hw, 0,  rw, hh, ProjMode::Perspective,
                     ViewPreset::Free, "Perspective view");
    }
    else
    {
        drawViewport(0, 0, g_app.winW, g_app.winH,
                     g_app.projMode, g_app.viewPreset, nullptr);
    }

    glViewport(0, 0, g_app.winW, g_app.winH);
    drawOverlay();

    // 一次性截图：在交换缓冲之前从后缓冲读，所以窗口隐藏也能正常截。
    if (!g_pendingShot.empty())
    {
        std::vector<unsigned char> buf((size_t)g_app.winW * g_app.winH * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, g_app.winW, g_app.winH, GL_RGB, GL_UNSIGNED_BYTE, buf.data());
        if (writeBMP(g_pendingShot.c_str(), g_app.winW, g_app.winH, buf.data()))
            std::printf("[app] screenshot saved: %s (%dx%d)\n",
                        g_pendingShot.c_str(), g_app.winW, g_app.winH);
        else
        {
            g_exitCode = 1;
            std::printf("[app] screenshot failed: %s\n", g_pendingShot.c_str());
        }
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
            std::fprintf(stderr, "[gl] error: 0x%x\n", error);
            g_exitCode = 1;
        }
        g_pendingShot.clear();

        glutSwapBuffers();
        glutLeaveMainLoop();
        return;
    }

    // 录制：同样在交换缓冲之前从后缓冲读，避免抓到未完成的帧
    bool recDone = false;
    if (g_rec.active)
    {
        std::vector<unsigned char> buf((size_t)g_app.winW * g_app.winH * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, g_app.winW, g_app.winH, GL_RGB, GL_UNSIGNED_BYTE, buf.data());
        if (!recWriteFrame(buf.data())) {
            g_exitCode = 1;
            finishRecording();
            glutLeaveMainLoop();
            return;
        }

        ++g_rec.frame;
        recDone = (g_rec.frame >= g_rec.total);
    }

    glutSwapBuffers();

    if (recDone)
    {
        finishRecording();
        glutLeaveMainLoop();
        return;
    }

    // 帧率统计
    ++g_app.frameCount;
    const int now = glutGet(GLUT_ELAPSED_TIME);
    if (g_app.lastFpsMs == 0) g_app.lastFpsMs = now;
    if (now - g_app.lastFpsMs >= 500)
    {
        g_app.fps = g_app.frameCount * 1000.0f / (float)(now - g_app.lastFpsMs);
        g_app.frameCount = 0;
        g_app.lastFpsMs  = now;
    }

    // 录制中：立刻请求下一帧，把所有帧尽快渲染完（离线渲染，不等真实时间）
    if (g_rec.active)
    {
        glutPostRedisplay();
    }
    else if (g_preview.active)
    {
        // 预览走 60fps 定时器。不能像录制那样无条件 postRedisplay ——
        // 那会变成忙等，把 CPU 吃满。
        if (!g_tickScheduled) { g_tickScheduled = true; glutTimerFunc(16, previewTick, 0); }
    }
    else if (g_app.sceneKind == SceneKind::Forest && (robotAlive() || animalsAlive()))
    {
        // 森林里有常驻动画源（机器人姿势渐变/行走、动物漫步吃草），
        // GLUT 默认只在输入事件后重绘 —— 不持续请求帧的话，按 n/b 只能看到
        // 渐变的第一格（看起来像"没反应"）。30Hz 足够顺滑，也不吃满 CPU。
        if (!g_tickScheduled) { g_tickScheduled = true; glutTimerFunc(33, previewTick, 0); }
    }
}

void appPostRedisplay()
{
    glutPostRedisplay();
}

void appCleanup()
{
    static bool cleaned = false;
    if (cleaned) return;
    cleaned = true;
    if (g_rec.active) finishRecording();
    renderer::shutdown();
    std::printf("[app] cleanup: releasing textures and meshes...\n");
    for (auto& m : g_app.scene.meshes)
        if (m) m->release();
    g_app.scene.meshes.clear();
    g_app.scene.objects.clear();
    std::printf("[app] cleanup: done\n");
}

// ---------------------------------------------------------------------------
void appSetProjection(ProjMode m)
{
    g_app.projMode = m;
    // 透视是唯一带视点位置概念的投影，切回透视时把相机恢复到默认站位
    appUpdateTitle();
}

void appSetViewPreset(ViewPreset v)
{
    g_app.viewPreset = v;
    if (v != ViewPreset::Free)
        g_app.camera.setPreset((int)v, g_app.scene.radius());
    appUpdateTitle();
}

void appResetCamera()
{
    g_app.viewPreset = ViewPreset::Free;
    g_app.camera.reset(g_app.scene.radius());
    g_app.camera.fovY = 45.0f;
    if (g_app.sceneKind == SceneKind::Forest) {
        g_app.camera.eye = v3(4.1f,3.35f,6.4f);
        g_app.camera.target = v3(0.10f,0.96f,0.20f);
        g_app.camera.up = v3(0,1,0);
    }
    appUpdateTitle();
}

void appResetObjects()
{
    g_app.scene.resetTransforms();
}

void appAccumulateObjectTransform(const float delta[16])
{
    if (g_app.scene.selected < 0 ||
        g_app.scene.selected >= (int)g_app.scene.objects.size())
        return;

    Object& o = g_app.scene.objects[g_app.scene.selected];
    if (!o.selectable) return;

    // CPU-side transforms do not depend on whichever GL pass ran last.
    mat4Multiply(o.xform, delta, o.xform);
}

void appUpdateTitle()
{
    // 标题保持 ASCII：freeglut 在部分版本里用 ANSI 代码页转换标题，
    // 在中文系统上放中文可能变成乱码。
    char buf[256];
    const Object* sel = g_app.scene.selectedObject();
    const char* title = g_app.sceneKind == SceneKind::Forest ? "Forest Patrol" : "Model Studio";
    std::snprintf(buf, sizeof(buf), "%s  -  %s  -  %s",
                  title, projModeName(g_app.projMode), sel ? sel->name.c_str() : "(none)");
    glutSetWindowTitle(buf);
}

void appClickHelp(int x, int y)
{
    const HelpPanel panel = helpPanel();
    if (panel.width < 90.0f || panel.height < 70.0f) return;
    const float screenY = (float)(g_app.winH - 1 - y);
    const float relativeX = x - panel.left - HelpPanel::pad;
    const float tabW = panel.tabWidth();
    if (screenY < panel.top - 66.0f || screenY > panel.top - 40.0f ||
        relativeX < 0.0f || relativeX >= 3.0f * tabW)
        return;
    g_app.helpPage = (int)(relativeX / tabW);
    glutPostRedisplay();
}

void appSetStatus(const std::string& msg, int ms)
{
    g_app.statusMsg = msg;
    g_app.statusUntilMs = (msg.empty() || ms <= 0)
                              ? 0
                              : glutGet(GLUT_ELAPSED_TIME) + ms;
}

// 射线-包围球拾取。根节点 "Robot" 不绘制，用可见部件的范围近似。
int appPickObjectIndex(const Scene& scene, const Camera& camera, float aspect,
                       float ndcX, float ndcY)
{
    const Vec3 fwd   = camera.forward();
    const Vec3 right = v3norm(v3cross(fwd, camera.up));
    const Vec3 up    = v3cross(right, fwd);
    const float tanHalf = std::tan(camera.fovY * 0.5f * 3.14159265f / 180.0f);
    const Vec3 dir = v3norm(fwd + right * (ndcX * aspect * tanHalf) + up * (ndcY * tanHalf));

    int   best  = -1;
    float bestT = 1e30f;
    for (int i = 0; i < (int)scene.objects.size(); ++i)
    {
        const Object& o = scene.objects[i];
        if (!o.selectable || !o.visible) continue;
        if (o.kind == PrimKind::Grid || o.kind == PrimKind::Axes ||
            o.kind == PrimKind::Ground) continue;

        float cx = o.pos[0], cy = o.pos[1], cz = o.pos[2];
        float r  = objectPickRadius(o);
        if (o.name == "Robot")
        {
            float minX = 0, maxX = 0, minY = 0, maxY = 0, minZ = 0, maxZ = 0;
            bool found = false;
            for (const Object& p : scene.objects)
            {
                if (!p.visible || !(p.name.rfind("Robot ", 0) == 0 || p.name == "Imported Character"))
                    continue;
                const float pr = objectPickRadius(p);
                const float x0 = p.pos[0] - pr, x1 = p.pos[0] + pr;
                const float y0 = p.pos[1] - pr, y1 = p.pos[1] + pr;
                const float z0 = p.pos[2] - pr, z1 = p.pos[2] + pr;
                if (!found) { minX = x0; maxX = x1; minY = y0; maxY = y1; minZ = z0; maxZ = z1; found = true; }
                else
                {
                    minX = std::min(minX, x0); maxX = std::max(maxX, x1);
                    minY = std::min(minY, y0); maxY = std::max(maxY, y1);
                    minZ = std::min(minZ, z0); maxZ = std::max(maxZ, z1);
                }
            }
            if (!found) continue;
            cx = (minX + maxX) * 0.5f;
            cy = (minY + maxY) * 0.5f;
            cz = (minZ + maxZ) * 0.5f;
            const float dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
            r = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        const Vec3 oc = v3(cx - camera.eye.x, cy - camera.eye.y, cz - camera.eye.z);
        const float tca = v3dot(oc, dir);
        if (tca < 0.0f) continue;
        const float d2 = v3dot(oc, oc) - tca * tca;
        if (d2 > r * r) continue;
        const float t = tca - std::sqrt(std::max(0.0f, r * r - d2));
        if (t >= 0.0f && t < bestT) { bestT = t; best = i; }
    }
    return best;
}

bool appPickAt(int mouseX, int mouseY)
{
    if (g_app.projMode != ProjMode::Perspective || g_app.quadView) return false;
    const float w = (float)std::max(1, g_app.winW);
    const float h = (float)std::max(1, g_app.winH);
    const float ndcX = 2.0f * (float)mouseX / w - 1.0f;
    const float ndcY = 1.0f - 2.0f * (float)mouseY / h;
    const int idx = appPickObjectIndex(g_app.scene, g_app.camera, w / h, ndcX, ndcY);
    if (idx >= 0 && idx != g_app.scene.selected)
    {
        g_app.scene.selected = idx;
        appSetStatus("Selected: " + g_app.scene.objects[idx].name);
    }
    return idx >= 0;
}

bool appImportCharacter(const std::string& path, bool slim)
{
    std::string error;
    const bool imported = robotImportCharacter(g_app.scene, path, slim, error);
    if (imported)
        appSetStatus("Character: " + robotCharacterSource(), 4000);
    else
    {
        appSetStatus(error, 6000);
        std::fprintf(stderr, "[character] %s: %s\n", path.c_str(), error.c_str());
    }
    return imported;
}

void appRequestShot(const char* path)
{
    g_pendingShot = path ? path : "";
    glutPostRedisplay();
}

void appStartPreview(AnimKind kind, float loopSeconds)
{
    if (kind == AnimKind::None) return;
    g_preview.anim    = kind;
    g_preview.seconds = (loopSeconds > 0.05f && loopSeconds <= 600.0f) ? loopSeconds : 6.0f;
    g_preview.startMs = glutGet(GLUT_ELAPSED_TIME);
    g_preview.active  = true;
    std::printf("[preview] playing: %s  (Space to stop, ESC to quit)\n", animKindName(kind));
}

void appStopPreview()
{
    if (!g_preview.active) return;
    g_preview.active = false;
    std::printf("[preview] stopped\n");
}

void appTogglePreview()
{
    if (g_preview.active) appStopPreview();
    else                  appStartPreview(g_preview.anim, g_preview.seconds);
    glutPostRedisplay();
}

bool     appPreviewActive() { return g_preview.active; }
AnimKind appPreviewKind()   { return g_preview.anim; }

void appStartRecording(const RecordOptions& opt)
{
    g_rec.path      = opt.path;
    g_rec.anim      = opt.anim;
    g_rec.fps       = (opt.fps > 0 && opt.fps <= 240) ? opt.fps : 30;
    g_rec.seconds   = (opt.seconds > 0.05f && opt.seconds <= 600.0f) ? opt.seconds : 6.0f;
    g_rec.frame     = 0;
    g_rec.active    = false;
    g_rec.requested = true;      // 真正启动推迟到尺寸确定之后
    glutPostRedisplay();
}

void appSaveScreenshot(const char* path)
{
    const int w = g_app.winW, h = g_app.winH;
    if (w <= 0 || h <= 0) return;

    std::vector<unsigned char> buf((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, buf.data());

    if (writeBMP(path, w, h, buf.data()))
        std::printf("[app] screenshot saved: %s (%dx%d)\n", path, w, h);
    else
        std::printf("[app] screenshot failed: %s\n", path);
}

int appExitCode() { return g_exitCode; }

bool appRecordingActive() { return g_rec.requested || g_rec.active; }
