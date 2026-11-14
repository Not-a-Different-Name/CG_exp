#include "app.h"
#include "hud.h"
#include "lighting.h"
#include "projection.h"
#include "recorder.h"
#include "renderer.h"
#include "robot.h"

#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>
#include <cstring>
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

void previewTick(int) { glutPostRedisplay(); }

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

void drawSceneFiltered()
{
    for (const Object& o : g_app.scene.objects)
        if (objectVisible(o))
            renderer::draw(o, g_app.textureOn);
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
    if (vp != ViewPreset::Free)
        cam.setPreset((int)vp, g_app.scene.radius());
    applyProjection(pm, w, h, cam, g_app.scene.radius(), g_app.obliqueAngleDeg);

    // 2. 观察变换
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    cam.applyLookAt();

    // 3. 光源位置：必须在 lookAt 之后、物体变换之前
    if (g_app.lighting) applyLightPositions();

    // 4. 其余渲染状态 + 场景
    setupRenderState();
    renderer::beginView(cam, g_app.sceneKind, g_app.fog, g_app.fogDensity, g_app.fogColor,
                        g_app.enhanced && g_app.lighting && g_app.renderMode == RenderMode::Solid, g_app.smoothShade);
    drawSceneFiltered();
    renderer::endView();

    // 5. 视口标题
    if (label && g_app.showHUD)
    {
        hudBegin(w, h);
        hudColor(1.0f, 0.85f, 0.35f);
        hudText(8.0f, (float)h - (y > 0 ? 74.0f : 20.0f), label);
        hudEnd();
    }
}

// ---------------------------------------------------------------------------
// 帮助界面（整屏模态）
// ---------------------------------------------------------------------------
void drawHelpScreen()
{
        static const char* kHelp[] = {
            "Mouse",
            "  Left drag ......... transform selected object (per Transform mode)",
            "  Right drag ........ orbit / rotate camera",
            "  Shift + Left drag . pan camera",
            "  Middle click ...... popup menu",
            "  Wheel ............. dolly (zoom)",
            "",
            "Geometry transform (Lab 5/6)",
            "  t / r / y ......... Translate / Rotate / Scale mode",
            "  0 ................. reset selected object transform",
            "",
            "Object",
            "  Tab ............... select next object",
            "",
            "Robot (2026 project)",
            "  n ................. next pose: stand / wave / T-pose / walk-freeze",
            "  b ................. walk animation on / off (legs & arms joints)",
            "  select 'Robot' + t/r/y ... translate / rotate / scale whole robot",
            "",
            "Projection (Lab 7 + oblique bonus)",
            "  3 ................. perspective        o ... orthographic",
            "  7 / 9 ............. cavalier / cabinet oblique parallel",
            "  - / = ............. oblique angle down / up",
            "  [ / ] ............. decrease / increase FOV",
            "",
            "Views - orthographic three views (Lab 7)",
            "  2 / 8 ............. front / back",
            "  4 / 6 ............. left / right",
            "  5 ................. top",
            "  v ................. 4-viewport layout (3 views + perspective)",
            "",
            "Camera roam (Lab 7 bonus)",
            "  W A S D ........... forward / left / back / right",
            "  Q / E ............. down / up",
            "  Shift+0 ........... reset camera",
            "",
            "Animation preview",
            "  Space ............. play / pause (roam = bunny in the forest)",
            "                      use --anim to pick: turntable/orbit/",
            "                      projections/transforms/roam",
            "",
            "Render options",
            "  1 ................. solid / wireframe / points",
            "  c ................. back-face culling",
            "  m ................. smooth / flat shading",
            "  l ................. lighting",
            "  T ................. texture",
            "  F2 / F3 ........... GLSL / shadows toggle",
            "  g / x / p ......... grid / axes / ground",
            "  F12 ............... save screenshot",
            "  ESC ............... quit",
            nullptr
        };

    int lines = 0;
    for (; kHelp[lines]; ++lines) {}

    const float W = (float)g_app.winW;
    const float H = (float)g_app.winH;

    hudBegin(g_app.winW, g_app.winH);

    // 面板尺寸按实测文字宽度定，不用写死的数字
    float maxW = 0.0f;
    for (int i = 0; i < lines; ++i)
    {
        const float w = hudTextWidth(kHelp[i]);
        if (w > maxW) maxW = w;
    }
    const float pad  = 18.0f;
    const int   rows = lines + 1;                 // +1 是标题行

    float ph = rows * kHudLineH + pad * 2.0f;
    if (ph > H - 16.0f) ph = H - 16.0f;
    float pw = maxW + pad * 2.0f;
    if (pw > W - 16.0f) pw = W - 16.0f;

    const float px = (W - pw) * 0.5f;
    const float py = (H - ph) * 0.5f;
    hudPanel(px, py, pw, ph, 0.92f);

    float ty = py + ph - pad - 6.0f;
    hudColor(1.0f, 0.85f, 0.35f);
    hudText(px + pad, ty, "3D Model Viewer  -  help   (h or F1 to close)");

    hudColor(0.95f, 0.95f, 0.98f);
    ty -= kHudLineH * 1.5f;
    for (int i = 0; i < lines; ++i)
    {
        if (ty < py + 6.0f) break;
        hudText(px + pad, ty, kHelp[i]);
        ty -= kHudLineH;
    }

    hudEnd();
}

