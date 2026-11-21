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
#include <filesystem>
#include <memory>

// GL_CLAMP_TO_EDGE 是 OpenGL 1.2 的令牌，Windows SDK 的 <GL/gl.h> 只到 1.1，
// 所以这里自己定义，避免依赖 glext.h。
#ifndef GL_CLAMP_TO_EDGE
  #define GL_CLAMP_TO_EDGE 0x812F
#endif

void appLogStage(const char* stage);   // app.cpp 中的启动阶段日志

namespace {
std::FILE* openImage(const std::string& path)
{
#ifdef _WIN32
    return _wfopen(std::filesystem::u8path(path).c_str(), L"rb");
#else
    return std::fopen(path.c_str(), "rb");
#endif
}
}

GLuint loadTexture2D(const std::string& path, int* wOut, int* hOut)
{
    if (wOut) *wOut = 0;
    if (hOut) *hOut = 0;
    if (path.empty()) return 0;

    stbi_set_flip_vertically_on_load(1);   // OBJ 的 vt 原点在左下，stb 首行是图像顶部

    appLogStage("texture: decode begin");
    int w = 0, h = 0, comp = 0;
    std::unique_ptr<std::FILE, decltype(&std::fclose)> file(openImage(path), std::fclose);
    unsigned char* pixels = file ? stbi_load_from_file(file.get(), &w, &h, &comp, STBI_rgb_alpha) : nullptr;
    if (!pixels)
    {
        std::fprintf(stderr, "[texture] load failed: %s (%s)\n", path.c_str(),
                     file ? stbi_failure_reason() : "unable to open image");
        return 0;
    }
    appLogStage("texture: decoded");

    const GLenum fmt = GL_RGBA;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    appLogStage("texture: gl begin");

    // 用 gluBuild2DMipmaps 而不是裸 glTexImage2D：它会对非 2 的幂尺寸自动缩放，
    // 这样任意贴图都不会因 NPOT 而失效。
    gluBuild2DMipmaps(GL_TEXTURE_2D, fmt, w, h, fmt, GL_UNSIGNED_BYTE, pixels);
    appLogStage("texture: mipmaps done");

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

bool readSkinPixels(const std::string& path, std::vector<unsigned char>& pixels,
                    int* wOut, int* hOut)
{
    if (wOut) *wOut = 0;
    if (hOut) *hOut = 0;
    std::unique_ptr<std::FILE, decltype(&std::fclose)> file(openImage(path), std::fclose);
    if (!file) return false;
    const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    for (unsigned char byte : signature)
        if (std::fgetc(file.get()) != byte) return false;
    std::rewind(file.get());
    int w = 0, h = 0, channels = 0;
    if (!stbi_info_from_file(file.get(), &w, &h, &channels)) return false;

    // 64x64 = 现代皮肤；64x32 = 旧版（左臂/左腿由右臂/右腿镜像）；
    // 其他 64 的整数倍正方形 = HD 皮肤（128x128、256x256）。
    const bool modern = (w == 64 && h == 64);
    const bool legacy = (w == 64 && h == 32);
    const bool hd     = (w == h && w > 64 && w % 64 == 0);
    if (!modern && !legacy && !hd) return false;

    std::rewind(file.get());
    stbi_set_flip_vertically_on_load(0);
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> decoded(
        stbi_load_from_file(file.get(), &w, &h, &channels, STBI_rgb_alpha), stbi_image_free);
    if (!decoded || !((w == 64 && (h == 64 || h == 32)) || (w == h && w > 64 && w % 64 == 0)))
        return false;
    pixels.assign(decoded.get(), decoded.get() + (size_t)w * h * 4);
    if (wOut) *wOut = w;
    if (hOut) *hOut = h;
    return true;
}

GLuint uploadSkinPixels(const unsigned char* pixels, int width, int height)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}
