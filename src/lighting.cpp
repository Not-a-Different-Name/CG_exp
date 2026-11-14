#include "lighting.h"
#include "app.h"

namespace {

// ---------------------------------------------------------------------------
// 展示台场景（单位尺度）的光源。
// 场景已归一化到单位包围球，所以光源距离取 2~3 个单位。
// w = 1 表示点光源（位置），w = 0 表示定向光（方向）。
// ---------------------------------------------------------------------------
const GLfloat kLight0Pos[]      = {  2.2f,  3.0f,  2.6f, 1.0f };   // 主光：右上前方
const GLfloat kLight0Ambient[]  = {  0.12f, 0.12f, 0.14f, 1.0f };
const GLfloat kLight0Diffuse[]  = {  0.92f, 0.90f, 0.86f, 1.0f };
const GLfloat kLight0Specular[] = {  1.00f, 1.00f, 1.00f, 1.0f };

const GLfloat kLight1Pos[]      = { -2.5f,  1.2f, -2.0f, 0.0f };   // 补光：左后方定向
const GLfloat kLight1Diffuse[]  = {  0.30f, 0.33f, 0.40f, 1.0f };
const GLfloat kLight1Specular[] = {  0.10f, 0.10f, 0.10f, 1.0f };

const GLfloat kGlobalAmbient[]  = {  0.22f, 0.22f, 0.24f, 1.0f };

// ---------------------------------------------------------------------------
// 森林场景的光源。尺度完全不同（树高 10~22 单位、视野 60+ 单位），
// 直接沿用展示台那套会在地上"烧"出一块光斑：点光源挂在 y=3 而地面在 y=0，
// 加上线性衰减，亮度就全集中在原点附近，看着非常假。
// 这里改成太阳：定向光、不衰减、偏暖。
// ---------------------------------------------------------------------------
const GLfloat kSunPos[]      = {  0.45f,  0.80f,  0.40f, 0.0f };   // w=0 -> 方向
const GLfloat kSunAmbient[]  = {  0.30f,  0.32f,  0.30f, 1.0f };
const GLfloat kSunDiffuse[]  = {  0.78f,  0.76f,  0.62f, 1.0f };
const GLfloat kSunSpecular[] = {  0.10f,  0.10f,  0.08f, 1.0f };

// 林间漫射：环境光要明显高于展示台，否则背光面死黑
const GLfloat kForestAmbient[] = {  0.34f,  0.38f,  0.36f, 1.0f };

inline bool isForest() { return g_app.sceneKind == SceneKind::Forest; }

} // namespace

void initLighting()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // 交互缩放会产生非均匀缩放，法线必须重新归一化，否则光照会失真
    glEnable(GL_NORMALIZE);

    // 双面光照：地面平台和任何开放曲面的背面也需要被照亮，否则是黑的
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    // 局部视点：高光更紧致、更正确（无限远视点下高光会散开）
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

    if (isForest())
    {
        glLightfv(GL_LIGHT0, GL_AMBIENT,  kSunAmbient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE,  kSunDiffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, kSunSpecular);
        // 定向光没有位置，线性衰减无从谈起
        glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.0f);
        glDisable(GL_LIGHT1);

        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, kForestAmbient);
    }
    else
    {
        glEnable(GL_LIGHT1);
        glLightfv(GL_LIGHT0, GL_AMBIENT,  kLight0Ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE,  kLight0Diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, kLight0Specular);
        glLightf (GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.06f);

        glLightfv(GL_LIGHT1, GL_DIFFUSE,  kLight1Diffuse);
        glLightfv(GL_LIGHT1, GL_SPECULAR, kLight1Specular);

        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, kGlobalAmbient);
    }
}

void applyLightPositions()
{
    if (isForest())
    {
        glLightfv(GL_LIGHT0, GL_POSITION, kSunPos);
        return;
    }
    glLightfv(GL_LIGHT0, GL_POSITION, kLight0Pos);
    glLightfv(GL_LIGHT1, GL_POSITION, kLight1Pos);
}
