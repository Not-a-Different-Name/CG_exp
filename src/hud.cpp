#include "hud.h"

#include <GL/freeglut.h>
#include <cstdarg>
#include <cstdio>

namespace {
void* font() { return GLUT_BITMAP_9_BY_15; }
} // namespace

void hudBegin(int viewportW, int viewportH)
{
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT);
    glDisable(GL_FOG);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (viewportW <= 0) viewportW = 1;
    if (viewportH <= 0) viewportH = 1;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    // 直接用像素坐标：glViewport 已经把 NDC 映射到当前视口，
    // 所以这里的 (0,0) 就是当前视口的左下角。
    gluOrtho2D(0.0, (double)viewportW, 0.0, (double)viewportH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
}

void hudEnd()
{
    glPopAttrib();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void hudColor(float r, float g, float b)
{
    glColor3f(r, g, b);
}

void hudText(float x, float y, const std::string& s)
{
    glRasterPos2f(x, y);
    for (unsigned char c : s)
        glutBitmapCharacter(font(), (int)c);
}

void hudTitle(float x, float y, const std::string& s)
{
    glRasterPos2f(x, y);
    for (unsigned char c : s)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, (int)c);
}

void hudTextf(float x, float y, const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    hudText(x, y, buf);
}

float hudTextWidth(const std::string& s)
{
    int w = 0;
    for (unsigned char c : s)
        w += glutBitmapWidth(font(), (int)c);
    return (float)w;
}

void hudPanel(float x, float y, float w, float h, float alpha)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.05f, 0.05f, 0.08f, alpha);
    glBegin(GL_QUADS);
        glVertex2f(x,     y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x,     y + h);
    glEnd();
    glDisable(GL_BLEND);
    glColor3f(1.0f, 1.0f, 1.0f);
}
