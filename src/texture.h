#pragma once
// 纹理加载：stb_image（public domain，单头文件，无需额外 DLL）→ OpenGL 纹理对象。
#include <GL/freeglut.h>
#include <string>
#include <vector>

// 加载图片并上传为 2D 纹理。失败返回 0。宽高通过出参返回（可为 nullptr）。
GLuint loadTexture2D(const std::string& path, int* wOut = nullptr, int* hOut = nullptr);
bool readSkinPixels(const std::string& path, std::vector<unsigned char>& pixels);
GLuint uploadSkinPixels(const unsigned char* pixels);
