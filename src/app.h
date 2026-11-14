#pragma once
// 程序全局状态与渲染主流程。
#include "anim.h"
#include "camera.h"
#include "scene.h"
#include "types.h"

#include <string>

struct AppState
{
    Scene  scene;
    Camera camera;

    ProjMode   projMode   = ProjMode::Perspective;
    ViewPreset viewPreset = ViewPreset::Free;      // Free 表示用 camera 自己的朝向
    TransMode  transMode  = TransMode::Rotate;
    RenderMode renderMode = RenderMode::Solid;

    bool enhanced    = true;
    bool shadows     = true;
    bool lighting    = true;
    bool textureOn   = true;
    bool cullFace    = true;
    bool smoothShade = true;

    bool showGrid   = true;
    bool showAxes   = true;
    bool showGround = true;
    bool showHUD    = true;
    bool showHelp   = false;

    bool quadView   = false;                       // 四视口：三视图 + 透视同屏

    // 默认就是森林 + 兔子。展示台那套（实验四立方体/茶壶/球）改成 --scene studio 才进，
    // 它主要是给实验报告出图用的。
    SceneKind sceneKind = SceneKind::Forest;
    // 雾（森林场景）：固定管线的雾是逐顶点的，能拉出纵深、也遮住林地边界
    bool  fog        = false;
    float fogDensity = 0.026f;
    float fogColor[4] = { 0.58f, 0.65f, 0.70f, 1.0f };

    int  winW = 1280;
    int  winH = 800;

    // 鼠标拖动状态
    enum class DragMode { None, Object, Camera, Pan };
    DragMode dragMode = DragMode::None;
    int      lastX = 0, lastY = 0;

    float obliqueAngleDeg = 45.0f;                 // 斜平行投影屏幕上呈现的斜边夹角

    std::string modelPath;
    UpAxis      modelUp = UpAxis::Auto;   // 模型的上轴，Auto 按几何判断
    std::string statusMsg;
    std::string modelStatus;

    // 帧率统计
    int   frameCount  = 0;
    int   lastFpsMs   = 0;
    float fps         = 0.0f;
};

extern AppState g_app;

void appSetup(const std::string& modelPath);
void appReshape(int w, int h);
void appDisplay();
void appCleanup();
void appPostRedisplay();

// 切换投影 / 视图 / 复位
void appSetProjection(ProjMode m);
void appSetViewPreset(ViewPreset v);
void appResetCamera();
void appResetObjects();

// 实验五/六：把一次增量变换左乘进选中物体的 objectXform
void appAccumulateObjectTransform(const float delta[16]);

// 把当前帧缓冲存成 BMP（F12），也方便写实验报告
void appSaveScreenshot(const char* path);
void appUpdateTitle();

// 请求在「下一帧画完、交换缓冲之前」截图，然后退出主循环。
// 从后缓冲读，所以窗口可以隐藏（batch 出图时不会再闪窗口）。
void appRequestShot(const char* path);

// ---- 视频录制 ----
struct RecordOptions
{
    std::string path;                 // 输出 mp4 路径
    AnimKind    anim    = AnimKind::None;
    int         fps     = 30;
    float       seconds = 6.0f;
};

// 请求录制。真正的 ffmpeg 子进程要等窗口尺寸确定（reshape 被调用过）之后才启动，
// 否则会用错误的尺寸去读帧缓冲。
void appStartRecording(const RecordOptions& opt);

// ---- 实时预览 ----
// 不录制，直接在窗口里循环播放某段动画（空格键切换）。
// 时间轴走真实时钟，和录制的「帧序号 / fps」不同 —— 录制要的是可复现，
// 预览要的是看着舒服。
void appStartPreview(AnimKind kind, float loopSeconds);
void appStopPreview();
void appTogglePreview();
bool appPreviewActive();
AnimKind appPreviewKind();

int appExitCode();

bool appRecordingActive();
