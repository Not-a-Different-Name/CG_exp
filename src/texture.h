#pragma once
// 纹理加载：stb_image（public domain，单头文件，无需额外 DLL）→ OpenGL 纹理对象。
#include <GL/freeglut.h>
#include <string>
#include <vector>

// 加载图片并上传为 2D 纹理。失败返回 0。宽高通过出参返回（可为 nullptr）。
GLuint loadTexture2D(const std::string& path, int* wOut = nullptr, int* hOut = nullptr);
// 读取方块角色皮肤：64x64（现代）、64x32（旧版）或 64 的整数倍 HD 皮肤。
// 返回 RGBA 像素（行 0 = 图像顶部），尺寸通过出参返回。
bool readSkinPixels(const std::string& path, std::vector<unsigned char>& pixels,
                    int* wOut = nullptr, int* hOut = nullptr);
GLuint uploadSkinPixels(const unsigned char* pixels, int width, int height);
