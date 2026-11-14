#include "texture.h"

// stb_image 的实现在这个 .cpp 里展开，且只在这一个编译单元里展开。
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO_DEPRECATED
#if defined(_MSC_VER)
  #pragma warning(push, 0)      // 第三方头文件不参与 /W3 告警
#endif
#include "stb_image.h"
#if defined(_MSC_VER)
  #pragma warning(pop)
#endif

#include <cstdio>

// GL_CLAMP_TO_EDGE 是 OpenGL 1.2 的令牌，Windows SDK 的 <GL/gl.h> 只到 1.1，
// 所以这里自己定义，避免依赖 glext.h。
#ifndef GL_CLAMP_TO_EDGE
  #define GL_CLAMP_TO_EDGE 0x812F
#endif

GLuint loadTexture2D(const std::string& path, int* wOut, int* hOut)
{
    if (wOut) *wOut = 0;
    if (hOut) *hOut = 0;
    if (path.empty()) return 0;

    stbi_set_flip_vertically_on_load(1);   // OBJ 的 vt 原点在左下，stb 首行是图像顶部

    int w = 0, h = 0, comp = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &comp, STBI_rgb_alpha);
    if (!pixels)
    {
        std::fprintf(stderr, "[texture] load failed: %s (%s)\n", path.c_str(), stbi_failure_reason());
        return 0;
    }

    const GLenum fmt = GL_RGBA;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // 用 gluBuild2DMipmaps 而不是裸 glTexImage2D：它会对非 2 的幂尺寸自动缩放，
    // 这样任意贴图都不会因 NPOT 而失效。
    gluBuild2DMipmaps(GL_TEXTURE_2D, fmt, w, h, fmt, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // 用白模调制：GL_MODULATE 下纹理会乘以材质色，若不设白色，兔子会被 .mtl 的
    // Kd(0.5882) 整体压暗。
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);

    if (wOut) *wOut = w;
    if (hOut) *hOut = h;
    std::printf("[texture] %s -> %dx%d, %d channels\n", path.c_str(), w, h, comp);
    return tex;
}