// ---------------------------------------------------------------------------
// HUD 叠加层
// ---------------------------------------------------------------------------
void drawOverlay()
{
    if (g_app.showHelp) { drawHelpScreen(); return; }
    if (!g_app.showHUD) return;
    const float W=(float)g_app.winW, H=(float)g_app.winH;
    hudBegin(g_app.winW,g_app.winH);
    // Keep the status in its own strip instead of overwriting viewport labels.
    hudPanel(0,H-54,W,54,0.82f);
    hudColor(0.91f,0.95f,1.0f);
    hudTextf(16,H-21,"CG LAB  /  %s  /  %s",g_app.sceneKind==SceneKind::Forest?"FOREST":"STUDIO",
             g_app.enhanced?renderer::name():"Classic OpenGL");
    const Object* selected=g_app.scene.selectedObject();
    int triangles=0;
    for(const auto& mesh:g_app.scene.meshes) if(mesh) triangles+=mesh->data().triangleCount;
    hudColor(0.66f,0.76f,0.85f);
    hudTextf(16,H-41,"%s  |  %s  |  %d triangles  |  %s%s%s",
             selected?selected->name.c_str():"No selection",projModeName(g_app.projMode),triangles,
             g_app.transMode==TransMode::Translate?"TRANSLATE":g_app.transMode==TransMode::Rotate?"ROTATE":"SCALE",
             robotAlive()?"  |  Robot: ":"",
             robotAlive()?(robotWalking()?std::string(robotPoseName(robotPose()))+" + walking"
                                        :std::string(robotPoseName(robotPose()))).c_str():"");
    hudPanel(0,0,W,30,0.82f);
    hudColor(0.72f,0.80f,0.87f);
    hudText(16,10,"H Help   F2 Renderer   F3 Shadows   Space Animate   Tab Select   N Pose   B Walk   F12 Screenshot");
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
    if (g_app.sceneKind == SceneKind::Forest) robotCreate(g_app.scene);

    g_app.camera.reset(g_app.scene.radius());

    // 森林场景的默认机位要显式给。
    // camera.reset() 按「场景半径 × 2.8」摆机位，森林半径是 10（留给雾足够的景深）
    // → 距离 28 单位，而兔子只有 1.5 单位高，才占画面高度的 6%，开进去几乎什么都看不见。
    if (g_app.sceneKind == SceneKind::Forest)
    {
        g_app.camera.eye    = v3(2.6f, 2.2f, 4.6f);   // 斜前上方，距兔子约 5.7 单位
        g_app.camera.target = v3(0.0f, 0.7f, 0.0f);   // 看向兔子身体中部
        g_app.camera.up     = v3(0.0f, 1.0f, 0.0f);
    }

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

    // 机器人关节动画：与录制/预览共用同一时间轴（录制=帧号/fps，其余=真实时钟），
    // 且在阴影 pass 之前更新，保证本帧的层级矩阵在所有绘制路径里一致。
    if (robotAlive())
    {
        const float rt = g_rec.active ? (float)g_rec.frame / (float)g_rec.fps
                                      : (float)glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
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
        glutTimerFunc(16, previewTick, 0);
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
        g_app.camera.eye = v3(2.6f,2.2f,4.6f);
        g_app.camera.target = v3(0,0.7f,0);
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
    std::snprintf(buf, sizeof(buf), "3D Model Viewer  -  %s  -  %s",
                  projModeName(g_app.projMode), sel ? sel->name.c_str() : "(none)");
    glutSetWindowTitle(buf);
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
