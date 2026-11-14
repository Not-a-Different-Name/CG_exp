# 三维模型查看器 —— 完整源代码

计算机图形学总结实验（实验四 / 五 / 六 / 七 / 十 + 斜平行投影 + 程序化森林）

用 **OpenGL + GLSL** 实现逐像素光照与 PCF 阴影，保留 GLUT 与经典管线对照。

---

## 统计

| 项 | 值 |
|---|---|
| 源文件 | 44 个 |
| 代码总行数 | 5718 行（不含本页的 README 部分）| 
| 模型 | `Models/bunny_10k.obj`（Stanford Bunny，10000 面，Y-up）|
| 依赖 | 已 vendor：freeglut（x64）+ stb_image v2.30，**离线可构建** |

## 构建与运行

```bash
CM="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
"$CM" -S . -B build -G "Visual Studio 17 2022" -A x64
"$CM" --build build --config Release
./build/Release/viewer.exe
```

> CMake 不在 PATH 里，要用 VS 自带的那份；`freeglut.dll` 构建时会自动拷到 exe 旁。

## 文件清单

| # | 文件 | 行数 | 说明 |
|---|---|---|---|
| 1 | `src/gl_api.h` | 95 | OpenGL runtime entry points |
| 2 | `src/shaders.h` | 66 | GLSL per-pixel lighting and PCF shadows |
| 3 | `src/renderer.h` | 14 | Renderer interface |
| 4 | `src/renderer.cpp` | 150 | Shadow pass and forward shading |
| 5 | `tests/obj_loader_test.cpp` | 31 | OBJ regression tests |
| 6 | `tools/render_smoke.py` | 36 | GPU rendering regression checks |
| 7 | `CMakeLists.txt` | 112 | 构建配置（含 32 位防护） |
| 8 | `CMakePresets.json` | 40 | CMake 预设：只提供 x64，避免选错 |
| 9 | `build.bat` | 82 | 一键构建（Windows cmd / PowerShell） |
| 10 | `build.sh` | 78 | 一键构建（git-bash / MSYS2） |
| 11 | `viewer.vcxproj` | 176 | 经典 VS 工程（自包含，双击 viewer.sln 即可） |
| 12 | `.gitignore` | 25 | Git 忽略规则 |
| 13 | `src/types.h` | 27 | 全局模式枚举 |
| 14 | `src/math3d.h` | 136 | 向量 / 矩阵 / 四元数工具（header-only） |
| 15 | `src/obj_loader.h` | 62 | OBJ 导入接口（实验十） |
| 16 | `src/obj_loader.cpp` | 408 | OBJ / MTL 解析：四边形三角化 + 顶点去重 |
| 17 | `src/mesh.h` | 50 | 网格接口 |
| 18 | `src/mesh.cpp` | 272 | 网格：归一化、上轴判别、客户端顶点数组绘制 |
| 19 | `src/texture.h` | 7 | 纹理接口 |
| 20 | `src/texture.cpp` | 63 | stb_image → OpenGL 纹理 |
| 21 | `src/camera.h` | 38 | 观察变换相机接口 |
| 22 | `src/camera.cpp` | 123 | 相机：环绕 / 漫游 / 四元数旋转 / 退化保护（实验七） |
| 23 | `src/projection.h` | 21 | 投影接口 |
| 24 | `src/projection.cpp` | 171 | 透视 / 正交 / 斜平行投影 + 保宽高比 |
| 25 | `src/lighting.h` | 15 | 光照接口 |
| 26 | `src/lighting.cpp` | 89 | 光照与材质（展示台点光源 / 森林太阳光） |
| 27 | `src/forest.h` | 28 | 程序化森林接口 |
| 28 | `src/forest.cpp` | 321 | 森林：地面 + 三种树型，烘焙成一个顶点色网格 |
| 29 | `src/scene.h` | 63 | 场景与物体接口 |
| 30 | `src/scene.cpp` | 375 | 两套场景：展示台（实验四/五）与森林 |
| 31 | `src/anim.h` | 30 | 脚本动画接口 |
| 32 | `src/anim.cpp` | 289 | 动画（时间的纯函数）：自转/环绕/投影/变换/森林漫步 |
| 33 | `src/recorder.h` | 19 | 录制接口 |
| 34 | `src/recorder.cpp` | 76 | 逐帧数据管道进 ffmpeg 编码 mp4 |
| 35 | `src/hud.h` | 21 | 叠加文字接口 |
| 36 | `src/hud.cpp` | 89 | 屏幕叠加文字与帮助面板 |
| 37 | `src/input.h` | 47 | 输入接口 |
| 38 | `src/input.cpp` | 382 | 键盘 / 鼠标 / 滚轮 / 右键菜单（实验六） |
| 39 | `src/app.h` | 114 | 程序状态接口 |
| 40 | `src/app.cpp` | 741 | 渲染主流程、HUD、截图、四视口、录制调度 |
| 41 | `src/main.cpp` | 403 | 入口：GLUT 初始化、命令行解析、回调注册、自检 |
| 42 | `tools/bmp2png.py` | 74 | 截图 BMP → PNG（纯标准库） |
| 43 | `tools/make_shots.sh` | 54 | 一键重新生成 shots/ 全部截图 |
| 44 | `tools/make_package.py` | 205 | 本脚本：生成这份代码文档 |

---

## 源代码

### 1. `src/gl_api.h`

OpenGL runtime entry points（95 行）

```cpp
#pragma once
#include <GL/freeglut.h>
#include <cstddef>
#include <cstdint>
// Small, explicit runtime API table; no additional binary dependency.
namespace glapi {
inline GLuint (APIENTRY *CreateShader)(GLenum) = nullptr;
inline void (APIENTRY *ShaderSource)(GLuint, GLsizei, const char* const*, const GLint*) = nullptr;
inline void (APIENTRY *CompileShader)(GLuint) = nullptr;
inline void (APIENTRY *GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
inline void (APIENTRY *GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, char*) = nullptr;
inline void (APIENTRY *DeleteShader)(GLuint) = nullptr;
inline GLuint (APIENTRY *CreateProgram)(void) = nullptr;
inline void (APIENTRY *AttachShader)(GLuint, GLuint) = nullptr;
inline void (APIENTRY *LinkProgram)(GLuint) = nullptr;
inline void (APIENTRY *GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
inline void (APIENTRY *GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, char*) = nullptr;
inline void (APIENTRY *DeleteProgram)(GLuint) = nullptr;
inline void (APIENTRY *UseProgram)(GLuint) = nullptr;
inline GLint (APIENTRY *GetUniformLocation)(GLuint, const char*) = nullptr;
inline void (APIENTRY *Uniform1i)(GLint, GLint) = nullptr;
inline void (APIENTRY *Uniform1f)(GLint, GLfloat) = nullptr;
inline void (APIENTRY *Uniform3f)(GLint, GLfloat, GLfloat, GLfloat) = nullptr;
inline void (APIENTRY *UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;
inline void (APIENTRY *ActiveTexture)(GLenum) = nullptr;
inline void (APIENTRY *GenFramebuffers)(GLsizei, GLuint*) = nullptr;
inline void (APIENTRY *BindFramebuffer)(GLenum, GLuint) = nullptr;
inline void (APIENTRY *FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
inline GLenum (APIENTRY *CheckFramebufferStatus)(GLenum) = nullptr;
inline void (APIENTRY *DeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
inline void (APIENTRY *GenBuffers)(GLsizei, GLuint*) = nullptr;
inline void (APIENTRY *BindBuffer)(GLenum, GLuint) = nullptr;
inline void (APIENTRY *BufferData)(GLenum, std::ptrdiff_t, const void*, GLenum) = nullptr;
inline void (APIENTRY *DeleteBuffers)(GLsizei, const GLuint*) = nullptr;
inline bool load() {
 bool ok = true;
 CreateShader = reinterpret_cast<decltype(CreateShader)>(glutGetProcAddress("glCreateShader"));
 ok = ok && CreateShader != nullptr && reinterpret_cast<std::uintptr_t>(CreateShader) > 3 && reinterpret_cast<std::uintptr_t>(CreateShader) != static_cast<std::uintptr_t>(-1);
 ShaderSource = reinterpret_cast<decltype(ShaderSource)>(glutGetProcAddress("glShaderSource"));
 ok = ok && ShaderSource != nullptr && reinterpret_cast<std::uintptr_t>(ShaderSource) > 3 && reinterpret_cast<std::uintptr_t>(ShaderSource) != static_cast<std::uintptr_t>(-1);
 CompileShader = reinterpret_cast<decltype(CompileShader)>(glutGetProcAddress("glCompileShader"));
 ok = ok && CompileShader != nullptr && reinterpret_cast<std::uintptr_t>(CompileShader) > 3 && reinterpret_cast<std::uintptr_t>(CompileShader) != static_cast<std::uintptr_t>(-1);
 GetShaderiv = reinterpret_cast<decltype(GetShaderiv)>(glutGetProcAddress("glGetShaderiv"));
 ok = ok && GetShaderiv != nullptr && reinterpret_cast<std::uintptr_t>(GetShaderiv) > 3 && reinterpret_cast<std::uintptr_t>(GetShaderiv) != static_cast<std::uintptr_t>(-1);
 GetShaderInfoLog = reinterpret_cast<decltype(GetShaderInfoLog)>(glutGetProcAddress("glGetShaderInfoLog"));
 ok = ok && GetShaderInfoLog != nullptr && reinterpret_cast<std::uintptr_t>(GetShaderInfoLog) > 3 && reinterpret_cast<std::uintptr_t>(GetShaderInfoLog) != static_cast<std::uintptr_t>(-1);
 DeleteShader = reinterpret_cast<decltype(DeleteShader)>(glutGetProcAddress("glDeleteShader"));
 ok = ok && DeleteShader != nullptr && reinterpret_cast<std::uintptr_t>(DeleteShader) > 3 && reinterpret_cast<std::uintptr_t>(DeleteShader) != static_cast<std::uintptr_t>(-1);
 CreateProgram = reinterpret_cast<decltype(CreateProgram)>(glutGetProcAddress("glCreateProgram"));
 ok = ok && CreateProgram != nullptr && reinterpret_cast<std::uintptr_t>(CreateProgram) > 3 && reinterpret_cast<std::uintptr_t>(CreateProgram) != static_cast<std::uintptr_t>(-1);
 AttachShader = reinterpret_cast<decltype(AttachShader)>(glutGetProcAddress("glAttachShader"));
 ok = ok && AttachShader != nullptr && reinterpret_cast<std::uintptr_t>(AttachShader) > 3 && reinterpret_cast<std::uintptr_t>(AttachShader) != static_cast<std::uintptr_t>(-1);
 LinkProgram = reinterpret_cast<decltype(LinkProgram)>(glutGetProcAddress("glLinkProgram"));
 ok = ok && LinkProgram != nullptr && reinterpret_cast<std::uintptr_t>(LinkProgram) > 3 && reinterpret_cast<std::uintptr_t>(LinkProgram) != static_cast<std::uintptr_t>(-1);
 GetProgramiv = reinterpret_cast<decltype(GetProgramiv)>(glutGetProcAddress("glGetProgramiv"));
 ok = ok && GetProgramiv != nullptr && reinterpret_cast<std::uintptr_t>(GetProgramiv) > 3 && reinterpret_cast<std::uintptr_t>(GetProgramiv) != static_cast<std::uintptr_t>(-1);
 GetProgramInfoLog = reinterpret_cast<decltype(GetProgramInfoLog)>(glutGetProcAddress("glGetProgramInfoLog"));
 ok = ok && GetProgramInfoLog != nullptr && reinterpret_cast<std::uintptr_t>(GetProgramInfoLog) > 3 && reinterpret_cast<std::uintptr_t>(GetProgramInfoLog) != static_cast<std::uintptr_t>(-1);
 DeleteProgram = reinterpret_cast<decltype(DeleteProgram)>(glutGetProcAddress("glDeleteProgram"));
 ok = ok && DeleteProgram != nullptr && reinterpret_cast<std::uintptr_t>(DeleteProgram) > 3 && reinterpret_cast<std::uintptr_t>(DeleteProgram) != static_cast<std::uintptr_t>(-1);
 UseProgram = reinterpret_cast<decltype(UseProgram)>(glutGetProcAddress("glUseProgram"));
 ok = ok && UseProgram != nullptr && reinterpret_cast<std::uintptr_t>(UseProgram) > 3 && reinterpret_cast<std::uintptr_t>(UseProgram) != static_cast<std::uintptr_t>(-1);
 GetUniformLocation = reinterpret_cast<decltype(GetUniformLocation)>(glutGetProcAddress("glGetUniformLocation"));
 ok = ok && GetUniformLocation != nullptr && reinterpret_cast<std::uintptr_t>(GetUniformLocation) > 3 && reinterpret_cast<std::uintptr_t>(GetUniformLocation) != static_cast<std::uintptr_t>(-1);
 Uniform1i = reinterpret_cast<decltype(Uniform1i)>(glutGetProcAddress("glUniform1i"));
 ok = ok && Uniform1i != nullptr && reinterpret_cast<std::uintptr_t>(Uniform1i) > 3 && reinterpret_cast<std::uintptr_t>(Uniform1i) != static_cast<std::uintptr_t>(-1);
 Uniform1f = reinterpret_cast<decltype(Uniform1f)>(glutGetProcAddress("glUniform1f"));
 ok = ok && Uniform1f != nullptr && reinterpret_cast<std::uintptr_t>(Uniform1f) > 3 && reinterpret_cast<std::uintptr_t>(Uniform1f) != static_cast<std::uintptr_t>(-1);
 Uniform3f = reinterpret_cast<decltype(Uniform3f)>(glutGetProcAddress("glUniform3f"));
 ok = ok && Uniform3f != nullptr && reinterpret_cast<std::uintptr_t>(Uniform3f) > 3 && reinterpret_cast<std::uintptr_t>(Uniform3f) != static_cast<std::uintptr_t>(-1);
 UniformMatrix4fv = reinterpret_cast<decltype(UniformMatrix4fv)>(glutGetProcAddress("glUniformMatrix4fv"));
 ok = ok && UniformMatrix4fv != nullptr && reinterpret_cast<std::uintptr_t>(UniformMatrix4fv) > 3 && reinterpret_cast<std::uintptr_t>(UniformMatrix4fv) != static_cast<std::uintptr_t>(-1);
 ActiveTexture = reinterpret_cast<decltype(ActiveTexture)>(glutGetProcAddress("glActiveTexture"));
 ok = ok && ActiveTexture != nullptr && reinterpret_cast<std::uintptr_t>(ActiveTexture) > 3 && reinterpret_cast<std::uintptr_t>(ActiveTexture) != static_cast<std::uintptr_t>(-1);
 GenFramebuffers = reinterpret_cast<decltype(GenFramebuffers)>(glutGetProcAddress("glGenFramebuffers"));
 ok = ok && GenFramebuffers != nullptr && reinterpret_cast<std::uintptr_t>(GenFramebuffers) > 3 && reinterpret_cast<std::uintptr_t>(GenFramebuffers) != static_cast<std::uintptr_t>(-1);
 BindFramebuffer = reinterpret_cast<decltype(BindFramebuffer)>(glutGetProcAddress("glBindFramebuffer"));
 ok = ok && BindFramebuffer != nullptr && reinterpret_cast<std::uintptr_t>(BindFramebuffer) > 3 && reinterpret_cast<std::uintptr_t>(BindFramebuffer) != static_cast<std::uintptr_t>(-1);
 FramebufferTexture2D = reinterpret_cast<decltype(FramebufferTexture2D)>(glutGetProcAddress("glFramebufferTexture2D"));
 ok = ok && FramebufferTexture2D != nullptr && reinterpret_cast<std::uintptr_t>(FramebufferTexture2D) > 3 && reinterpret_cast<std::uintptr_t>(FramebufferTexture2D) != static_cast<std::uintptr_t>(-1);
 CheckFramebufferStatus = reinterpret_cast<decltype(CheckFramebufferStatus)>(glutGetProcAddress("glCheckFramebufferStatus"));
 ok = ok && CheckFramebufferStatus != nullptr && reinterpret_cast<std::uintptr_t>(CheckFramebufferStatus) > 3 && reinterpret_cast<std::uintptr_t>(CheckFramebufferStatus) != static_cast<std::uintptr_t>(-1);
 DeleteFramebuffers = reinterpret_cast<decltype(DeleteFramebuffers)>(glutGetProcAddress("glDeleteFramebuffers"));
 ok = ok && DeleteFramebuffers != nullptr && reinterpret_cast<std::uintptr_t>(DeleteFramebuffers) > 3 && reinterpret_cast<std::uintptr_t>(DeleteFramebuffers) != static_cast<std::uintptr_t>(-1);
 GenBuffers = reinterpret_cast<decltype(GenBuffers)>(glutGetProcAddress("glGenBuffers"));
 ok = ok && GenBuffers != nullptr && reinterpret_cast<std::uintptr_t>(GenBuffers) > 3 && reinterpret_cast<std::uintptr_t>(GenBuffers) != static_cast<std::uintptr_t>(-1);
 BindBuffer = reinterpret_cast<decltype(BindBuffer)>(glutGetProcAddress("glBindBuffer"));
 ok = ok && BindBuffer != nullptr && reinterpret_cast<std::uintptr_t>(BindBuffer) > 3 && reinterpret_cast<std::uintptr_t>(BindBuffer) != static_cast<std::uintptr_t>(-1);
 BufferData = reinterpret_cast<decltype(BufferData)>(glutGetProcAddress("glBufferData"));
 ok = ok && BufferData != nullptr && reinterpret_cast<std::uintptr_t>(BufferData) > 3 && reinterpret_cast<std::uintptr_t>(BufferData) != static_cast<std::uintptr_t>(-1);
 DeleteBuffers = reinterpret_cast<decltype(DeleteBuffers)>(glutGetProcAddress("glDeleteBuffers"));
 ok = ok && DeleteBuffers != nullptr && reinterpret_cast<std::uintptr_t>(DeleteBuffers) > 3 && reinterpret_cast<std::uintptr_t>(DeleteBuffers) != static_cast<std::uintptr_t>(-1);
 return ok;
}
}
```

### 2. `src/shaders.h`

GLSL per-pixel lighting and PCF shadows（66 行）

```cpp
#pragma once
// Embedded to keep VS, CMake, and relocated executables independent of the working directory.
namespace shaders {
inline const char* vertex = R"GLSL(#version 120
uniform mat4 model, lightVP;
uniform bool vertexColor;
uniform vec3 albedo;
varying vec3 normalEye, positionEye, baseColor;
varying vec4 shadowPosition;
varying vec2 uv;
void main() {
 vec4 eye = gl_ModelViewMatrix * gl_Vertex;
 positionEye = eye.xyz;
 normalEye = gl_NormalMatrix * gl_Normal;
 baseColor = vertexColor ? gl_Color.rgb : albedo;
 uv = gl_MultiTexCoord0.xy;
 shadowPosition = lightVP * model * gl_Vertex;
 gl_Position = gl_ProjectionMatrix * eye;
}
)GLSL";
inline const char* fragment = R"GLSL(#version 120
uniform sampler2D diffuseMap;
uniform sampler2DShadow shadowMap;
uniform bool textured, shadows, fogEnabled, smoothNormals, orthographic;
uniform vec3 sunEye, skyEye, fogColor, specular;
uniform float fogDensity, shininess;
varying vec3 normalEye, positionEye, baseColor;
varying vec4 shadowPosition;
varying vec2 uv;
float visibility(float ndl) {
 vec3 p = shadowPosition.xyz / shadowPosition.w * 0.5 + 0.5;
 if (!shadows || p.z <= 0.0 || p.z >= 1.0 || p.x <= 0.002 || p.x >= 0.998 || p.y <= 0.002 || p.y >= 0.998) return 1.0;
 float bias = max(0.00010, 0.00065 * (1.0 - ndl));
 float sum = 0.0;
 for (int x=-1;x<=1;x++) for (int y=-1;y<=1;y++) {
  sum += shadow2D(shadowMap, vec3(p.xy + vec2(float(x),float(y))/2048.0,p.z-bias)).r;
 }
 return sum/9.0;
}
void main() {
 vec3 n = normalize(normalEye);
 if (!smoothNormals) n = normalize(cross(dFdx(positionEye), dFdy(positionEye)));
 if (!gl_FrontFacing && smoothNormals) n = -n;
 vec3 l = normalize(sunEye);
 vec3 v = orthographic ? vec3(0.0,0.0,1.0) : normalize(-positionEye);
 float ndl = max(dot(n,l),0.0);
 float vis = visibility(ndl);
 vec3 base = baseColor;
 if (textured) base *= texture2D(diffuseMap,uv).rgb;
 // Decode artist colours, light in linear space, then map to the display once.
 base = pow(max(base,vec3(0.0)),vec3(2.2));
 float hemi = dot(n,normalize(skyEye))*0.5+0.5;
 vec3 ambient = mix(vec3(0.10,0.085,0.065),vec3(0.32,0.40,0.49),hemi);
 float highlight = pow(max(dot(n,normalize(l+v)),0.0),max(shininess,8.0)) * ndl;
 vec3 color = base*(ambient + vec3(1.65,1.48,1.20)*ndl*vis) + specular*highlight*vis;
 color = color/(color+vec3(0.65));
 color = pow(max(color,vec3(0.0)),vec3(1.0/2.2));
 if (fogEnabled) {
  float distance = length(positionEye);
  float f = 1.0-exp(-fogDensity*fogDensity*distance*distance);
  color = mix(color,fogColor,clamp(f,0.0,1.0));
 }
 gl_FragColor = vec4(color,1.0);
}
)GLSL";
}
```

### 3. `src/renderer.h`

Renderer interface（14 行）

```cpp
#pragma once
#include "scene.h"
#include "camera.h"
// Renderer owns programs and shadow resources. All methods require a current GL context.
namespace renderer {
bool initialize();
void shutdown();
bool available();
const char* name();
void shadowPass(const Scene& scene, SceneKind kind, bool ground, bool enabled);
void beginView(const Camera& camera, SceneKind kind, bool fog, float density, const float* fogColor, bool enabled, bool smooth);
void draw(const Object& object, bool texture);
void endView();
}
```

### 4. `src/renderer.cpp`

Shadow pass and forward shading（150 行）

```cpp
#include "renderer.h"
#include "gl_api.h"
#include "shaders.h"
#include <cstdio>
#include <algorithm>

namespace renderer {
namespace {
constexpr GLenum Framebuffer = 0x8D40, DepthAttachment = 0x8D00;
constexpr int ShadowSize = 2048;
GLuint program = 0, framebuffer = 0, depthTexture = 0;
bool active = false, shadowReady = false;
float lightVP[16];
struct Uniforms {
 GLint model, lightVP, vertexColor, albedo, diffuseMap, shadowMap, textured,
       shadows, fogEnabled, smoothNormals, orthographic, sunEye, skyEye,
       fogColor, specular, fogDensity, shininess;
} u;
GLuint compile(GLenum type, const char* source) {
 GLuint shader = glapi::CreateShader(type);
 glapi::ShaderSource(shader,1,&source,nullptr);
 glapi::CompileShader(shader);
 GLint ok=0; glapi::GetShaderiv(shader,0x8B81,&ok);
 if (!ok) {
  char log[4096]{}; glapi::GetShaderInfoLog(shader,sizeof(log),nullptr,log);
  std::fprintf(stderr,"[renderer] shader compilation failed: %s\n",log);
  glapi::DeleteShader(shader); return 0;
 }
 return shader;
}
Vec3 sunDirection(SceneKind kind) {
 return v3norm(kind==SceneKind::Forest ? v3(0.45f,0.80f,0.40f) : v3(0.6f,1.0f,0.8f));
}
bool visible(const Object& o, bool ground) {
 return o.visible && o.kind!=PrimKind::Grid && o.kind!=PrimKind::Axes && (ground || o.kind!=PrimKind::Ground);
}
}
bool available() { return program!=0; }
const char* name() { return available() ? "GLSL / PCF shadows" : "Classic OpenGL"; }
bool initialize() {
 if (!glapi::load()) { std::fprintf(stderr,"[renderer] required GL entry points unavailable; classic fallback\n"); return false; }
 GLuint vs=compile(0x8B31,shaders::vertex), fs=compile(0x8B30,shaders::fragment);
 if (!vs || !fs) { if(vs) glapi::DeleteShader(vs); if(fs) glapi::DeleteShader(fs); return false; }
 program=glapi::CreateProgram();
 glapi::AttachShader(program,vs); glapi::AttachShader(program,fs); glapi::LinkProgram(program);
 glapi::DeleteShader(vs); glapi::DeleteShader(fs);
 GLint ok=0; glapi::GetProgramiv(program,0x8B82,&ok);
 if (!ok) {
  char log[4096]{}; glapi::GetProgramInfoLog(program,sizeof(log),nullptr,log);
  std::fprintf(stderr,"[renderer] program link failed: %s\n",log); shutdown(); return false;
 }
#define LOCATION(field) u.field=glapi::GetUniformLocation(program,#field)
 LOCATION(model); LOCATION(lightVP); LOCATION(vertexColor); LOCATION(albedo);
 LOCATION(diffuseMap); LOCATION(shadowMap); LOCATION(textured); LOCATION(shadows);
 LOCATION(fogEnabled); LOCATION(smoothNormals); LOCATION(orthographic); LOCATION(sunEye);
 LOCATION(skyEye); LOCATION(fogColor); LOCATION(specular); LOCATION(fogDensity); LOCATION(shininess);
#undef LOCATION
 glGenTextures(1,&depthTexture); glBindTexture(GL_TEXTURE_2D,depthTexture);
 glTexImage2D(GL_TEXTURE_2D,0,0x81A6,ShadowSize,ShadowSize,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,0x884C,0x884E); // compare mode: reference to texture
 glTexParameteri(GL_TEXTURE_2D,0x884D,GL_LEQUAL);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
 glapi::GenFramebuffers(1,&framebuffer); glapi::BindFramebuffer(Framebuffer,framebuffer);
 glapi::FramebufferTexture2D(Framebuffer,DepthAttachment,GL_TEXTURE_2D,depthTexture,0);
 glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
 bool complete=glapi::CheckFramebufferStatus(Framebuffer)==0x8CD5;
 glapi::BindFramebuffer(Framebuffer,0); glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
 glBindTexture(GL_TEXTURE_2D,0);
 if (!complete) { std::fprintf(stderr,"[renderer] shadow framebuffer unavailable; unshadowed GLSL\n"); glapi::DeleteFramebuffers(1,&framebuffer); framebuffer=0; }
 std::printf("[renderer] GLSL initialized; shadow map %s\n",complete ? "2048 x 2048" : "disabled");
 return true;
}
void shutdown() {
 active=false; shadowReady=false;
 if(program) { glapi::UseProgram(0); glapi::DeleteProgram(program); program=0; }
 if(framebuffer) { glapi::DeleteFramebuffers(1,&framebuffer); framebuffer=0; }
 if(depthTexture) { glDeleteTextures(1,&depthTexture); depthTexture=0; }
}
void shadowPass(const Scene& scene, SceneKind kind, bool ground, bool enabled) {
 shadowReady=false;
 if (!enabled || !program || !framebuffer) return;
 // A stable world-space projection avoids camera-dependent shadow swimming.
 const float extent=kind==SceneKind::Forest ? 48.0f : std::max(4.0f,scene.radius()*2.0f);
 Vec3 light=sunDirection(kind)*(extent*2.0f);
 glPushAttrib(GL_ALL_ATTRIB_BITS);
 glapi::UseProgram(0); glapi::BindFramebuffer(Framebuffer,framebuffer);
 glViewport(0,0,ShadowSize,ShadowSize);
 glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
 glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
 glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glClear(GL_DEPTH_BUFFER_BIT);
 glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glDisable(GL_FOG); glDisable(GL_BLEND);
 glDisable(GL_CULL_FACE); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
 glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(2.0f,4.0f);
 glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
 glOrtho(-extent,extent,-extent,extent,0.1,extent*4.0f);
 float projection[16]; glGetFloatv(GL_PROJECTION_MATRIX,projection);
 glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
 gluLookAt(light.x,light.y,light.z,0,0,0,0,1,0);
 float view[16]; glGetFloatv(GL_MODELVIEW_MATRIX,view); mat4Multiply(lightVP,projection,view);
 for(const Object& o:scene.objects) if(visible(o,ground)) Scene::drawOne(o,false);
 glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
 glapi::BindFramebuffer(Framebuffer,0); glPopAttrib();
 shadowReady=true;
}
void beginView(const Camera&, SceneKind kind, bool fog, float density, const float* color, bool enabled, bool smooth) {
 active=enabled && available(); if(!active) return;
 glapi::UseProgram(program);
 glapi::UniformMatrix4fv(u.lightVP,1,GL_FALSE,lightVP);
 float view[16],projection[16]; glGetFloatv(GL_MODELVIEW_MATRIX,view); glGetFloatv(GL_PROJECTION_MATRIX,projection);
 Vec3 sun=mat4MulDir(view,sunDirection(kind)), sky=mat4MulDir(view,v3(0,1,0));
 glapi::Uniform3f(u.sunEye,sun.x,sun.y,sun.z); glapi::Uniform3f(u.skyEye,sky.x,sky.y,sky.z);
 glapi::Uniform1i(u.orthographic,std::fabs(projection[15]-1.0f)<0.001f);
 glapi::Uniform1i(u.fogEnabled,fog); glapi::Uniform1f(u.fogDensity,density);
 glapi::Uniform3f(u.fogColor,color[0],color[1],color[2]);
 glapi::Uniform1i(u.smoothNormals,smooth); glapi::Uniform1i(u.shadows,shadowReady);
 glapi::Uniform1i(u.diffuseMap,0); glapi::Uniform1i(u.shadowMap,1);
 glapi::ActiveTexture(0x84C1); glBindTexture(GL_TEXTURE_2D,depthTexture); glapi::ActiveTexture(0x84C0);
}
void draw(const Object& o, bool texture) {
 const bool helper=o.kind==PrimKind::Grid || o.kind==PrimKind::Axes;
 if(active && helper) glapi::UseProgram(0);
 if(active && !helper) {
  float translation[16],scale[16],model[16]; mat4FromTranslation(translation,o.pos[0],o.pos[1],o.pos[2]);
  mat4FromScale(scale,o.size,o.size,o.size); mat4Multiply(model,translation,o.xform); mat4Multiply(model,model,scale);
  glapi::UniformMatrix4fv(u.model,1,GL_FALSE,model);
  const MeshData* data=o.mesh ? &o.mesh->data() : nullptr;
  bool colors=o.kind==PrimKind::Cube || (data && data->hasColors);
  bool tex=texture && o.mesh && o.mesh->hasTexture() && data->hasUVs;
  glapi::Uniform1i(u.vertexColor,colors); glapi::Uniform1i(u.textured,tex);
  const float* albedo=data ? data->material.diffuse : o.color;
  glapi::Uniform3f(u.albedo,tex?1.0f:albedo[0],tex?1.0f:albedo[1],tex?1.0f:albedo[2]);
  const float* spec=data ? data->material.specular : nullptr;
  glapi::Uniform3f(u.specular,colors?0.025f:(spec?spec[0]:0.25f),colors?0.025f:(spec?spec[1]:0.25f),colors?0.025f:(spec?spec[2]:0.25f));
  glapi::Uniform1f(u.shininess,data?data->material.shininess:48.0f);
 }
 // Scope legacy material/enable changes to one object, including unlit helpers.
 glPushAttrib(GL_ENABLE_BIT|GL_LIGHTING_BIT|GL_CURRENT_BIT|GL_TEXTURE_BIT|GL_POLYGON_BIT);
 Scene::drawOne(o,texture);
 glPopAttrib();
 if(active && helper) glapi::UseProgram(program);
}
void endView() {
 if(!active) return;
 glapi::UseProgram(0); glapi::ActiveTexture(0x84C1); glBindTexture(GL_TEXTURE_2D,0); glapi::ActiveTexture(0x84C0);
 active=false;
}
}
```

### 5. `tests/obj_loader_test.cpp`

OBJ regression tests（31 行）

```cpp
#include "obj_loader.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
int main() {
 const auto file=std::filesystem::temp_directory_path()/"cg_obj_loader_regression.obj";
 int failures=0;
 auto test=[&](const char* name,const char* body,bool expected,bool checkNormal=false) {
  {std::ofstream out(file);out<<body;}
  MeshData data; std::string error;
  bool loaded=loadOBJ(file.string(),data,&error);
  bool pass=loaded==expected;
  if(pass && loaded && checkNormal) {
   pass=data.hasNormals;
   for(size_t i=0;i<data.normals.size();i+=3)
    pass=pass && std::fabs(data.normals[i])<1e-5f && std::fabs(data.normals[i+1])<1e-5f && std::fabs(data.normals[i+2]-1.0f)<1e-5f;
  }
  std::cout<<(pass?"PASS ":"FAIL ")<<name<<"\n"; failures+=!pass;
 };
 test("missing normals", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",true,true);
 test("whitespace / negative indices / comment", "  v 0 0 0\n\tv 1 0 0\nv 0 1 0\n f -3 -2 -1 # triangle\n",true,true);
 test("mixed authored and generated normals", "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2 3\n",true,true);
 test("invalid position index", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n",false);
 test("invalid normal index", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1//2 2 3\n",false);
 test("short face", "v 0 0 0\nv 1 0 0\nf 1 2\n",false);
 test("empty model", "# no geometry\n",false);
 test("quad triangulation", "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n",true,true);
 std::filesystem::remove(file);
 return failures?1:0;
}
```

### 6. `tools/render_smoke.py`

GPU rendering regression checks（36 行）

```python
"""Actual OpenGL integration checks. Run after building; requires a GPU context."""
from pathlib import Path
import subprocess, sys, struct, json
root = Path(__file__).resolve().parents[1]
exe = Path(sys.argv[1]).resolve() if len(sys.argv)>1 else root/'build/Release/viewer.exe'
out = root/'shots/refactor'; out.mkdir(parents=True,exist_ok=True)
cases = {
 'after': ['--no-hud'],
 'classic': ['--classic','--no-hud'],
 'no-shadows': ['--no-shadows','--no-hud'],
 'studio': ['--scene','studio','--no-hud'],
 'quad': ['--scene','studio','--quad'],
 'ortho': ['--scene','studio','--proj','ortho','--view','front'],
 'cavalier': ['--scene','studio','--proj','cav','--view','front'],
 'cabinet': ['--scene','studio','--proj','cab','--view','front'],
 'wire': ['--scene','studio','--render','wire'],
 'points': ['--scene','studio','--render','points'],
 'wide': ['--size','1600x300','--no-hud'],
 'textured': ['Models/Rabbit/10037_Rabbit_v1_L3.obj','--scene','studio','--no-hud'],
}
report=[]
for name,args in cases.items():
 shot=out/(name+'.bmp')
 result=subprocess.run([str(exe),'--shot',str(shot),*args],cwd=root,capture_output=True,timeout=60)
 log=(result.stdout+result.stderr).decode('utf-8',errors='replace')
 (out/(name+'.log')).write_text(log,encoding='utf-8')
 assert result.returncode==0,(name,result.returncode,log)
 assert 'GLSL initialized' in log,(name,log)
 data=shot.read_bytes(); w,h=struct.unpack_from('<ii',data,18)
 assert len(data)==struct.unpack_from('<I',data,2)[0] and len(set(data[54:]))>32,name
 subprocess.run([sys.executable,str(root/'tools/bmp2png.py'),str(shot)],check=True,capture_output=True)
 report.append({'case':name,'width':w,'height':h,'exit':result.returncode})
 print('PASS',name,f'{w}x{h}',flush=True)
assert (out/'after.bmp').read_bytes()!=(out/'no-shadows.bmp').read_bytes(),'shadows have no effect'
assert (out/'after.bmp').read_bytes()!=(out/'classic.bmp').read_bytes(),'shader has no effect'
(out/'smoke-results.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
```

### 7. `CMakeLists.txt`

构建配置（含 32 位防护）（112 行）

```cmake
cmake_minimum_required(VERSION 3.21)
project(CGExperiment_3DViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ---------------------------------------------------------------------------
# Vendored freeglut (offline build), 32 位和 64 位都提供。
#
#   third_party/freeglut/include/GL/        头文件（架构无关）
#   third_party/freeglut/lib/Win32|x64/     导入库
#   third_party/freeglut/bin/Win32|x64/     DLL
#
# Deliberately NO find_package(GLUT) and NO CMAKE_PREFIX_PATH: FindGLUT on
# Windows searches PATH/registry and would silently latch onto whatever copy
# happens to be installed (this machine has an unrelated one in Anaconda),
# making the build machine-specific and defeating the point of vendoring.
# ---------------------------------------------------------------------------
set(FREEGLUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/freeglut")

# 依赖按平台分目录：lib/Win32、lib/x64，bin 同理。
# 用指针宽度判断而不是 CMAKE_GENERATOR_PLATFORM —— 后者只在 VS 生成器下有值，
# 换 NMake / Ninja 就是空的。
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(FREEGLUT_PLATFORM "x64")
else()
    set(FREEGLUT_PLATFORM "Win32")
endif()

set(FREEGLUT_LIB "${FREEGLUT_DIR}/lib/${FREEGLUT_PLATFORM}/freeglut.lib")
set(FREEGLUT_DLL "${FREEGLUT_DIR}/bin/${FREEGLUT_PLATFORM}/freeglut.dll")

if(NOT EXISTS "${FREEGLUT_LIB}")
    message(FATAL_ERROR
        "\n找不到 ${FREEGLUT_PLATFORM} 版的 freeglut.lib：\n  ${FREEGLUT_LIB}\n")
endif()

add_library(freeglut_vendored SHARED IMPORTED GLOBAL)
set_target_properties(freeglut_vendored PROPERTIES
    IMPORTED_IMPLIB               "${FREEGLUT_LIB}"
    IMPORTED_LOCATION             "${FREEGLUT_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${FREEGLUT_DIR}/include"
)

# ---------------------------------------------------------------------------
# Viewer
# ---------------------------------------------------------------------------
add_executable(viewer WIN32
    src/renderer.cpp
    src/main.cpp
    src/app.cpp
    src/input.cpp
    src/scene.cpp
    src/mesh.cpp
    src/obj_loader.cpp
    src/texture.cpp
    src/camera.cpp
    src/projection.cpp
    src/lighting.cpp
    src/hud.cpp
    src/anim.cpp
    src/forest.cpp
    src/recorder.cpp
)

set_target_properties(viewer PROPERTIES WIN32_EXECUTABLE TRUE)

target_include_directories(viewer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
)

# /utf-8 is mandatory on this zh-CN Windows: MSVC would otherwise assume the GBK
# codepage for source files, mangling the Chinese comments.
# /wd4819 suppresses the "character cannot be represented in the current code
#   page" nag -- a false positive once /utf-8 has fixed the source encoding.
# _CRT_SECURE_NO_WARNINGS silences the fopen/snprintf deprecation noise.
# /MP builds the translation units in parallel.
target_compile_options(viewer PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<CXX_COMPILER_ID:MSVC>:/W3>
    $<$<CXX_COMPILER_ID:MSVC>:/wd4819>
    $<$<CXX_COMPILER_ID:MSVC>:/MP>
)
target_compile_definitions(viewer PRIVATE _CRT_SECURE_NO_WARNINGS)

# freeglut.lib provides no WinMain.  With /SUBSYSTEM:WINDOWS the linker's default
# entry (WinMainCRTStartup) would reference WinMain and fail with LNK2019.
target_link_options(viewer PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/ENTRY:mainCRTStartup>)

# glu32 is needed for gluLookAt / gluOrtho2D / gluBuild2DMipmaps.
target_link_libraries(viewer PRIVATE freeglut_vendored opengl32 glu32)

# Place freeglut.dll beside the exe (multi-config aware under the VS generator).
add_custom_command(TARGET viewer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${FREEGLUT_DLL}"
            "$<TARGET_FILE_DIR:viewer>"
    VERBATIM
)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME math_camera COMMAND viewer --selftest)
    add_executable(obj_loader_test tests/obj_loader_test.cpp src/obj_loader.cpp)
    target_include_directories(obj_loader_test PRIVATE src "${FREEGLUT_DIR}/include")
    target_compile_options(obj_loader_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_compile_definitions(obj_loader_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(obj_loader_test PRIVATE freeglut_vendored opengl32 glu32)
    add_test(NAME obj_loader COMMAND obj_loader_test)
endif()
```

### 8. `CMakePresets.json`

CMake 预设：只提供 x64，避免选错（40 行）

```json
{
  "version": 3,
  "cmakeMinimumRequired": { "major": 3, "minor": 21, "patch": 0 },
  "configurePresets": [
    {
      "name": "x64-release",
      "displayName": "x64 Release (64 位)",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build"
    },
    {
      "name": "x64-debug",
      "displayName": "x64 Debug (64 位)",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build"
    },
    {
      "name": "win32-release",
      "displayName": "Win32 Release (32 位)",
      "generator": "Visual Studio 17 2022",
      "architecture": "Win32",
      "binaryDir": "${sourceDir}/build32"
    },
    {
      "name": "win32-debug",
      "displayName": "Win32 Debug (32 位)",
      "generator": "Visual Studio 17 2022",
      "architecture": "Win32",
      "binaryDir": "${sourceDir}/build32"
    }
  ],
  "buildPresets": [
    { "name": "x64-release",   "configurePreset": "x64-release",   "configuration": "Release" },
    { "name": "x64-debug",     "configurePreset": "x64-debug",     "configuration": "Debug" },
    { "name": "win32-release", "configurePreset": "win32-release", "configuration": "Release" },
    { "name": "win32-debug",   "configurePreset": "win32-debug",   "configuration": "Debug" }
  ]
}
```

### 9. `build.bat`

一键构建（Windows cmd / PowerShell）（82 行）

```bat
@echo off
setlocal enabledelayedexpansion
rem ===========================================================================
rem  一键构建（Windows cmd / PowerShell）：双击本文件，或在本目录执行
rem
rem      build.bat            默认编译 64 位
rem      build.bat x64        64 位
rem      build.bat Win32      32 位（指导书默认的「Debug Win32」就是这个）
rem
rem  CMake 不在 PATH 里也没关系 —— 脚本会去找 Visual Studio 2022 自带的那份。
rem
rem  注意：这里所有 echo 输出都用 ASCII。本文件是 UTF-8，而中文 Windows 的
rem  cmd 默认代码页是 GBK，UTF-8 的中文在 GBK 控制台里会变成乱码（而且是位置
rem  相关的：某个字节刚好落在 ASCII 边界上就崩）。注释里留中文不影响执行。
rem ===========================================================================
cd /d "%~dp0"

set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"

rem 每个架构用独立的构建目录：CMake 缓存和生成器平台绑死，
rem 同一个目录里先从 x64 再切 Win32 会报 generator platform does not match。
if /i "%ARCH%"=="Win32" (
    set "BUILDDIR=build32"
) else (
    set "BUILDDIR=build"
)

set "CMAKE="

rem 1) Prefer whatever is on PATH
where cmake >nul 2>nul
if not errorlevel 1 set "CMAKE=cmake"

rem 2) Otherwise look for the copy bundled with Visual Studio 2022
if not defined CMAKE (
    for %%E in (Community Professional Enterprise Preview BuildTools) do (
        if not defined CMAKE (
            set "CAND=C:\Program Files\Microsoft Visual Studio\2022\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if exist "!CAND!" set "CMAKE=!CAND!"
        )
    )
)

if not defined CMAKE (
    echo.
    echo [ERROR] cmake.exe not found.
    echo.
    echo   Install Visual Studio 2022 with the "Desktop development with C++"
    echo   workload, or put cmake on your PATH and retry.
    echo.
    exit /b 1
)

echo Using CMake: %CMAKE%
echo Architecture: %ARCH%   ^(build dir: %BUILDDIR%^)
echo.

"%CMAKE%" -S . -B "%BUILDDIR%" -G "Visual Studio 17 2022" -A %ARCH%
if errorlevel 1 (
    echo.
    echo [ERROR] configure failed.
    echo   If you configured this directory with a different generator or
    echo   architecture before, delete %BUILDDIR%\CMakeCache.txt and retry.
    exit /b 1
)

"%CMAKE%" --build "%BUILDDIR%" --config Release
if errorlevel 1 (
    echo.
    echo [ERROR] build failed.
    exit /b 1
)

echo.
echo ============================================================
echo  Build complete  (%ARCH%)
echo    executable: %BUILDDIR%\Release\viewer.exe
echo    run:        %BUILDDIR%\Release\viewer.exe
echo    forest:     %BUILDDIR%\Release\viewer.exe Models\bunny_10k.obj --scene forest --anim roam
echo ============================================================
endlocal
```

### 10. `build.sh`

一键构建（git-bash / MSYS2）（78 行）

```bash
#!/usr/bin/env bash
# 一键构建（git-bash / MSYS2 / WSL 里跑）：
#
#   bash build.sh          默认 64 位
#   bash build.sh x64      64 位
#   bash build.sh Win32    32 位
#
# CMake 不在 PATH 里也没关系 —— 脚本会去找 Visual Studio 2022 自带的那份。
#
# 输出统一用 ASCII：和 build.bat 保持一致，也避开各种控制台代码页的坑。
# 中文说明都在 README 和源码注释里。
set -e
cd "$(dirname "$0")"

ARCH="${1:-x64}"

# 每个架构用独立的构建目录：CMake 缓存和生成器平台绑死，
# 同一个目录里从 x64 切 Win32 会报 generator platform does not match。
if [ "$ARCH" = "Win32" ] || [ "$ARCH" = "win32" ]; then
    ARCH="Win32"
    BUILDDIR="build32"
else
    ARCH="x64"
    BUILDDIR="build"
fi

CMAKE=""

# 1) 先用 PATH 里的
if command -v cmake >/dev/null 2>&1; then
    CMAKE="$(command -v cmake)"
fi

# 2) 再找 VS2022 自带的（Community / Professional / Enterprise / BuildTools）
if [ -z "$CMAKE" ]; then
    for ed in Community Professional Enterprise Preview BuildTools; do
        cand="/c/Program Files/Microsoft Visual Studio/2022/$ed/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
        if [ -x "$cand" ]; then CMAKE="$cand"; break; fi
    done
fi

if [ -z "$CMAKE" ]; then
    echo
    echo "[ERROR] cmake not found."
    echo
    echo "  Install Visual Studio 2022 with the \"Desktop development with C++\""
    echo "  workload, or put cmake on your PATH and retry."
    echo
    exit 1
fi

# 输出统一用 ASCII：和 build.bat 保持一致，也避开各种控制台代码页的坑。
# 中文说明都在 README 和源码注释里。
echo "Using CMake:  $CMAKE"
echo "Architecture: $ARCH  (build dir: $BUILDDIR)"
echo

if ! "$CMAKE" -S . -B "$BUILDDIR" -G "Visual Studio 17 2022" -A "$ARCH"; then
    echo
    echo "[ERROR] configure failed."
    echo "  If you configured this directory with a different generator or"
    echo "  architecture before, delete $BUILDDIR/CMakeCache.txt and retry."
    exit 1
fi

if ! "$CMAKE" --build "$BUILDDIR" --config Release; then
    echo
    echo "[ERROR] build failed."
    exit 1
fi

echo
echo "============================================================"
echo " Build complete  ($ARCH)"
echo "   executable: $BUILDDIR/Release/viewer.exe"
echo "   run:        ./$BUILDDIR/Release/viewer.exe"
echo "   forest:     ./$BUILDDIR/Release/viewer.exe Models/bunny_10k.obj --scene forest --anim roam"
echo "============================================================"
```

### 11. `viewer.vcxproj`

经典 VS 工程（自包含，双击 viewer.sln 即可）（176 行）

```xml
<?xml version="1.0" encoding="utf-8"?>
<!--
  三维模型查看器 —— 经典 VS 工程（自包含，不依赖 CMake）

  双击同目录的 viewer.sln 打开，选好配置，F5 运行。

  提供 4 个配置：Debug/Release × Win32/x64。两个平台都能编 ——
  third_party/freeglut 下按平台分好了目录（lib/Win32、lib/x64、bin/Win32、bin/x64），
  所以直接用 $(Platform) 拼路径就行：VS 的平台名正好就是 "Win32" 和 "x64"。

  全部路径都用 $(ProjectDir) 相对定位，整个文件夹可以搬到任何位置、任何机器。
-->
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|Win32">
      <Configuration>Debug</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32">
      <Configuration>Release</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>

  <PropertyGroup Label="Globals">
    <VCProjectVersion>17.0</VCProjectVersion>
    <Keyword>Win32Proj</Keyword>
    <ProjectGuid>{3F8E2A91-6C4D-4E7B-9A15-8D2F5B7C1E30}</ProjectGuid>
    <RootNamespace>viewer</RootNamespace>
    <!-- 10.0 = 用已安装的最新 Windows SDK -->
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />

  <PropertyGroup Condition="'$(Configuration)'=='Debug'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>MultiByte</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)'=='Release'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>MultiByte</CharacterSet>
  </PropertyGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings" />
  <ImportGroup Label="Shared" />
  <ImportGroup Label="PropertySheets">
    <Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props"
            Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')"
            Label="LocalAppDataPlatform" />
  </ImportGroup>

  <PropertyGroup Label="UserMacros" />

  <!--
    显式指定输出目录。都用默认值的话两个平台会落到不同形状：
      Win32 -> Release\viewer.exe          （Win32 是默认平台，不带平台子目录）
      x64   -> x64\Release\viewer.exe
    这种历史遗留的不对称很难看也容易搞混，统一成 bin\平台\配置\。
  -->
  <PropertyGroup>
    <OutDir>$(SolutionDir)bin\$(Platform)\$(Configuration)\</OutDir>
    <IntDir>$(SolutionDir)obj\$(Platform)\$(Configuration)\</IntDir>
  </PropertyGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <SDLCheck>true</SDLCheck>
      <ConformanceMode>true</ConformanceMode>
      <LanguageStandard>stdcpp17</LanguageStandard>
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
      <PreprocessorDefinitions>_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <AdditionalIncludeDirectories>$(ProjectDir)src;$(ProjectDir)third_party\freeglut\include;$(ProjectDir)third_party\stb;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <!--
        /utf-8        源码是 UTF-8，不指定的话 MSVC 会按系统 GBK 代码页解析，中文注释全乱
        /wd4819       抑制「当前代码页无法表示的字符」—— /utf-8 已定好编码，这是误报
      -->
      <AdditionalOptions>/utf-8 /wd4819 %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>
    <Link>
      <SubSystem>Windows</SubSystem>
      <!--
        freeglut.lib 不提供 WinMain。默认入口 WinMainCRTStartup 会引用 WinMain，
        在 /SUBSYSTEM:WINDOWS 下必然 LNK2019。把入口指到 CRT 给 int main() 用的
        mainCRTStartup 上。
      -->
      <EntryPointSymbol>mainCRTStartup</EntryPointSymbol>
      <!-- $(Platform) 展开成 Win32 或 x64，正好对上依赖目录名 -->
      <AdditionalDependencies>opengl32.lib;glu32.lib;$(ProjectDir)third_party\freeglut\lib\$(Platform)\freeglut.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
    <PostBuildEvent>
      <!-- 把对应架构的 freeglut.dll 拷到 exe 旁边，否则运行时找不到 DLL -->
      <Command>copy /Y "$(ProjectDir)third_party\freeglut\bin\$(Platform)\freeglut.dll" "$(OutDir)" &gt;nul</Command>
      <Message>copy freeglut.dll ($(Platform))</Message>
    </PostBuildEvent>
  </ItemDefinitionGroup>

  <ItemDefinitionGroup Condition="'$(Configuration)'=='Debug'">
    <ClCompile>
      <Optimization>Disabled</Optimization>
      <RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary>
    </ClCompile>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)'=='Release'">
    <ClCompile>
      <Optimization>MaxSpeed</Optimization>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>
    </ClCompile>
    <Link>
      <EnableCOMDATFolding>true</EnableCOMDATFolding>
      <OptimizeReferences>true</OptimizeReferences>
    </Link>
  </ItemDefinitionGroup>

  <ItemGroup>
    <ClCompile Include="src\renderer.cpp" />
    <ClCompile Include="src\main.cpp" />
    <ClCompile Include="src\app.cpp" />
    <ClCompile Include="src\input.cpp" />
    <ClCompile Include="src\scene.cpp" />
    <ClCompile Include="src\mesh.cpp" />
    <ClCompile Include="src\obj_loader.cpp" />
    <ClCompile Include="src\texture.cpp" />
    <ClCompile Include="src\camera.cpp" />
    <ClCompile Include="src\projection.cpp" />
    <ClCompile Include="src\lighting.cpp" />
    <ClCompile Include="src\forest.cpp" />
    <ClCompile Include="src\anim.cpp" />
    <ClCompile Include="src\recorder.cpp" />
    <ClCompile Include="src\hud.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="src\types.h" />
    <ClInclude Include="src\math3d.h" />
    <ClInclude Include="src\obj_loader.h" />
    <ClInclude Include="src\mesh.h" />
    <ClInclude Include="src\texture.h" />
    <ClInclude Include="src\camera.h" />
    <ClInclude Include="src\projection.h" />
    <ClInclude Include="src\lighting.h" />
    <ClInclude Include="src\forest.h" />
    <ClInclude Include="src\scene.h" />
    <ClInclude Include="src\anim.h" />
    <ClInclude Include="src\recorder.h" />
    <ClInclude Include="src\hud.h" />
    <ClInclude Include="src\input.h" />
    <ClInclude Include="src\renderer.h" />
    <ClInclude Include="src\shaders.h" />
    <ClInclude Include="src\gl_api.h" />
    <ClInclude Include="src\app.h" />
  </ItemGroup>
  <ItemGroup>
    <None Include="README.md" />
  </ItemGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets" />
</Project>
```

### 12. `.gitignore`

Git 忽略规则（25 行）

```text
# 构建产物
build/          # CMake 构建目录（x64）
build32/        # CMake 构建目录（Win32）
bin/            # VS 工程 viewer.sln 的输出（bin/<平台>/<配置>/）
obj/            # VS 工程的中间文件
x64/            # 旧版 VS 默认输出，保留忽略以防手滑
Release/
Debug/
out/
.vs/

# 注意：不要忽略 *.sln / *.vcxproj —— viewer.sln / viewer.vcxproj 是本项目
# 手写并提交的经典 VS 工程，不是生成物。只有 .user / .suo 这类用户级状态才忽略。

# 用户级状态
*.user
*.suo
*.opendb
*.VC.db

# 截图：BMP 单张 3 MB，只保留 shots/ 下的 PNG
*.bmp

# 编辑器
.vscode/
```

### 13. `src/types.h`

全局模式枚举（27 行）

```cpp
#pragma once
// 全局模式枚举。单独放一个头，避免 app / input / projection 之间的循环包含。

// 投影模式（实验七 + 斜平行投影加分项）
enum class ProjMode
{
    Perspective,    // 透视投影
    Ortho,          // 正平行投影（正投影）
    Cavalier,       // 斜平行投影 - 斜等测（L = 1.0）
    Cabinet         // 斜平行投影 - 斜二测（L = 0.5）
};

// 标准视图预设（正平行投影三视图 + 前后上下）
enum class ViewPreset { Free, Front, Back, Left, Right, Top, Bottom };

// 几何变换模式（实验六）
enum class TransMode { Translate, Rotate, Scale };

// 绘制模式
enum class RenderMode { Solid, Wireframe, Points };

// 场景类型
enum class SceneKind
{
    Studio,   // 展示台：实验四立方体 + 茶壶 + 球 + 地面 + 网格 + 坐标轴
    Forest    // 森林：程序化树木 + 起伏草地 + 雾，兔子在里面漫步
};
```

### 14. `src/math3d.h`

向量 / 矩阵 / 四元数工具（header-only）（136 行）

```cpp
#pragma once
// 三维数学小工具（header-only）。
// 矩阵一律使用 OpenGL 的列主序 GLfloat[16]：m[col*4 + row]。
#include <cmath>

struct Vec3
{
    float x, y, z;
    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float ax, float ay, float az) : x(ax), y(ay), z(az) {}
};

inline Vec3 v3(float x, float y, float z) { return Vec3{ x, y, z }; }
inline Vec3 operator+(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 operator-(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 operator*(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }

inline float v3dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float v3len(Vec3 a) { return std::sqrt(v3dot(a, a)); }

inline Vec3 v3cross(Vec3 a, Vec3 b)
{
    return v3(a.y * b.z - a.z * b.y,
              a.z * b.x - a.x * b.z,
              a.x * b.y - a.y * b.x);
}

inline Vec3 v3norm(Vec3 a)
{
    const float len = v3len(a);
    if (len < 1e-8f) return v3(0.0f, 0.0f, 0.0f);
    return a * (1.0f / len);
}

// ---------------------------------------------------------------------------
// 四元数旋转（实验七）
// 把点 (px,py,pz) 绕过原点的单位轴 (ux,uy,uz) 旋转 theta 弧度，结果直接写回引用。
// 与指导书 quatRotate 的引用传参写法等价，这里用显式的临时副本保证旧坐标参与全部计算。
// ---------------------------------------------------------------------------
inline void quatRotate(float theta, float ux, float uy, float uz,
                       float& px, float& py, float& pz)
{
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    const float t = 1.0f - c;

    const float inx = px, iny = py, inz = pz;

    px = (ux * ux * t + c)       * inx + (ux * uy * t - uz * s) * iny + (ux * uz * t + uy * s) * inz;
    py = (uy * ux * t + uz * s)  * inx + (uy * uy * t + c)      * iny + (uy * uz * t - ux * s) * inz;
    pz = (uz * ux * t - uy * s)  * inx + (uz * uy * t + ux * s) * iny + (uz * uz * t + c)      * inz;
}

// ---------------------------------------------------------------------------
// 4x4 矩阵（列主序）
// ---------------------------------------------------------------------------
inline void mat4Identity(float m[16])
{
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// out = a * b   （列主序：m[col*4+row]）
inline void mat4Multiply(float out[16], const float a[16], const float b[16])
{
    float tmp[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
                sum += a[k * 4 + row] * b[col * 4 + k];
            tmp[col * 4 + row] = sum;
        }
    }
    for (int i = 0; i < 16; ++i) out[i] = tmp[i];
}

// 变换一个点（含平移分量）
inline Vec3 mat4MulPoint(const float m[16], Vec3 p)
{
    return v3(m[0] * p.x + m[4] * p.y + m[8]  * p.z + m[12],
              m[1] * p.x + m[5] * p.y + m[9]  * p.z + m[13],
              m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]);
}

// 变换一个方向向量（忽略平移分量）
inline Vec3 mat4MulDir(const float m[16], Vec3 p)
{
    return v3(m[0] * p.x + m[4] * p.y + m[8]  * p.z,
              m[1] * p.x + m[5] * p.y + m[9]  * p.z,
              m[2] * p.x + m[6] * p.y + m[10] * p.z);
}

// 绕任意轴 (ux,uy,uz) 旋转 angleDeg 度的矩阵（列主序）。
// 轴会被内部归一化；退化时返回单位阵。
inline void mat4FromAxisAngle(float m[16], float angleDeg, float ux, float uy, float uz)
{
    const float len = std::sqrt(ux * ux + uy * uy + uz * uz);
    if (len < 1e-8f) { mat4Identity(m); return; }
    ux /= len; uy /= len; uz /= len;

    const float r = angleDeg * 3.14159265358979323846f / 180.0f;
    const float c = std::cos(r), s = std::sin(r), t = 1.0f - c;

    m[0]  = t * ux * ux + c;        // r0c0
    m[1]  = t * ux * uy + s * uz;   // r1c0
    m[2]  = t * ux * uz - s * uy;   // r2c0
    m[3]  = 0.0f;

    m[4]  = t * ux * uy - s * uz;   // r0c1
    m[5]  = t * uy * uy + c;        // r1c1
    m[6]  = t * uy * uz + s * ux;   // r2c1
    m[7]  = 0.0f;

    m[8]  = t * ux * uz + s * uy;   // r0c2
    m[9]  = t * uy * uz - s * ux;   // r1c2
    m[10] = t * uz * uz + c;        // r2c2
    m[11] = 0.0f;

    m[12] = m[13] = m[14] = 0.0f;
    m[15] = 1.0f;
}

// 平移矩阵
inline void mat4FromTranslation(float m[16], float tx, float ty, float tz)
{
    mat4Identity(m);
    m[12] = tx; m[13] = ty; m[14] = tz;
}

// 缩放矩阵
inline void mat4FromScale(float m[16], float sx, float sy, float sz)
{
    mat4Identity(m);
    m[0] = sx; m[5] = sy; m[10] = sz;
}
```

### 15. `src/obj_loader.h`

OBJ 导入接口（实验十）（62 行）

```cpp
#pragma once
// 实验十：OBJ / MTL 模型导入。
//
// 与指导书示例的差别（都是为了让兔子这种真实模型能跑起来）：
//   1. 指导书只支持三角形面，本模型 38352 个面**全部是四边形**，必须扇形三角化；
//   2. 指导书逐三角形 glBegin/glEnd 绘制，7.6 万面每帧约 46 万次立即模式调用，
//      这里改成去重后的统一顶点缓冲 + glDrawElements，一次调用画完；
//   3. 指导书用平滑法线生成（带 90 度夹角阈值），本模型自带 38353 个 vn，直接用。
#include <string>
#include <vector>
// 用 <GL/freeglut.h>（而不是裸 <GL/gl.h>）：它会先定义 WIN32_LEAN_AND_MEAN /
// NOMINMAX 再引入 <windows.h>，从而给 <GL/gl.h> 提供 WINGDIAPI 和 APIENTRY。
// 直接包含 <GL/gl.h> 会因缺少这两个宏而报 C2182/C2086。指导书的 OBJImporter.h
// 也是以 <GL/glut.h> 开头，思路一致。
#include <GL/freeglut.h>

// .mtl 里的一个材质
struct ObjMaterial
{
    std::string name;
    float ambient[4]  = { 0.2f, 0.2f, 0.2f, 1.0f };
    float diffuse[4]  = { 0.8f, 0.8f, 0.8f, 1.0f };
    float specular[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float emission[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float shininess   = 65.0f;          // 已从 [0,1000] 映射到 OpenGL 的 [0,128]
    std::string diffuseMap;             // map_Kd，相对 .mtl 所在目录
};

// 去重后的渲染数据，可直接交给 glDrawElements
struct MeshData
{
    std::vector<GLfloat> positions;     // 3 floats / 顶点
    std::vector<GLfloat> normals;       // 3
    std::vector<GLfloat> uvs;           // 2
    std::vector<GLfloat> colors;        // 3，可选：程序化生成的网格用（OBJ 不会填）
    std::vector<GLuint>  indices;       // 3 / 三角形
    bool hasNormals = false;
    bool hasUVs     = false;
    bool hasColors  = false;

    ObjMaterial material;
    std::string diffuseMapPath;         // 拼好的贴图路径（可能为空）

    // 归一化用
    float bboxMin[3] = { 0, 0, 0 };
    float bboxMax[3] = { 0, 0, 0 };
    float center[3]  = { 0, 0, 0 };
    float radius     = 1.0f;            // 包围球半径（相对外接中心）

    // 统计信息，用于自检与实验报告
    int srcVertexCount   = 0;
    int srcFaceCount     = 0;
    int uniqueVertexCount = 0;
    int triangleCount    = 0;
    int quadFaceCount    = 0;
    int otherFaceCount   = 0;           // 非三角形/四边形的面

    bool valid = false;
};

// 读取 .obj（并按其 mtllib 读取 .mtl）。失败时返回 false，原因写入 errOut。
bool loadOBJ(const std::string& path, MeshData& out, std::string* errOut = nullptr);
```

### 16. `src/obj_loader.cpp`

OBJ / MTL 解析：四边形三角化 + 顶点去重（408 行）

```cpp
#include "obj_loader.h"
#include "math3d.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

// ---------------------------------------------------------------------------
// 路径小工具
// ---------------------------------------------------------------------------
std::string dirOf(const std::string& path)
{
    const size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? std::string() : path.substr(0, pos + 1);
}

std::string joinPath(const std::string& dir, const std::string& name)
{
    if (name.empty()) return std::string();
    // 绝对路径（含盘符或前导斜杠）直接返回
    if (name[0] == '/' || name[0] == '\\') return name;
    if (name.size() > 1 && name[1] == ':') return name;
    return dir + name;
}

// 去掉行尾的 '\r'（本模型的 OBJ 是 CRLF 行尾）
void chompCR(std::string& s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
}

// 把 OBJ 的索引转成 0 基下标。OBJ 允许负数，表示"从末尾往前数"。
inline int resolveIndex(int raw, int count)
{
    if (raw > 0)  return raw - 1;
    if (raw < 0)  return count + raw;
    return -1;      // 0 表示该项不存在
}

// ---------------------------------------------------------------------------
// 解析一个面的顶点项：a | a/b | a//c | a/b/c
// 返回 false 表示这一项格式不认识
// ---------------------------------------------------------------------------
struct CornerRef { int v = 0, vt = 0, vn = 0; };

bool parseCorner(const std::string& tok, CornerRef& out)
{
    // 拆出由 '/' 分隔的至多 3 段
    int  field[3] = { 0, 0, 0 };
    bool present[3] = { false, false, false };
    int  fi = 0;

    const char* p = tok.c_str();
    while (fi < 3)
    {
        // 跳过可能的空段（"a//c" 中间那段为空）
        if (*p == '/') { ++fi; ++p; continue; }

        char* end = nullptr;
        const long val = std::strtol(p, &end, 10);
        if (end == p) break;             // 没有数字了
        field[fi]   = (int)val;
        present[fi] = true;
        p = end;

        if (*p == '/') { ++fi; ++p; continue; }
        break;
    }

    if (!present[0]) return false;
    out.v  = field[0];
    out.vt = present[1] ? field[1] : 0;
    out.vn = present[2] ? field[2] : 0;
    return true;
}

// ---------------------------------------------------------------------------
// 解析 .mtl
// ---------------------------------------------------------------------------
bool parseMTL(const std::string& mtlPath, ObjMaterial& out, std::string& mapOut)
{
    std::ifstream ifs(mtlPath);
    if (!ifs.is_open()) return false;

    const std::string mtlDir = dirOf(mtlPath);

    std::string line;
    while (std::getline(ifs, line))
    {
        chompCR(line);
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        line.erase(0, first);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key.empty()) continue;

        if (key == "newmtl")
        {
            ls >> out.name;
        }
        else if (key == "Ka" || key == "Kd" || key == "Ks" || key == "Ke")
        {
            float r = 0, g = 0, b = 0;
            ls >> r >> g >> b;
            float* dst = (key == "Ka") ? out.ambient
                       : (key == "Kd") ? out.diffuse
                       : (key == "Ks") ? out.specular
                                       : out.emission;
            dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = 1.0f;
        }
        else if (key == "Ns")
        {
            float ns = 65.0f;
            ls >> ns;
            // wavefront 的 shininess 范围是 [0,1000]，换算到 OpenGL 的 [0,128]
            out.shininess = ns / 1000.0f * 128.0f;
            if (out.shininess < 0.0f)   out.shininess = 0.0f;
            if (out.shininess > 128.0f) out.shininess = 128.0f;
        }
        else if (key == "map_Kd")
        {
            std::string file;
            ls >> file;
            if (!file.empty()) mapOut = joinPath(mtlDir, file);
        }
    }
    return true;
}

// 去重用的键：OBJ 的 (v, vt, vn) 三元组
struct VertexKey
{
    int v = 0, vt = 0, vn = 0;
    bool operator==(const VertexKey& o) const { return v == o.v && vt == o.vt && vn == o.vn; }
};

struct VertexKeyHash
{
    size_t operator()(const VertexKey& k) const noexcept
    {
        // 三个 21 位字段打包（覆盖下标 < 2^21）
        unsigned long long x = (unsigned long long)(unsigned)k.v
                             | ((unsigned long long)(unsigned)k.vt << 21)
                             | ((unsigned long long)(unsigned)k.vn << 42);
        x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return (size_t)x;
    }
};

} // namespace

// ---------------------------------------------------------------------------
bool loadOBJ(const std::string& path, MeshData& out, std::string* errOut)
{
    auto fail = [&](const std::string& msg) {
        if (errOut) *errOut = msg;
        return false;
    };

    std::ifstream ifs(path);
    if (!ifs.is_open()) return fail("cannot open OBJ file: " + path);

    const std::string objDir = dirOf(path);

    // 原始数据（OBJ 的独立索引空间）
    struct RawVert { float x, y, z; };
    struct RawUV   { float u, v; };
    std::vector<RawVert> V;
    std::vector<RawUV>   T;
    std::vector<RawVert> N;

    // 去重后的统一顶点缓冲
    std::unordered_map<VertexKey, GLuint, VertexKeyHash> unique;
    unique.reserve(160000);

    std::string curMaterial;
    std::string mtlFile;
    bool sawFace = false;
    std::vector<int> sourceVertices;
    std::vector<bool> missingNormals;

    std::string line;
    while (std::getline(ifs, line))
    {
        chompCR(line);
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        line.erase(0, first);
        if (line.empty() || line[0] == '#') continue;

        // 按首字符分派（与指导书 parseStream 的思路一致）
        switch (line[0])
        {
        case 'v':
            if (line.size() > 1 && line[1] == 't')          // vt u v
            {
                std::istringstream ls(line);
                std::string k; RawUV t{ 0, 0 };
                ls >> k >> t.u >> t.v;
                T.push_back(t);
            }
            else if (line.size() > 1 && line[1] == 'n')     // vn x y z
            {
                std::istringstream ls(line);
                std::string k; RawVert n{ 0, 0, 0 };
                ls >> k >> n.x >> n.y >> n.z;
                N.push_back(n);
            }
            else                                            // v x y z
            {
                std::istringstream ls(line);
                std::string k; RawVert p{ 0, 0, 0 };
                ls >> k >> p.x >> p.y >> p.z;
                V.push_back(p);
            }
            break;

        case 'f':
        {
            sawFace = true;
            std::istringstream ls(line);
            std::string k;
            ls >> k;

            // 收集所有角点
            std::vector<CornerRef> corners;
            std::string tok;
            while (ls >> tok)
            {
                CornerRef c;
                if (tok[0] == '#') break;
                if (!parseCorner(tok, c)) return fail("invalid face corner: " + tok);
                const bool hasUV = c.vt != 0, hasNormal = c.vn != 0;
                c.v  = resolveIndex(c.v,  (int)V.size());
                c.vt = resolveIndex(c.vt, (int)T.size());
                c.vn = resolveIndex(c.vn, (int)N.size());
                if (c.v < 0 || c.v >= (int)V.size() ||
                    (hasUV && (c.vt < 0 || c.vt >= (int)T.size())) ||
                    (hasNormal && (c.vn < 0 || c.vn >= (int)N.size())))
                    return fail("face index out of range: " + tok);
                corners.push_back(c);
            }
            if (corners.size() < 3) return fail("face has fewer than three corners");

            if (corners.size() == 3)      out.srcFaceCount += 1;
            else if (corners.size() == 4) { out.srcFaceCount += 1; out.quadFaceCount += 1; }
            else                          { out.srcFaceCount += 1; out.otherFaceCount += 1; }

            // 把每个角点映射到统一顶点缓冲里的下标
            std::vector<GLuint> ci(corners.size());
            for (size_t i = 0; i < corners.size(); ++i)
            {
                const CornerRef& c = corners[i];
                const VertexKey key{ c.v, c.vt, c.vn };

                auto it = unique.find(key);
                if (it == unique.end())
                {
                    const GLuint id = (GLuint)(out.positions.size() / 3);
                    sourceVertices.push_back(c.v);
                    missingNormals.push_back(c.vn < 0);

                    if (c.v >= 0 && c.v < (int)V.size())
                        out.positions.insert(out.positions.end(),
                            { V[c.v].x, V[c.v].y, V[c.v].z });
                    else
                        out.positions.insert(out.positions.end(), { 0.0f, 0.0f, 0.0f });

                    if (c.vn >= 0 && c.vn < (int)N.size())
                    {
                        out.normals.insert(out.normals.end(),
                            { N[c.vn].x, N[c.vn].y, N[c.vn].z });
                        out.hasNormals = true;
                    }
                    else
                    {
                        out.normals.insert(out.normals.end(), { 0.0f, 0.0f, 1.0f });
                    }

                    if (c.vt >= 0 && c.vt < (int)T.size())
                    {
                        out.uvs.insert(out.uvs.end(), { T[c.vt].u, T[c.vt].v });
                        out.hasUVs = true;
                    }
                    else
                    {
                        out.uvs.insert(out.uvs.end(), { 0.0f, 0.0f });
                    }

                    it = unique.emplace(key, id).first;
                }
                ci[i] = it->second;
            }

            // 扇形三角化 (0,1,2) (0,2,3) ... —— 保持 OBJ 的 CCW 绕序
            for (size_t i = 1; i + 1 < ci.size(); ++i)
            {
                out.indices.push_back(ci[0]);
                out.indices.push_back(ci[i]);
                out.indices.push_back(ci[i + 1]);
            }
            break;
        }

        case 'm':
            if (line.rfind("mtllib", 0) == 0)
            {
                std::istringstream ls(line);
                std::string k;
                ls >> k >> mtlFile;
            }
            break;

        case 'u':
            if (line.rfind("usemtl", 0) == 0)
            {
                std::istringstream ls(line);
                std::string k;
                ls >> k >> curMaterial;
                if (out.material.name.empty()) out.material.name = curMaterial;
            }
            break;

        default:
            break;      // o / g / s 等暂不处理
        }
    }

    if (!sawFace || out.indices.empty()) return fail("no face data in OBJ: " + path);
    // Area-weighted normals for missing vn entries; accumulate by source position
    // so a UV seam does not introduce a lighting seam. Authored normals survive.
    std::vector<Vec3> sums(V.size());
    for (size_t i=0; i<out.indices.size(); i+=3) {
        GLuint a=out.indices[i], b=out.indices[i+1], c=out.indices[i+2];
        auto point=[&](GLuint v) { return v3(out.positions[v*3],out.positions[v*3+1],out.positions[v*3+2]); };
        Vec3 n=v3cross(point(b)-point(a),point(c)-point(a));
        for (GLuint v : {a,b,c}) sums[sourceVertices[v]]=sums[sourceVertices[v]]+n;
    }
    for (size_t i=0;i<missingNormals.size();++i) if (missingNormals[i]) {
        Vec3 n=v3norm(sums[sourceVertices[i]]);
        if(v3len(n)<1e-8f) n=v3(0,1,0);
        out.normals[i*3]=n.x; out.normals[i*3+1]=n.y; out.normals[i*3+2]=n.z;
    }
    out.hasNormals=true;

    // ---- 材质与贴图 ----
    if (!mtlFile.empty())
    {
        ObjMaterial mtl;
        std::string mapPath;
        if (parseMTL(joinPath(objDir, mtlFile), mtl, mapPath))
        {
            out.material      = mtl;
            out.diffuseMapPath = mapPath;
        }
    }

    // ---- 包围盒 / 包围球（供归一化使用）----
    const size_t nv = out.positions.size() / 3;
    if (nv == 0) return fail("no vertices in OBJ: " + path);

    float mn[3] = { out.positions[0], out.positions[1], out.positions[2] };
    float mx[3] = { mn[0], mn[1], mn[2] };
    for (size_t i = 1; i < nv; ++i)
    {
        for (int a = 0; a < 3; ++a)
        {
            const float v = out.positions[i * 3 + a];
            if (v < mn[a]) mn[a] = v;
            if (v > mx[a]) mx[a] = v;
        }
    }
    for (int a = 0; a < 3; ++a)
    {
        out.bboxMin[a] = mn[a];
        out.bboxMax[a] = mx[a];
        out.center[a]  = (mn[a] + mx[a]) * 0.5f;
    }

    float maxDistSq = 0.0f;
    for (size_t i = 0; i < nv; ++i)
    {
        const float dx = out.positions[i * 3 + 0] - out.center[0];
        const float dy = out.positions[i * 3 + 1] - out.center[1];
        const float dz = out.positions[i * 3 + 2] - out.center[2];
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > maxDistSq) maxDistSq = d2;
    }
    out.radius = std::sqrt(maxDistSq);
    if (out.radius < 1e-6f) out.radius = 1.0f;

    out.uniqueVertexCount = (int)nv;
    out.triangleCount     = (int)(out.indices.size() / 3);
    out.srcVertexCount    = (int)V.size();
    out.valid             = true;
    return true;
}
```

### 17. `src/mesh.h`

网格接口（50 行）

```cpp
#pragma once
// 网格：持有 OBJ 解析结果 + 纹理，并负责绘制。
// 绘制用「客户端顶点数组 + 一次 glDrawElements」，替代指导书里逐三角形的
// glBegin/glEnd —— 对 76704 个三角形这是 1 次 GL 调用 vs 约 46 万次。
#include "obj_loader.h"
#include <GL/freeglut.h>
#include <string>

// 模型的「上」是哪根轴。Auto 会按几何自动判断，判断不出来的场合可以显式指定。
//   - 原来那只兔子（3ds Max 导出）是 Z-up
//   - Stanford Bunny 是 Y-up，而且底面就在 y = 0
enum class UpAxis { Auto, Y, Z };

class Mesh
{
public:
    Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    // 读取 OBJ。up != Y 时把模型旋转成 Y-up。
    // normalize=true 时把模型平移到原点并缩放到单位包围球，这样取景和相机距离
    // 就与模型原始单位无关（老兔子原始半径 25.42、高 40 单位；新兔子只有 ~1 单位高）。
    bool loadFromOBJ(const std::string& path, UpAxis up, bool normalize,
                     std::string* errOut = nullptr);

    // 程序化构造（森林等）：直接塞顶点数据进去，不经过文件
    void adopt(MeshData&& data);

    // 用当前材质设定 + 纹理绘制。useTexture 关掉时退化为纯材质着色。
    void draw(bool useTexture) const;
    // 只发 draw call，不碰材质/纹理（由 glPolygonMode 决定填充方式）
    void drawRaw(bool useTexture) const;

    void applyMaterial() const;         // 把 .mtl 材质喂给固定管线
    void bindTexture(bool useTexture) const;

    void release();                  // 删除纹理、释放内存

    const MeshData& data() const { return data_; }
    bool  hasTexture() const { return tex_ != 0; }
    float radius()     const { return data_.radius; }
    const float* center() const { return data_.center; }

private:
    void upload() const;
    mutable GLuint buffers_[5] = {}; // position, normal, UV, colour, indices
    MeshData data_;
    GLuint   tex_ = 0;
    int      texW_ = 0, texH_ = 0;
};
```

### 18. `src/mesh.cpp`

网格：归一化、上轴判别、客户端顶点数组绘制（272 行）

```cpp
#include "mesh.h"
#include "texture.h"
#include "gl_api.h"

#include <cmath>
#include <cstdio>

namespace {

// Z-up → Y-up：绕 X 轴 -90°，(x,y,z) → (x, z, -y)。
// 本兔子模型是 Z-up 且高 40 单位，不转换的话「正视图」看到的是脚底板。
void rotateZUpToYUp(MeshData& m)
{
    const size_t nv = m.positions.size() / 3;
    for (size_t i = 0; i < nv; ++i)
    {
        float& y = m.positions[i * 3 + 1];
        float& z = m.positions[i * 3 + 2];
        const float ny = z, nz = -y;
        y = ny; z = nz;
    }
    for (size_t i = 0; i < m.normals.size() / 3; ++i)
    {
        float& y = m.normals[i * 3 + 1];
        float& z = m.normals[i * 3 + 2];
        const float ny = z, nz = -y;
        y = ny; z = nz;
    }
}

// 旋转后重新计算包围盒 / 包围球（三视图共用缩放要用到）
void recomputeBounds(MeshData& m)
{
    const size_t nv = m.positions.size() / 3;
    if (nv == 0) return;

    float mn[3] = { m.positions[0], m.positions[1], m.positions[2] };
    float mx[3] = { mn[0], mn[1], mn[2] };
    for (size_t i = 1; i < nv; ++i)
    {
        for (int a = 0; a < 3; ++a)
        {
            const float v = m.positions[i * 3 + a];
            if (v < mn[a]) mn[a] = v;
            if (v > mx[a]) mx[a] = v;
        }
    }
    for (int a = 0; a < 3; ++a)
    {
        m.bboxMin[a] = mn[a];
        m.bboxMax[a] = mx[a];
        m.center[a]  = (mn[a] + mx[a]) * 0.5f;
    }

    float maxDistSq = 0.0f;
    for (size_t i = 0; i < nv; ++i)
    {
        const float dx = m.positions[i * 3 + 0] - m.center[0];
        const float dy = m.positions[i * 3 + 1] - m.center[1];
        const float dz = m.positions[i * 3 + 2] - m.center[2];
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > maxDistSq) maxDistSq = d2;
    }
    m.radius = std::sqrt(maxDistSq);
    if (m.radius < 1e-6f) m.radius = 1.0f;
}

// 平移到原点 + 缩放到单位包围球
void normalizeToUnitSphere(MeshData& m)
{
    if (m.radius < 1e-8f) return;
    const float s = 1.0f / m.radius;
    const size_t nv = m.positions.size() / 3;
    for (size_t i = 0; i < nv; ++i)
    {
        m.positions[i * 3 + 0] = (m.positions[i * 3 + 0] - m.center[0]) * s;
        m.positions[i * 3 + 1] = (m.positions[i * 3 + 1] - m.center[1]) * s;
        m.positions[i * 3 + 2] = (m.positions[i * 3 + 2] - m.center[2]) * s;
    }
    recomputeBounds(m);      // 归一化后 center≈0、radius≈1
}

// 判断模型的「上」是哪根轴：取「底面贴着的那根轴」，也就是 min 值相对自身跨度
// 最小的那根。归一化之后这个比例能很好地区分两种常见朝向：
//   老兔子   z[0.02, 40.21]  -> ratio 0.0005  -> Z-up
//   新兔子   y[0.00,  1.03]  -> ratio 0.0000  -> Y-up
// 只用 Y / Z 两种结果：实际模型基本只有这两种朝向，X-up 的按 Z-up 处理。
UpAxis detectUpAxis(const MeshData& d)
{
    float best = 1e30f;
    int   bestAxis = 2;
    for (int a = 0; a < 3; ++a)
    {
        const float ext = d.bboxMax[a] - d.bboxMin[a];
        if (ext < 1e-9f) continue;
        const float ratio = std::fabs(d.bboxMin[a]) / ext;
        if (ratio < best) { best = ratio; bestAxis = a; }
    }
    return (bestAxis == 1) ? UpAxis::Y : UpAxis::Z;
}

} // namespace

void Mesh::adopt(MeshData&& data)
{
    release();
    data_ = std::move(data);
    recomputeBounds(data_);
    // 这两个统计量平时由 loadOBJ 填，程序化生成的网格要自己补上，
    // 否则 HUD 和日志里的三角形数会显示成 0
    data_.uniqueVertexCount = (int)(data_.positions.size() / 3);
    data_.triangleCount     = (int)(data_.indices.size() / 3);
    data_.valid             = true;
}

bool Mesh::loadFromOBJ(const std::string& path, UpAxis up, bool normalize, std::string* errOut)
{
    MeshData loaded;
    if (!loadOBJ(path, loaded, errOut)) return false;
    release();
    data_ = std::move(loaded);

    if (up == UpAxis::Auto) up = detectUpAxis(data_);
    if (up == UpAxis::Z) rotateZUpToYUp(data_);
    recomputeBounds(data_);
    if (normalize) normalizeToUnitSphere(data_);

    if (!data_.diffuseMapPath.empty())
    {
        tex_ = loadTexture2D(data_.diffuseMapPath, &texW_, &texH_);
        if (tex_ == 0)
            std::fprintf(stderr, "[mesh] texture load failed; material colour only\n");
    }

    std::printf("[mesh] %s  (up axis = %s)\n", path.c_str(),
                up == UpAxis::Y ? "Y (no rotation)" : "Z -> rotated to Y");
    std::printf("       vertices %d -> unique %d | faces %d (quads %d) -> triangles %d\n",
                data_.srcVertexCount, data_.uniqueVertexCount,
                data_.srcFaceCount, data_.quadFaceCount, data_.triangleCount);
    std::printf("       bbox x[%.2f,%.2f] y[%.2f,%.2f] z[%.2f,%.2f]  radius %.2f\n",
                data_.bboxMin[0], data_.bboxMax[0],
                data_.bboxMin[1], data_.bboxMax[1],
                data_.bboxMin[2], data_.bboxMax[2], data_.radius);
    return true;
}

void Mesh::applyMaterial() const
{
    const ObjMaterial& mtl = data_.material;

    // 有贴图时把漫反射设为白色：GL_MODULATE 会把纹理乘以材质色，
    // 若沿用 .mtl 的 Kd=0.5882，兔子会整体发灰。
    float kd[4] = { mtl.diffuse[0], mtl.diffuse[1], mtl.diffuse[2], 1.0f };
    if (hasTexture()) kd[0] = kd[1] = kd[2] = 1.0f;

    // Ka 调小：0.5882 的环境光叠加 GL_LIGHT_MODEL_AMBIENT 会让模型发白变平。
    const float ka[4] = { mtl.ambient[0] * 0.35f, mtl.ambient[1] * 0.35f, mtl.ambient[2] * 0.35f, 1.0f };
    // Ke 置零：.mtl 里的 0.1765 自发光会让兔子显得发乳白。
    const float ke[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   ka);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   kd);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  mtl.specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION,  ke);
    glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, mtl.shininess);
}

void Mesh::bindTexture(bool useTexture) const
{
    if (tex_ != 0 && useTexture)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex_);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
    }
}

void Mesh::upload() const
{
    if (buffers_[0] || !glapi::GenBuffers || !glapi::BufferData || !glapi::BindBuffer) return;
    glapi::GenBuffers(5, buffers_);
    auto uploadArray = [&](int slot, const std::vector<GLfloat>& values) {
        glapi::BindBuffer(0x8892, buffers_[slot]);
        glapi::BufferData(0x8892, values.size()*sizeof(GLfloat), values.data(), 0x88E4);
    };
    uploadArray(0, data_.positions); uploadArray(1, data_.normals);
    uploadArray(2, data_.uvs); uploadArray(3, data_.colors);
    glapi::BindBuffer(0x8893, buffers_[4]);
    glapi::BufferData(0x8893, data_.indices.size()*sizeof(GLuint), data_.indices.data(), 0x88E4);
    glapi::BindBuffer(0x8892, 0); glapi::BindBuffer(0x8893, 0);
}

void Mesh::drawRaw(bool useTexture) const
{
    if (data_.indices.empty()) return;
    upload();
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    auto pointer = [&](int slot, const std::vector<GLfloat>& values) -> const void* {
        if (!buffers_[0]) return values.data();
        glapi::BindBuffer(0x8892, buffers_[slot]); return nullptr;
    };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, pointer(0, data_.positions));
    if (data_.hasNormals) {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, 0, pointer(1, data_.normals));
    } else { glDisableClientState(GL_NORMAL_ARRAY); glNormal3f(0,1,0); }
    const bool useTex = tex_ && useTexture && data_.hasUVs;
    if (useTex) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, pointer(2, data_.uvs));
    } else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    if (data_.hasColors) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(3, GL_FLOAT, 0, pointer(3, data_.colors));
    } else glDisableClientState(GL_COLOR_ARRAY);
    if (buffers_[0]) glapi::BindBuffer(0x8893, buffers_[4]);
    glDrawElements(GL_TRIANGLES, (GLsizei)data_.indices.size(), GL_UNSIGNED_INT,
                   buffers_[0] ? nullptr : data_.indices.data());
    if (buffers_[0]) { glapi::BindBuffer(0x8892,0); glapi::BindBuffer(0x8893,0); }
    glPopClientAttrib();
}

void Mesh::draw(bool useTexture) const
{
    if (data_.hasColors)
    {
        // 顶点色驱动材质。注意 GL_COLOR_MATERIAL 一开，glMaterialfv 的
        // AMBIENT/DIFFUSE 就被忽略，所以这里不调 applyMaterial，而是把材质设成
        // 中性偏白 —— 否则顶点色会和材质色相乘，整片森林变暗。
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        const float white[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };
        const float ambient[4] = { 0.45f, 0.45f, 0.45f, 1.0f };
        // 高光要压得很低：草地/树叶基本是漫反射材质，给 0.45 的镜面反射
        // 会在地面上烧出一块明显的光斑，非常假
        const float specular[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   white);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  specular);
        glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 8.0f);
    }
    else
    {
        // 用 .mtl 材质时必须关掉 GL_COLOR_MATERIAL，否则材质会被残留的"当前颜色"覆盖
        glDisable(GL_COLOR_MATERIAL);
        // 关掉光照时材质不起作用，模型会沿用一个残留的 glColor —— 显式设成浅灰，
        // 否则可能整只兔子被上一次 glColor 染成奇怪的颜色。
        if (!glIsEnabled(GL_LIGHTING))
            glColor3f(0.82f, 0.80f, 0.78f);
        applyMaterial();
    }

    bindTexture(useTexture);
    drawRaw(useTexture);
    glDisable(GL_TEXTURE_2D);
}

void Mesh::release()
{
    if (buffers_[0] && glapi::DeleteBuffers) glapi::DeleteBuffers(5, buffers_);
    for (auto& buffer : buffers_) buffer = 0;
    if (tex_ != 0)
    {
        glDeleteTextures(1, &tex_);
        tex_ = 0;
    }
    data_ = MeshData{};
}
```

### 19. `src/texture.h`

纹理接口（7 行）

```cpp
#pragma once
// 纹理加载：stb_image（public domain，单头文件，无需额外 DLL）→ OpenGL 纹理对象。
#include <GL/freeglut.h>
#include <string>

// 加载图片并上传为 2D 纹理。失败返回 0。宽高通过出参返回（可为 nullptr）。
GLuint loadTexture2D(const std::string& path, int* wOut = nullptr, int* hOut = nullptr);
```

### 20. `src/texture.cpp`

stb_image → OpenGL 纹理（63 行）

```cpp
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
```

### 21. `src/camera.h`

观察变换相机接口（38 行）

```cpp
#pragma once
// 观察变换相机（实验七）。
// eye/target/up 三个向量就是 gluLookAt 的参数；绕参考点的旋转用四元数 quatRotate，
// 与指导书里 rotateCamera 分支的算法一致。
#include "math3d.h"

struct Camera
{
    Vec3  eye    { 0.0f, 0.0f, 3.0f };
    Vec3  target { 0.0f, 0.0f, 0.0f };
    Vec3  up     { 0.0f, 1.0f, 0.0f };
    float fovY   = 45.0f;      // 透视投影的垂直视场角（度）

    Vec3  forward()  const { return v3norm(target - eye); }
    Vec3  right()    const { return v3norm(v3cross(forward(), up)); }
    float distance() const { return v3len(target - eye); }

    // 回到默认观察位置（相机放在 +Z 方向上，距离随场景半径自适应）
    void reset(float sceneRadius);

    // 绕参考点旋转：dPitchDeg 为俯仰、dYawDeg 为偏航（单位：度）
    void orbit(float dPitchDeg, float dYawDeg);

    // 相机平面内平移（world 单位）
    void pan(float dxWorld, float dyWorld);

    // 沿视线推拉；factor > 1 拉远，< 1 拉近
    void dolly(float factor);

    // 漫游：沿视线前后 / 沿右向量左右 / 沿世界 Y 升降，amount 单位为当前距离的比例
    void fly(float fwd, float strafe, float rise);

    // 把当前 eye/target/up 交给 gluLookAt，并处理顶视图的退化情况
    void applyLookAt() const;

    // 切到某个标准视图（正平行投影三视图）
    void setPreset(int presetIndex, float sceneRadius);
};
```

### 22. `src/camera.cpp`

相机：环绕 / 漫游 / 四元数旋转 / 退化保护（实验七）（123 行）

```cpp
#include "camera.h"
#include "types.h"

#include <GL/freeglut.h>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.0f;

// 视线方向与 up 反平行时 gluLookAt 的基会退化（叉积为零），且它不报错，
// 画面直接空白。这里检测到就换成备用 up。
constexpr float kDegenerateDot = 0.999f;
} // namespace

void Camera::reset(float sceneRadius)
{
    const float d = sceneRadius * 2.8f;
    eye    = v3(0.55f * d, 0.40f * d, 0.75f * d);
    target = v3(0.0f, 0.0f, 0.0f);
    up     = v3(0.0f, 1.0f, 0.0f);
    fovY   = 45.0f;
}

void Camera::orbit(float dPitchDeg, float dYawDeg)
{
    // 与指导书一致：绕「当前视线 × up」得到的局部右向量做俯仰，
    // 绕世界 Y 轴做偏航，只更新视点（参考点不动）。
    const Vec3 r = right();
    if (v3len(r) < 1e-6f) return;          // 已退化，本次忽略

    const float thetaX = -dPitchDeg * kDeg2Rad;
    const float thetaY = -dYawDeg   * kDeg2Rad;

    float px = target.x - eye.x;
    float py = target.y - eye.y;
    float pz = target.z - eye.z;
    quatRotate(thetaX, r.x, r.y, r.z, px, py, pz);
    quatRotate(thetaY, 0.0f, 1.0f, 0.0f, px, py, pz);
    target = v3(eye.x + px, eye.y + py, eye.z + pz);

    float ux = up.x, uy = up.y, uz = up.z;
    quatRotate(thetaX, r.x, r.y, r.z, ux, uy, uz);
    quatRotate(thetaY, 0.0f, 1.0f, 0.0f, ux, uy, uz);
    up = v3norm(v3(ux, uy, uz));
    if (v3len(up) < 1e-6f) up = v3(0.0f, 1.0f, 0.0f);
}

void Camera::pan(float dxWorld, float dyWorld)
{
    const Vec3 r = right();
    const Vec3 u = v3norm(v3cross(r, forward()));
    const Vec3 delta = r * dxWorld + u * dyWorld;
    eye    = eye    + delta;
    target = target + delta;
}

void Camera::dolly(float factor)
{
    if (factor <= 0.0f) return;
    const float dist = distance();
    const float nd   = dist * factor;
    if (nd < 1e-3f) return;
    const Vec3 f = forward();
    target = eye + f * nd;
}

void Camera::fly(float fwd, float strafe, float rise)
{
    const Vec3 f = forward();
    const Vec3 r = right();
    // 平移量与当前距离成比例，远处移动快、近处移动慢
    const float speed = distance() * 0.25f;
    const Vec3 delta = f * (fwd * speed) + r * (strafe * speed) + v3(0.0f, rise * speed, 0.0f);
    eye    = eye    + delta;
    target = target + delta;
}

void Camera::applyLookAt() const
{
    const Vec3 f = forward();
    if (v3len(target - eye) < 1e-6f) return;      // 视点与参考点重合，放弃本次

    const float d = std::fabs(v3dot(f, up));
    if (d > kDegenerateDot)
    {
        // 视线与 up 几乎平行（顶视图/仰视图）：换 (0,0,-1) 作为 up，
        // 得到标准俯视平面视图：屏幕右 = 世界 +X，屏幕上 = 世界 -Z。
        gluLookAt(eye.x, eye.y, eye.z, target.x, target.y, target.z, 0.0f, 0.0f, -1.0f);
        return;
    }
    gluLookAt(eye.x, eye.y, eye.z,
              target.x, target.y, target.z,
              up.x, up.y, up.z);
}

void Camera::setPreset(int presetIndex, float sceneRadius)
{
    const float D = sceneRadius * 2.8f;
    target = v3(0.0f, 0.0f, 0.0f);
    up     = v3(0.0f, 1.0f, 0.0f);

    switch (static_cast<ViewPreset>(presetIndex))
    {
    case ViewPreset::Front:  eye = v3(0.0f,  0.0f,  D); break;   // 正视图（看向 -Z）
    case ViewPreset::Back:   eye = v3(0.0f,  0.0f, -D); break;   // 后视图
    case ViewPreset::Left:   eye = v3(-D,    0.0f,  0.0f); break; // 左视图
    case ViewPreset::Right:  eye = v3( D,    0.0f,  0.0f); break; // 右视图
    case ViewPreset::Top:                                          // 顶视图
        eye = v3(0.0f,  D,     0.0f);
        // up 与视线反平行会退化，指导书这里也用 (0,0,-1)
        up  = v3(0.0f,  0.0f, -1.0f);
        break;
    case ViewPreset::Bottom:                                       // 仰视图
        eye = v3(0.0f, -D,     0.0f);
        up  = v3(0.0f,  0.0f,  1.0f);
        break;
    case ViewPreset::Free:
    default:
        reset(sceneRadius);
        break;
    }
}
```

### 23. `src/projection.h`

投影接口（21 行）

```cpp
#pragma once
// 投影变换与「窗口变化图形不变形」（实验七 + 要求 3）。
//
// 注意：不要照抄指导书的 glFrustum(-2.5, 2.5, -2.5, 2.5, near, far) ——
// 它在近平面上永远是 1:1，窗口一变宽图形就被压扁，正好踩中要求 3。
#include "types.h"

struct Camera;

// 在 GL_PROJECTION 上设置好投影矩阵。调用方需保证已 glMatrixMode(GL_PROJECTION)。
//   winW/winH   : 当前窗口像素尺寸
//   cam         : 观察相机（透视模式需要其距离与 FOV）
//   sceneRadius : 场景包围球半径，用于自适应取景
//   obliqueAngleDeg : 斜平行投影的斜边与水平方向夹角（屏幕上呈现的角度）
void applyProjection(ProjMode mode, int winW, int winH, const Camera& cam,
                     float sceneRadius, float obliqueAngleDeg = 45.0f);

// 该模式是否使用平行投影（正交 / 斜平行），供 HUD 与菜单显示用
bool isParallelProjection(ProjMode mode);

const char* projModeName(ProjMode mode);
```

### 24. `src/projection.cpp`

透视 / 正交 / 斜平行投影 + 保宽高比（171 行）

```cpp
#include "projection.h"
#include "camera.h"

#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.0f;

// 取景余量：包围球半径乘这个系数，保证物体四周留一圈空白
constexpr float kFitMargin = 1.06f;

// ---------------------------------------------------------------------------
// 斜平行投影的错切矩阵（列主序 GLfloat[16]）
//
// 推导：斜平行投影 = 正交投影 ∘ 沿视线方向的错切。眼空间里可见点的 z_e = -d (d>0)，
// 错切量正比于深度，于是
//     x' = x - L·cosφ·z_e
//     y' = y - L·sinφ·z_e
// 其中 L = cot α 是斜边的缩短系数：斜等测(cavalier) α=45° → L=1；
// 斜二测(cabinet) α≈63.43° → L=0.5。
//
// 关键：m[10] 必须是 1，不能是 0。教材上那个斜投影矩阵是奇异的（把 z 压平成一个平面），
// 直接拿来用会让所有片元落到同一个 NDC 深度，深度测试彻底失效，整个模型 z-fighting。
// 保持仿射形式投影方向完全一致（屏幕上 x,y 结果相同），但深度缓冲仍然可用。
// ---------------------------------------------------------------------------
void buildObliqueShear(float L, float phiRad, float m[16])
{
    m[0] = 1.0f;  m[1] = 0.0f;  m[2]  = 0.0f;  m[3]  = 0.0f;
    m[4] = 0.0f;  m[5] = 1.0f;  m[6]  = 0.0f;  m[7]  = 0.0f;
    m[8] = -L * std::cos(phiRad);      // 错切列（对应 z_e 的系数）
    m[9] = -L * std::sin(phiRad);
    m[10] = 1.0f;
    m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

} // namespace

bool isParallelProjection(ProjMode mode)
{
    return mode != ProjMode::Perspective;
}

const char* projModeName(ProjMode mode)
{
    switch (mode)
    {
    case ProjMode::Perspective: return "Perspective";
    case ProjMode::Ortho:       return "Orthographic";
    case ProjMode::Cavalier:    return "Oblique cavalier (L=1)";
    case ProjMode::Cabinet:     return "Oblique cabinet (L=0.5)";
    }
    return "?";
}

void applyProjection(ProjMode mode, int winW, int winH, const Camera& cam,
                     float sceneRadius, float obliqueAngleDeg)
{
    if (winH <= 0) winH = 1;
    if (winW <= 0) winW = 1;
    if (sceneRadius < 1e-4f) sceneRadius = 1.0f;

    // 保证「世界单位/像素」在 x、y 两个方向相等 —— 这就是不变形的充要条件
    const float aspect = (float)winW / (float)winH;
    const float fitHalfH = sceneRadius * kFitMargin;

    // 取景半高/半宽。
    //
    // 先按高度取景；但窗口很扁（aspect < 1）时，如果还按高度取景，水平方向能看到的
    // 范围就会小于场景，物体左右会被裁掉。所以此时改成按宽度取景、把竖直范围放大。
    //
    // 无论走哪个分支，halfW / halfH 恒等于 aspect —— 这是不变形的关键。
    auto fitExtents = [&](float& halfW, float& halfH) {
        halfH = (aspect >= 1.0f) ? fitHalfH : fitHalfH / aspect;
        halfW = halfH * aspect;
    };

    switch (mode)
    {
    case ProjMode::Perspective:
    {
        const float dist = cam.distance();
        // near 取「到最近几何的距离」和半径的一个小比例中的较大者，避免相机贴近时被裁掉；
        // glFrustum 要求 near > 0，必须夹住。
        float nearZ = dist - sceneRadius * 1.5f;
        const float minNear = sceneRadius * 0.01f;
        if (nearZ < minNear) nearZ = minNear;

        // 相机在场景**内部**时（森林里追尾相机就是这种情形），上面那个式子会给出
        // 一个远大于相机距离的近平面 —— 例如 dist=28、半径 10 时算出 13，
        // 于是把 13 单位内的树整个切掉，露出树干的内部截面，画面上一道道深色尖刺。
        // 近平面永远不该超过相机到目标的四分之一。
        const float maxNear = dist * 0.12f;
        if (nearZ > maxNear) nearZ = maxNear;

        float farZ = dist + sceneRadius * 4.0f;
        if (farZ <= nearZ) farZ = nearZ + sceneRadius;

        // glFrustum 的四个边界是「在近平面上」量的，所以要乘 nearZ。
        // 与正交同理：宽窗口固定竖直 FOV，扁窗口固定水平 FOV。
        const float tanV = std::tan(cam.fovY * 0.5f * kDeg2Rad);
        float halfW, halfH;
        if (aspect >= 1.0f) { halfH = nearZ * tanV;         halfW = halfH * aspect; }
        else                { halfW = nearZ * tanV;         halfH = halfW / aspect; }

        glFrustum(-halfW, halfW, -halfH, halfH, nearZ, farZ);
        break;
    }

    case ProjMode::Ortho:
    {
        float halfW, halfH;
        fitExtents(halfW, halfH);
        // 正交投影用「永不裁剪」的近远平面：glOrtho 允许 near 为负，而且正交深度
        // 精度是线性的，±1000 对 24 位深度缓冲完全够用。这样六种视图和相机漫游
        // 都不会因为相机跑到物体另一侧而把物体裁掉。
        glOrtho(-halfW, halfW, -halfH, halfH, -1000.0, 1000.0);
        break;
    }

    case ProjMode::Cavalier:
    case ProjMode::Cabinet:
    {
        const float L = (mode == ProjMode::Cavalier) ? 1.0f : 0.5f;

        // 屏幕上看到的斜边夹角 θ 与世界空间的 φ 满足 tanθ = tanφ · aspect。
        // 想让屏幕上正好是 obliqueAngleDeg（斜二测教科书上的 45°），要反解 φ。
        const float phiScreen = obliqueAngleDeg * kDeg2Rad;
        const float phi = std::atan(std::tan(phiScreen) / aspect);

        const float d = cam.distance();

        float baseW, baseH;
        fitExtents(baseW, baseH);

        // 错切会把物体整体平移 L·cosφ·d，并额外撑开 L·|cosφ|·r。
        // 前者靠平移 ortho 窗口抵消（顺带让斜投影不再依赖相机距离 —— 平行投影
        // 本来就与距离无关），后者靠放大窗口覆盖。
        //
        // 放大时必须对宽高用同一个比例：如果 x、y 各加各的，halfW/halfH 就不再等于
        // aspect，物体正对相机的那一面会被拉伸 —— 而"正对相机的面保持真实形状"
        // 恰恰是斜平行投影的定义。
        const float spreadX = L * std::fabs(std::cos(phi)) * sceneRadius;
        const float spreadY = L * std::fabs(std::sin(phi)) * sceneRadius;
        const float grow    = std::max(1.0f,
                              std::max((baseW + spreadX) / baseW, (baseH + spreadY) / baseH));

        const float halfW = baseW * grow;
        const float halfH = baseH * grow;

        const float shiftX = L * std::cos(phi) * d;
        const float shiftY = L * std::sin(phi) * d;

        glOrtho(-halfW + shiftX, halfW + shiftX,
                -halfH + shiftY, halfH + shiftY,
                -1000.0, 1000.0);

        // glMultMatrixf 是右乘：M ← M·S，即先错切再正交，正是斜平行投影的定义。
        // 必须放在投影矩阵上：错切是相对投影平面（眼空间）定义的，放到 modelview
        // 就变成世界空间错切，相机一转斜边方向就错了。
        float shear[16];
        buildObliqueShear(L, phi, shear);
        glMultMatrixf(shear);
        break;
    }
    }
}
```

### 25. `src/lighting.h`

光照接口（15 行）

```cpp
#pragma once
// 光照（实验十）。
// 关键顺序：glLightfv(GL_POSITION) 会被「当前 MODELVIEW 矩阵」变换，所以必须在
// gluLookAt 之后、任何物体变换之前调用，否则光会跟着模型一起转、漫游时高光乱飘。
#include <GL/freeglut.h>

// 只设置光照常量（颜色、模型参数、开关）。在 GL 初始化时调用一次。
void initLighting();

// 在每帧的 gluLookAt 之后、物体变换之前调用：把光源位置按当前模型视图变换一次。
// 这样光源固定在世界坐标里，相机移动时明暗关系保持稳定。
void applyLightPositions();

// 顶点法线在交互缩放（非均匀）下会失效，必须重新归一化
inline void enableNormalizeIfLighting() { glEnable(GL_NORMALIZE); }
```

### 26. `src/lighting.cpp`

光照与材质（展示台点光源 / 森林太阳光）（89 行）

```cpp
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
```

### 27. `src/forest.h`

程序化森林接口（28 行）

```cpp
#pragma once
// 程序化森林：地面 + 树，全部烘焙进**一个**带顶点色的 MeshData。
//
// 为什么烘焙成一个而不是每棵树一个物体：
//   - 整片森林一次 glDrawElements 画完，不用几十个 draw call
//   - 树是纯背景，不需要被选中 / 单独变换，合并没有任何损失
//   - 顶点色让树干和树叶在同一个网格里用不同颜色，不必拆成两个材质
//
// 全部几何用固定种子的 xorshift 伪随机数生成 —— 同一个种子必然得到同一片森林，
// 录像才能复现（rand() 的实现各编译器不同，不能用）。
#include "math3d.h"
#include "obj_loader.h"

#include <vector>

// 把整片森林（地面 + 树）写进 out。out 会被清空。
//   halfSize   林地半边长（世界单位）
//   treeCount  树的数量
//   path       兔子会经过的路径采样点（XZ 平面），该路径两侧 clearance 内不种树，
//              否则兔子会从树干里穿过去
//   clearance  路径两侧保留的空地半径
//   seed       随机种子
void buildForest(MeshData& out,
                 float halfSize,
                 int   treeCount,
                 const std::vector<Vec3>& path,
                 float clearance,
                 unsigned seed);
```

### 28. `src/forest.cpp`

森林：地面 + 三种树型，烘焙成一个顶点色网格（321 行）

```cpp
#include "forest.h"

#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// 确定性伪随机：xorshift32。固定种子保证每次运行生成完全一样的森林，录像才可复现。
// （不能用 rand()：各 C 运行库的实现不同，换个编译器森林就变了。）
// ---------------------------------------------------------------------------
struct Rng
{
    unsigned s;
    explicit Rng(unsigned seed) : s(seed ? seed : 1u) {}

    unsigned next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float unit() { return (float)(next() & 0xFFFFFFu) / (float)0x1000000u; }   // [0,1)
    float range(float a, float b) { return a + (b - a) * unit(); }
};

// ---------------------------------------------------------------------------
// 往 MeshData 里塞一个三角形（平面法线）。绕序必须 CCW，否则会被背面剔除。
// ---------------------------------------------------------------------------
void pushTri(MeshData& m,
             const Vec3& a, const Vec3& b, const Vec3& c,
             const float col[3])
{
    const Vec3 n = v3norm(v3cross(b - a, c - a));
    const GLuint base = (GLuint)(m.positions.size() / 3);

    const Vec3* pts[3] = { &a, &b, &c };
    for (int i = 0; i < 3; ++i)
    {
        m.positions.push_back(pts[i]->x);
        m.positions.push_back(pts[i]->y);
        m.positions.push_back(pts[i]->z);
        m.normals.push_back(n.x);
        m.normals.push_back(n.y);
        m.normals.push_back(n.z);
        m.colors.push_back(col[0]);
        m.colors.push_back(col[1]);
        m.colors.push_back(col[2]);
    }
    m.indices.push_back(base);
    m.indices.push_back(base + 1);
    m.indices.push_back(base + 2);

    m.hasNormals = true;
    m.hasColors  = true;
}

// 竖直圆柱侧面（可锥形：r0 在 y0，r1 在 y1）。
// 绕序：在 θ=0 处取 p0=(r,y0,0)，p1≈(r,y0,r·dθ)，q0=(r,y1,0)，q1≈(r,y1,r·dθ)，
// 算 (p0,q0,q1) 的叉积得 +X，正是该处的外法线方向。
void addCylinderSide(MeshData& m, float y0, float y1, float r0, float r1,
                     const float col[3], int sides)
{
    for (int i = 0; i < sides; ++i)
    {
        const float t0 = 2.0f * kPi * (float)i / (float)sides;
        const float t1 = 2.0f * kPi * (float)(i + 1) / (float)sides;

        const Vec3 p0(r0 * std::cos(t0), y0, r0 * std::sin(t0));
        const Vec3 p1(r0 * std::cos(t1), y0, r0 * std::sin(t1));
        const Vec3 q0(r1 * std::cos(t0), y1, r1 * std::sin(t0));
        const Vec3 q1(r1 * std::cos(t1), y1, r1 * std::sin(t1));

        pushTri(m, p0, q0, q1, col);
        pushTri(m, p0, q1, p1, col);
    }
}

// 圆锥：底面圆 (y0, r) 收到顶点 (y1)。
void addCone(MeshData& m, float y0, float y1, float r, const float col[3], int sides)
{
    const Vec3 apex(0.0f, y1, 0.0f);
    for (int i = 0; i < sides; ++i)
    {
        const float t0 = 2.0f * kPi * (float)i / (float)sides;
        const float t1 = 2.0f * kPi * (float)(i + 1) / (float)sides;
        const Vec3 p0(r * std::cos(t0), y0, r * std::sin(t0));
        const Vec3 p1(r * std::cos(t1), y0, r * std::sin(t1));

        // 侧面：(p0, apex, p1) 叉积得 (h·rdθ, r²dθ, 0)，朝外且略微朝上 —— 正确。
        // 反过来写成 (p0, p1, apex) 会变成朝内，整个锥体被剔除。
        pushTri(m, p0, apex, p1, col);
        pushTri(m, v3(0.0f, y0, 0.0f), p0, p1, col);      // 底盖朝下
    }
}

// 椭球（树叶团）。用解析法线做平滑着色，比平面法线更像一团叶子。
void addEllipsoid(MeshData& m, const Vec3& c, float rx, float ry, float rz,
                  const float col[3], int slices, int stacks)
{
    const GLuint base = (GLuint)(m.positions.size() / 3);

    for (int i = 0; i <= stacks; ++i)
    {
        const float phi = kPi * (float)i / (float)stacks;      // 0 = 北极
        for (int j = 0; j <= slices; ++j)
        {
            const float th = 2.0f * kPi * (float)j / (float)slices;
            const float sx = std::sin(phi) * std::cos(th);
            const float sy = std::cos(phi);
            const float sz = std::sin(phi) * std::sin(th);

            m.positions.push_back(c.x + rx * sx);
            m.positions.push_back(c.y + ry * sy);
            m.positions.push_back(c.z + rz * sz);

            // 椭球在 (rx·sx, ry·sy, rz·sz) 处的法线正比于 (sx/rx, sy/ry, sz/rz)
            const Vec3 n = v3norm(v3(sx / rx, sy / ry, sz / rz));
            m.normals.push_back(n.x);
            m.normals.push_back(n.y);
            m.normals.push_back(n.z);

            m.colors.push_back(col[0]);
            m.colors.push_back(col[1]);
            m.colors.push_back(col[2]);
        }
    }

    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            const GLuint a = base + (GLuint)(i * (slices + 1) + j);
            const GLuint b = a + (GLuint)(slices + 1);
            m.indices.push_back(a);     m.indices.push_back(a + 1); m.indices.push_back(b);
            m.indices.push_back(a + 1); m.indices.push_back(b + 1); m.indices.push_back(b);
        }
    }
    m.hasNormals = true;
    m.hasColors  = true;
}

// ---------------------------------------------------------------------------
// 地面：细分网格 + 平滑的草色变化。
// 必须细分：固定管线的雾是**逐顶点**算的，一整块大平面只有四个角有雾值，
// 远处会突变成一条硬边。
// ---------------------------------------------------------------------------
void addGround(MeshData& m, float halfSize, int div)
{
    // Shared vertices give adjacent cells identical colours, avoiding visible tile seams.
    const GLuint base = static_cast<GLuint>(m.positions.size()/3);
    const float step=2.0f*halfSize/div;
    for(int i=0;i<=div;++i) for(int j=0;j<=div;++j) {
        float x=-halfSize+step*i, z=-halfSize+step*j;
        float g=0.5f+0.5f*std::sin(x*0.23f)*std::cos(z*0.19f);
        float d=0.5f+0.5f*std::sin(x*0.07f+2.1f)*std::sin(z*0.09f-1.3f);
        m.positions.insert(m.positions.end(),{x,0.0f,z});
        m.normals.insert(m.normals.end(),{0.0f,1.0f,0.0f});
        m.colors.insert(m.colors.end(),{0.12f+0.09f*g+0.07f*d,0.25f+0.15f*g+0.03f*d,0.09f+0.06f*g+0.03f*d});
    }
    for(int i=0;i<div;++i) for(int j=0;j<div;++j) {
        GLuint a=base+i*(div+1)+j, b=a+1, c=b+div+1, d=a+div+1;
        m.indices.insert(m.indices.end(),{a,b,c,a,c,d});
    }
    m.hasNormals=true; m.hasColors=true;
}

// ---------------------------------------------------------------------------
// 三种树型。都按「高度 1、底面在 y = 0、中心在原点」生成，
// 摆放时用 appendScaled 统一缩放平移。
// ---------------------------------------------------------------------------
void buildConifer(MeshData& t, Rng& rng)
{
    const float trunk[3] = { 0.20f, 0.13f, 0.07f };
    addCylinderSide(t, 0.0f, 0.30f, 0.030f, 0.020f, trunk, 6);

    const float b = rng.range(0.0f, 0.08f);
    const float g1[3] = { 0.10f + b, 0.24f + b, 0.09f + b * 0.5f };
    const float g2[3] = { 0.08f + b, 0.21f + b, 0.08f + b * 0.5f };

    addCone(t, 0.22f, 0.62f, 0.190f, g1, 8);
    addCone(t, 0.48f, 0.84f, 0.140f, g1, 8);
    addCone(t, 0.71f, 1.00f, 0.088f, g2, 8);
}

void buildRoundTree(MeshData& t, Rng& rng)
{
    const float trunk[3] = { 0.24f, 0.16f, 0.09f };
    addCylinderSide(t, 0.0f, 0.46f, 0.034f, 0.026f, trunk, 6);

    const float v = rng.range(0.0f, 0.45f);
    const float g1[3] = { 0.13f + v, 0.29f + v, 0.10f + v * 0.4f };
    const float g2[3] = { 0.10f + v, 0.25f + v, 0.08f + v * 0.4f };

    addEllipsoid(t, v3(rng.range(-0.02f, 0.02f), 0.62f, rng.range(-0.02f, 0.02f)),
                 0.24f, 0.21f, 0.24f, g1, 8, 5);
    addEllipsoid(t, v3(rng.range(-0.05f, 0.05f), 0.79f, rng.range(-0.05f, 0.05f)),
                 0.17f, 0.15f, 0.17f, g2, 8, 4);
    addEllipsoid(t, v3(rng.range(-0.05f, 0.05f), 0.53f, rng.range(-0.05f, 0.05f)),
                 0.16f, 0.14f, 0.16f, g2, 8, 4);
}

void buildBush(MeshData& t, Rng& rng)
{
    const float v = rng.range(0.0f, 0.5f);
    const float g1[3] = { 0.12f + v, 0.28f + v, 0.09f + v * 0.4f };
    const float g2[3] = { 0.09f + v, 0.24f + v, 0.07f + v * 0.4f };

    addEllipsoid(t, v3(0.0f, 0.45f, 0.0f), 0.42f, 0.45f, 0.42f, g1, 8, 5);
    addEllipsoid(t, v3(rng.range(-0.10f, 0.10f), 0.62f, rng.range(-0.10f, 0.10f)),
                 0.30f, 0.30f, 0.30f, g2, 8, 4);
}

// 把一棵「单位高度」的树缩放 height 倍后平移到 (x, z)，索引偏移后追加进 out。
// 均匀缩放不改变法线方向，所以法线可以直接照搬。
void appendScaled(MeshData& out, const MeshData& tree,
                  float x, float z, float height, float yawRad)
{
    const GLuint base = (GLuint)(out.positions.size() / 3);
    const float cs = std::cos(yawRad), sn = std::sin(yawRad);

    const size_t nv = tree.positions.size() / 3;
    for (size_t i = 0; i < nv; ++i)
    {
        const float px = tree.positions[i * 3 + 0] * height;
        const float py = tree.positions[i * 3 + 1] * height;
        const float pz = tree.positions[i * 3 + 2] * height;

        // 绕 Y 转一圈，让同一种树看起来不重样
        out.positions.push_back(px * cs + pz * sn + x);
        out.positions.push_back(py);
        out.positions.push_back(-px * sn + pz * cs + z);

        out.normals.push_back(tree.normals[i * 3 + 0]);
        out.normals.push_back(tree.normals[i * 3 + 1]);
        out.normals.push_back(tree.normals[i * 3 + 2]);

        out.colors.push_back(tree.colors[i * 3 + 0]);
        out.colors.push_back(tree.colors[i * 3 + 1]);
        out.colors.push_back(tree.colors[i * 3 + 2]);
    }

    for (GLuint idx : tree.indices)
        out.indices.push_back(base + idx);

    out.hasNormals = true;
    out.hasColors  = true;
}

// 点到路径采样点的最近距离平方（XZ 平面）
float distSqToPath(const std::vector<Vec3>& path, float x, float z)
{
    float best = 1e30f;
    for (const Vec3& p : path)
    {
        const float dx = p.x - x;
        const float dz = p.z - z;
        const float d2 = dx * dx + dz * dz;
        if (d2 < best) best = d2;
    }
    return best;
}

} // namespace

// ---------------------------------------------------------------------------
void buildForest(MeshData& out,
                 float halfSize,
                 int   treeCount,
                 const std::vector<Vec3>& path,
                 float clearance,
                 unsigned seed)
{
    out = MeshData{};
    Rng rng(seed);

    addGround(out, halfSize, 48);

    // 拒绝采样：随机撒点，太靠近兔子路线的丢掉，否则兔子会从树干里穿过去。
    // 用尝试次数上限而不是 while(true)，参数不合理时也不会死循环。
    const float clearSq = clearance * clearance;
    const float margin  = 2.5f;
    const int   maxTries = treeCount * 30;

    MeshData tree;
    int placed = 0;

    for (int tries = 0; tries < maxTries && placed < treeCount; ++tries)
    {
        const float x = rng.range(-halfSize + margin, halfSize - margin);
        const float z = rng.range(-halfSize + margin, halfSize - margin);
        if (distSqToPath(path, x, z) < clearSq) continue;

        tree = MeshData{};                       // 复用同一个临时缓冲

        const float pick = rng.unit();
        float height;
        if (pick < 0.42f)
        {
            buildConifer(tree, rng);
            height = rng.range(11.0f, 22.0f);
        }
        else if (pick < 0.75f)
        {
            buildRoundTree(tree, rng);
            height = rng.range(8.0f, 16.0f);
        }
        else
        {
            buildBush(tree, rng);
            height = rng.range(1.8f, 3.6f);      // 灌木本来就矮
        }

        // 针叶树保持竖直；阔叶树稍微倾斜一点更自然
        // （倾斜要绕水平轴，这里简单起见只做随机偏航 + 轻微高度差异）
        appendScaled(out, tree, x, z, height, rng.range(0.0f, 2.0f * kPi));
        ++placed;
    }
}
```

### 29. `src/scene.h`

场景与物体接口（63 行）

```cpp
#pragma once
// 三维场景：若干个物体 + 每个物体自己的累积变换矩阵 objectXform（实验五）。
#include "mesh.h"
#include "math3d.h"
#include "types.h"

#include <memory>
#include <string>
#include <vector>

// 物体类型。Mesh 之外的几种用 GLUT 预定义图元 / 手写顶点绘制。
enum class PrimKind
{
    Mesh,       // 导入的 OBJ 模型
    Cube,       // 实验四的手写立方体（8 顶点 6 面 + 顶点色）
    Teapot,     // glutSolidTeapot
    Sphere,     // glutSolidSphere
    Ground,     // 地面平台
    Grid,       // 地面网格线
    Axes        // 坐标轴
};

struct Object
{
    std::string           name;
    PrimKind              kind        = PrimKind::Cube;
    std::shared_ptr<Mesh> mesh;                    // kind == Mesh 时有效
    float                 xform[16];               // objectXform：累积变换矩阵
    float                 size        = 1.0f;
    float                 pos[3]      = { 0, 0, 0 };
    float                 color[3]    = { 1, 1, 1 };
    bool                  visible     = true;
    bool                  selectable  = true;
    bool                  useMeshMaterial = false; // 用 .mtl 材质还是 glColor
};

class Scene
{
public:
    // 建场景。modelPath 为空时跳过模型，只留图元。
    bool build(const std::string& modelPath, UpAxis up, SceneKind kind,
               std::string* errOut = nullptr);

    void  drawAll(bool useTexture) const;
    static void drawOne(const Object& o, bool useTexture);

    void  resetTransforms();                       // 所有物体变换复位为单位阵

    float radius() const { return radius_; }       // 场景半径，用于自适应取景
    const std::string& modelStatus() const { return modelStatus_; }

    // 在场景中把第 idx 个「可选中的物体」加 1，返回新的下标（供 Tab 循环切换）
    int  nextSelectable(int idx) const;
    const Object* selectedObject() const;

    std::vector<Object>                 objects;
    std::vector<std::shared_ptr<Mesh>>  meshes;
    int                                 selected = 0;

private:
    float       radius_      = 2.2f;
    std::string modelStatus_ = "(no model loaded)";
};
```

### 30. `src/scene.cpp`

两套场景：展示台（实验四/五）与森林（375 行）

```cpp
#include "scene.h"
#include "anim.h"      // roamPath：种树时要避开兔子真正走的路线
#include "forest.h"

#include <cmath>
#include <cstdio>

namespace {

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
        // 采样兔子的漫游路线，种树时避开 —— 否则兔子会直接从树干里穿过去。
        // 路径函数与漫游动画共用同一个 roamPath，两边永远一致。
        std::vector<Vec3> path;
        path.reserve(722);
        for (int i = 0; i <= 720; ++i)
            path.push_back(roamPath(0.02f * (float)i));       // 覆盖 0 ~ 14.4 秒

        // 兔子默认站在原点，但漫游曲线 t=0 的位置离原点还有 5 个单位（约 (4.8, -2.5)），
        // 所以原点根本不在避让范围内 —— 实测会有一棵树正好杵在兔子和默认相机之间，
        // 把兔子挡掉一大半。把原点也加进避让点，等于给兔子留一小片林间空地：
        // 近景干净，树落在中景，正好是最舒服的构图。
        path.push_back(v3(0.0f, 0.0f, 0.0f));

        // clearance 必须大于追尾相机的跟随距离（anim.cpp 里 kCamDist = 4.6）：
        // 相机吊在兔子后面 4.6 单位，如果树只让开 3.6，相机会一头扎进树干里，
        // 因为背面剔除会直接看穿那棵树，树就"闪没"了。
        MeshData forestData;
        buildForest(forestData, /*halfSize=*/34.0f, /*treeCount=*/150,
                    path, /*clearance=*/7.0f, /*seed=*/20260924u);

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
            // 森林地面在 y = 0，而模型原点在包围球中心，所以要垫到脚底刚好贴地
            o.pos[1] = -model->data().bboxMin[1];
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
```

### 31. `src/anim.h`

脚本动画接口（30 行）

```cpp
#pragma once
// 录制用的脚本动画。
//
// 关键约束：applyAnimation 必须是**时间的纯函数** —— 同一个 t 永远得到同一个场景/相机状态。
// 录像时渲染速度是不确定的（这台机器快、换台机器慢），如果动画按真实时间推进，
// 出来的视频就会忽快忽慢。用「帧序号 / fps」当时间轴，离线渲染多少帧都由参数决定，
// 视频必然匀速且可复现。
#include "math3d.h"

#include <string>

enum class AnimKind
{
    None,          // 不动（只用于单帧截图）
    Turntable,     // 模型绕 Y 轴自转，相机不动
    Orbit,         // 相机绕模型环绕，模型不动（演示观察变换 / 实验七）
    Projections,   // 依次切换四种投影模式（实验七 + 斜平行投影加分项）
    Transforms,    // 立方体依次做平移 / 旋转 / 缩放（实验五几何变换加分项）
    Roam           // 兔子在森林里蹦跳漫步（程序化步态 + 追尾相机）
};

// 漫步路径（XZ 平面上的平滑曲线）。森林生成和漫游动画共用这一个函数，
// 保证「种树时避开的路线」正是「兔子真正走过的路线」。
Vec3 roamPath(float t);

bool parseAnimKind(const std::string& s, AnimKind& out);
const char* animKindName(AnimKind k);

// 把场景/相机设置到该动画在 t 秒时的状态。totalSeconds 是动画总时长。
void applyAnimation(AnimKind kind, float t, float totalSeconds);
```

### 32. `src/anim.cpp`

动画（时间的纯函数）：自转/环绕/投影/变换/森林漫步（289 行）

```cpp
#include "anim.h"
#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// 蹦跳步态参数（森林漫游用）
constexpr float kHopFreq   = 2.2f;    // 每秒跳几下
constexpr float kHopHeight = 0.52f;   // 跳跃高度，模型已归一化（兔子高约 1.5 单位）
constexpr float kCamDist   = 4.6f;    // 追尾相机在兔子后方的距离
constexpr float kCamHeight = 1.75f;   // 追尾相机高度

Object* findObjectByName(const char* name)
{
    for (Object& o : g_app.scene.objects)
        if (o.name == name) return &o;
    return nullptr;
}

// 优先拿导入的模型，没有就退回彩色立方体
Object* findSpinTarget()
{
    for (Object& o : g_app.scene.objects)
        if (o.kind == PrimKind::Mesh) return &o;
    return findObjectByName("Color Cube");
}

// 场景里第一个导入的模型（森林场景里就是兔子）
Object* findFirstMesh()
{
    for (Object& o : g_app.scene.objects)
        if (o.kind == PrimKind::Mesh && o.mesh) return &o;
    return nullptr;
}

inline float smoothOutAndBack(float local01)
{
    // local 0 -> 0, 0.5 -> 1, 1 -> 0，两端速度为 0，看起来不突兀
    return std::sin(local01 * kPi);
}

} // namespace

bool parseAnimKind(const std::string& s, AnimKind& out)
{
    if (s == "turntable" || s == "turn" || s == "spin") out = AnimKind::Turntable;
    else if (s == "orbit"  || s == "circle")            out = AnimKind::Orbit;
    else if (s == "projections" || s == "proj" || s == "projection") out = AnimKind::Projections;
    else if (s == "transforms" || s == "transform" || s == "xform")  out = AnimKind::Transforms;
    else if (s == "roam" || s == "walk" || s == "hop")  out = AnimKind::Roam;
    else if (s == "none")                               out = AnimKind::None;
    else return false;
    return true;
}

Vec3 roamPath(float t)
{
    // 两个不同频率的正弦叠加：轨迹平滑、不闭合，看起来像在悠闲地逛而不是绕圈
    const float x = 13.0f * std::sin(0.115f * t) + 5.0f * std::sin(0.283f * t + 1.3f);
    const float z = 13.0f * std::cos(0.097f * t) + 5.0f * std::cos(0.241f * t + 2.1f);
    return v3(x, 0.0f, z);
}

const char* animKindName(AnimKind k)
{
    switch (k)
    {
    case AnimKind::None:        return "none";
    case AnimKind::Turntable:   return "turntable (model spins about Y)";
    case AnimKind::Orbit:       return "orbit (camera circles the model)";
    case AnimKind::Projections: return "projections (perspective/ortho/cavalier/cabinet)";
    case AnimKind::Transforms:  return "transforms (translate/rotate/scale the cube)";
    case AnimKind::Roam:        return "roam (bunny hopping through the forest)";
    }
    return "?";
}

void applyAnimation(AnimKind kind, float t, float totalSeconds)
{
    if (totalSeconds <= 0.0f) totalSeconds = 1.0f;
    const float u = std::min(1.0f, t / totalSeconds);        // 归一化进度 0..1

    // 几何变换演示只留立方体 + 地面/网格/坐标轴作空间参照。
    // 兔子半径 1.0（两单位宽）、离相机又只有两三个单位，不管把相机挪到哪一侧都会
    // 占掉一大块画面；要让立方体的旋转和缩放看清楚，只能把它藏起来。
    // 每个动画分支都显式设置可见性，保证同一 kind 下结果可复现。
    const bool cubeOnly = (kind == AnimKind::Transforms);
    for (Object& o : g_app.scene.objects)
    {
        if (!cubeOnly)
        {
            o.visible = true;
        }
        else
        {
            o.visible = (o.kind == PrimKind::Ground || o.kind == PrimKind::Grid ||
                         o.kind == PrimKind::Axes   || o.name == "Color Cube");
        }
    }

    switch (kind)
    {
    // -----------------------------------------------------------------------
    case AnimKind::Turntable:
    {
        Object* o = findSpinTarget();
        if (o) mat4FromAxisAngle(o->xform, 360.0f * u, 0.0f, 1.0f, 0.0f);

        // 相机固定在默认站位，让模型自己在转
        g_app.viewPreset = ViewPreset::Free;
        g_app.camera.reset(g_app.scene.radius());
        break;
    }

    // -----------------------------------------------------------------------
    case AnimKind::Orbit:
    {
        Camera& c = g_app.camera;
        const float R = g_app.scene.radius() * 2.8f;
        const float a = 2.0f * kPi * u;

        c.target = v3(0.0f, 0.0f, 0.0f);
        c.eye    = v3(R * std::sin(a), 0.42f * R, R * std::cos(a));
        c.up     = v3(0.0f, 1.0f, 0.0f);
        c.fovY   = 45.0f;

        g_app.viewPreset = ViewPreset::Free;
        break;
    }

    // -----------------------------------------------------------------------
    case AnimKind::Projections:
    {
        // 四段等长，依次是 透视 / 正平行 / 斜等测 / 斜二测
        const int phase = std::min(3, (int)(u * 4.0f));

        // 平行投影（含斜平行）从正视图方向看最能体现"投影"的差别；
        // 透视则用自由相机，有纵深感。
        if (phase == 0)
        {
            g_app.projMode   = ProjMode::Perspective;
            g_app.viewPreset = ViewPreset::Free;
        }
        else
        {
            g_app.projMode   = (phase == 1) ? ProjMode::Ortho
                             : (phase == 2) ? ProjMode::Cavalier
                                            : ProjMode::Cabinet;
            g_app.viewPreset = ViewPreset::Front;
            g_app.camera.setPreset((int)ViewPreset::Front, g_app.scene.radius());
        }
        break;
    }

    // -----------------------------------------------------------------------
    case AnimKind::Transforms:
    {
        Object* cube = findObjectByName("Color Cube");
        if (!cube) cube = findSpinTarget();
        if (!cube) break;

        // 把相机拉到立方体附近并正对着它。场景级取景（相机距离 ~4.4）下立方体
        // 只占画面高度的 11%，旋转和缩放的幅度根本看不出来。
        const Vec3 base = v3(cube->pos[0], cube->pos[1], cube->pos[2]);
        Camera& c = g_app.camera;
        // 归一化的观察方向，这样 D 就是真正的相机距离。用常规的四分之三视角。
        const Vec3 dir = v3norm(v3(0.35f, 0.42f, 1.00f));
        const float D = 2.3f;
        c.target = base;
        c.eye    = base + dir * D;
        c.up     = v3(0.0f, 1.0f, 0.0f);
        c.fovY   = 45.0f;
        g_app.viewPreset = ViewPreset::Free;
        g_app.projMode   = ProjMode::Perspective;

        // 三段：平移 → 旋转 → 缩放
        const int   phase = std::min(2, (int)(u * 3.0f));
        const float local = std::min(1.0f, std::max(0.0f, u * 3.0f - (float)phase));

        // 物体自身的尺寸作为幅度基准，换模型不用改常数
        const float amp = cube->size;

        float m[16];
        mat4Identity(m);
        switch (phase)
        {
        case 0:   // 平移：往 +X 出去再回来，同时略微抬起，避免和地面视觉上粘住
        {
            const float k = smoothOutAndBack(local);
            mat4FromTranslation(m, amp * 2.0f * k, amp * 0.7f * k, 0.0f);
            break;
        }
        case 1:   // 旋转：绕 Y 轴整整一圈
            mat4FromAxisAngle(m, 360.0f * local, 0.0f, 1.0f, 0.0f);
            break;
        default:  // 缩放：从地面往上长
        {
            // 立方体的 xform 作用在「已经按 size 缩放过的」坐标上，底面在 -1*size。
            // 放大约 1.8 倍后底面会掉到 -1.8*size，也就是陷进地面里；这里同步抬升
            // size*(s-1) 让底面保持在原处。
            const float s = 1.0f + 0.8f * smoothOutAndBack(local);
            mat4FromScale(m, s, s, s);
            m[13] = cube->size * (s - 1.0f);
            break;
        }
        }
        std::memcpy(cube->xform, m, sizeof(m));

        // 让 HUD 的 "Selected" 跟着指向立方体：画面里只剩它，写着 Rabbit 会误导
        for (size_t i = 0; i < g_app.scene.objects.size(); ++i)
            if (&g_app.scene.objects[i] == cube) g_app.scene.selected = (int)i;
        break;
    }

    // -----------------------------------------------------------------------
    case AnimKind::Roam:
    {
        Object* bunny = findFirstMesh();
        if (!bunny || !bunny->mesh) break;

        // 水平位置沿漫步曲线走
        const Vec3 p = roamPath(t);

        // 朝向取路径切线。模型自身朝 -X（顶视图确认过）：绕 Y 转 θ 后模型前向是
        // (-cosθ, 0, sinθ)，令它等于单位切线 h，即 θ = atan2(h.z, -h.x)。
        const Vec3 ahead = roamPath(t + 0.06f);
        const Vec3 h = v3norm(v3(ahead.x - p.x, 0.0f, ahead.z - p.z));
        if (v3len(h) < 1e-6f) break;
        const float yawDeg = std::atan2(h.z, -h.x) * 180.0f / kPi;

        // 蹦跳弧线用抛物线（4k(1-k)）而不是正弦 —— 更接近真实的弹道，
        // 落地那一下的顿挫感明显得多。k: 0 在地面，1 在最高点。
        const float phase = t * kHopFreq;
        const float frac  = phase - std::floor(phase);
        const float k     = 4.0f * frac * (1.0f - frac);
        const float lift  = kHopHeight * k;

        // 腾空拉长、落地压扁（动画十二原则的 squash & stretch），用 1/sqrt 保持体积
        const float sy  = 1.0f + 0.26f * (k - 0.32f);
        const float sxz = 1.0f / std::sqrt(sy);

        // 起跳抬头、下落低头。
        //
        // 注意轴：模型朝 -X，所以"俯仰"（机头上下）是绕 **Z** 轴转，不是绕 X 轴 ——
        // 绕 X 转只会让它左右翻滚。绕 Z 转 θ 时前向 (-1,0,0) 变成 (-cosθ,-sinθ,0)，
        // 即 θ>0 低头、θ<0 抬头。
        const float pitchDeg = 13.0f * (frac * 2.0f - 1.0f);   // 起跳 -13(抬头) → 落地 +13(低头)

        float mYaw[16], mPitch[16], mSquash[16], tmp[16];
        mat4FromAxisAngle(mYaw,   yawDeg,   0.0f, 1.0f, 0.0f);
        mat4FromAxisAngle(mPitch, pitchDeg, 0.0f, 0.0f, 1.0f);
        mat4FromScale(mSquash, sxz, sy, sxz);
        mat4Multiply(tmp, mYaw, mPitch);
        mat4Multiply(bunny->xform, tmp, mSquash);

        // 模型原点在包围球中心而不是脚底，得垫高 standY 才踩在地上；
        // 压扁时脚还会往上跑，乘 sy 抵回来让脚始终贴着地面。
        const float standY = -bunny->mesh->data().bboxMin[1];
        bunny->pos[0] = p.x;
        bunny->pos[1] = lift + standY * sy;
        bunny->pos[2] = p.z;

        // 追尾相机：斜后上方，看向兔子前方一点，让兔子落在画面中下部
        Camera& c = g_app.camera;
        c.eye    = v3(p.x - h.x * kCamDist, kCamHeight + lift * 0.30f, p.z - h.z * kCamDist);
        c.target = v3(p.x + h.x * 2.2f, 0.85f + lift * 0.55f, p.z + h.z * 2.2f);
        c.up     = v3(0.0f, 1.0f, 0.0f);
        c.fovY   = 45.0f;

        g_app.viewPreset = ViewPreset::Free;
        g_app.projMode   = ProjMode::Perspective;

        // HUD 指着兔子
        for (size_t i = 0; i < g_app.scene.objects.size(); ++i)
            if (&g_app.scene.objects[i] == bunny) g_app.scene.selected = (int)i;
        break;
    }

    // -----------------------------------------------------------------------
    case AnimKind::None:
    default:
        break;
    }
}
```

### 33. `src/recorder.h`

录制接口（19 行）

```cpp
#pragma once
// 视频录制：把逐帧 RGB 数据管进 ffmpeg 的 stdin，由它编码成 H.264 mp4。
// 全程不落临时文件。
//
// 用管道而不是「先存一堆图再调 ffmpeg」的原因：1280x800 一帧就是 3 MB，
// 6 秒 30fps 要 540 MB 临时空间，而管道是流式的。
#include <string>

// 启动 ffmpeg 子进程。w/h 必须与实际帧尺寸一致，fps 决定输出帧率。
bool recStart(const std::string& outPath, int w, int h, int fps, std::string* err);

// 写入一帧。rgb 为自下而上的 RGB24（glReadPixels 的原生行序），
// ffmpeg 侧用 -vf vflip 翻正。
bool recWriteFrame(const unsigned char* rgb);

// 关闭管道并等待 ffmpeg 结束。返回 false 表示编码失败（含 ffmpeg 不在 PATH）。
bool recStop(std::string* err);

bool recActive();
```

### 34. `src/recorder.cpp`

逐帧数据管道进 ffmpeg 编码 mp4（76 行）

```cpp
#include "recorder.h"

#include <cstdio>
#include <cstring>

#if defined(_MSC_VER)
  #define POPEN  _popen
  #define PCLOSE _pclose
#else
  #define POPEN  popen
  #define PCLOSE pclose
#endif

namespace {
std::FILE*  g_pipe = nullptr;
int         g_w = 0, g_h = 0;
std::string g_out;
} // namespace

bool recActive() { return g_pipe != nullptr; }

bool recStart(const std::string& outPath, int w, int h, int fps, std::string* err)
{
    if (g_pipe) { if (err) *err = "recorder already running"; return false; }
    if (w <= 0 || h <= 0 || fps <= 0)
    {
        if (err) *err = "bad record parameters";
        return false;
    }

    // -y 覆盖旧文件；-loglevel error 只保留错误输出，不干扰控制台
    // -f rawvideo + -i - 表示从 stdin 读原始帧
    // -vf vflip 把 glReadPixels 的自下而上行序翻正
    // -pix_fmt yuv420p 是兼容性最好的像素格式（QuickTime / 播放器 / PPT 都能放）
    char cmd[1200];
    std::snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -hide_banner -loglevel error "
        "-f rawvideo -pixel_format rgb24 -video_size %dx%d -framerate %d -i - "
        "-vf vflip -c:v libx264 -preset medium -crf 18 -pix_fmt yuv420p \"%s\"",
        w, h, fps, outPath.c_str());

    g_pipe = POPEN(cmd, "wb");
    if (!g_pipe)
    {
        if (err) *err = "failed to start ffmpeg (is ffmpeg.exe on PATH?)";
        return false;
    }
    g_w = w;
    g_h = h;
    g_out = outPath;
    return true;
}

bool recWriteFrame(const unsigned char* rgb)
{
    if (!g_pipe || !rgb) return false;
    const size_t n = (size_t)g_w * (size_t)g_h * 3u;
    return std::fwrite(rgb, 1, n, g_pipe) == n;
}

bool recStop(std::string* err)
{
    if (!g_pipe) return false;

    std::FILE* p = g_pipe;
    g_pipe = nullptr;

    // 关掉 stdin 让 ffmpeg 收尾（写 moov atom），_pclose 会等它退出
    const int rc = PCLOSE(p);
    if (rc != 0)
    {
        if (err) *err = "ffmpeg exited with code " + std::to_string(rc);
        return false;
    }
    return true;
}
```

### 35. `src/hud.h`

叠加文字接口（21 行）

```cpp
#pragma once
// 屏幕叠加文字（对应实验六里用 glutBitmapCharacter 显示信息的做法）。
// 注意：GLUT 的位图字体只支持 ASCII，所以 HUD 文案用英文。
#include <string>

// 在当前视口上开启 2D 叠加层（内部保存并恢复投影/模型视图矩阵，关闭光照与深度测试）
void hudBegin(int viewportW, int viewportH);
void hudEnd();

void hudColor(float r, float g, float b);
void hudText(float x, float y, const std::string& s);
void hudTextf(float x, float y, const char* fmt, ...);

// 一行文字的实际像素宽度（用 glutBitmapWidth 逐字符累加）。
// 面板尺寸要靠它实测，写死宽度会把长行截断。
float hudTextWidth(const std::string& s);

// 画一个半透明底板，给帮助面板用
void hudPanel(float x, float y, float w, float h, float alpha);

constexpr float kHudLineH = 16.0f;    // 行高（配合 GLUT_BITMAP_9_BY_15）
```

### 36. `src/hud.cpp`

屏幕叠加文字与帮助面板（89 行）

```cpp
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
```

### 37. `src/input.h`

输入接口（47 行）

```cpp
#pragma once
// 键盘 / 鼠标 / 滚轮 / 右键菜单（实验六）。
// 键位安排遵循指导书的提醒：缩放键用 y 而不是 s，避免与 WASD 相机漫游的 s 冲突。

// 菜单项编号。子菜单用不同的区间，父菜单只做分发。
enum MenuId
{
    M_NONE = 0,

    // 变换
    M_TRANS_T = 100, M_TRANS_R, M_TRANS_S,
    M_RESET_OBJ,
    M_SELECT_NEXT,

    // 投影
    M_PROJ_PERSP = 200, M_PROJ_ORTHO, M_PROJ_CAV, M_PROJ_CAB,
    M_OBL_ANGLE_DOWN, M_OBL_ANGLE_UP,
    M_FOV_DOWN, M_FOV_UP,

    // 视图
    M_VIEW_FRONT = 300, M_VIEW_BACK, M_VIEW_LEFT, M_VIEW_RIGHT,
    M_VIEW_TOP, M_VIEW_BOTTOM, M_VIEW_FREE,
    M_RESET_CAM,
    M_TOGGLE_QUAD,

    // 渲染
    M_RENDER_SOLID = 400, M_RENDER_WIRE, M_RENDER_POINT,
    M_TOGGLE_CULL, M_TOGGLE_SHADE, M_TOGGLE_LIGHT, M_TOGGLE_TEX,

    // 显示
    M_TOGGLE_GRID = 500, M_TOGGLE_AXES, M_TOGGLE_GROUND, M_TOGGLE_HUD,

    // 其他
    M_SCREENSHOT = 600, M_HELP, M_QUIT
};

void inputKeyboard(unsigned char key, int x, int y);
void inputSpecial(int key, int x, int y);
void inputMouseButton(int button, int state, int x, int y);
void inputMouseMotion(int x, int y);
void inputMouseWheel(int wheel, int direction, int x, int y);

void inputBuildMenu();
void inputMenu(int choice);

// 供菜单直接复用键盘动作
void inputAction(int action);
```

### 38. `src/input.cpp`

键盘 / 鼠标 / 滚轮 / 右键菜单（实验六）（382 行）

```cpp
#include "input.h"
#include "app.h"
#include "math3d.h"

#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// 拖动时鼠标位移 → 变换量的换算基准
constexpr float kTransWorldPerWindow = 2.0f;    // 拖过整个窗口约等于移动 2 个场景半径
constexpr float kRotDegPerWindow     = 180.0f;
constexpr float kScalePerWindow      = 2.0f;

inline bool shiftDown() { return (glutGetModifiers() & GLUT_ACTIVE_SHIFT) != 0; }

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// 相机被手动操作过之后，视图预设要让位给自由相机。
//
// 同时停掉动画预览：预热播放时每帧都会覆写相机，用户拖半天看不到反应。
// 手动一介入就交还控制权，预览可以随时按空格再开。
// appStopPreview() 内部对「本来就没在播」直接返回，所以这里可以放心反复调。
inline void leavePreset()
{
    g_app.viewPreset = ViewPreset::Free;
    appStopPreview();
}

// ---------------------------------------------------------------------------
// 实验五/六：由鼠标位移构造一次增量变换矩阵
// ---------------------------------------------------------------------------
void buildObjectDelta(TransMode mode, float dx, float dy, float delta[16])
{
    const float winW = (float)std::max(1, g_app.winW);
    const float winH = (float)std::max(1, g_app.winH);
    const float r    = g_app.scene.radius();

    switch (mode)
    {
    case TransMode::Translate:
    {
        // 窗口坐标系的 y 轴与世界坐标系相反，所以 dy 取负
        const float tx =  dx * (kTransWorldPerWindow * r / winW);
        const float ty = -dy * (kTransWorldPerWindow * r / winH);
        mat4FromTranslation(delta, tx, ty, 0.0f);
        break;
    }

    case TransMode::Rotate:
    {
        // 拖动方向决定旋转轴，拖动距离决定角度（轨球式手感）
        const float angle = std::sqrt(dx * dx + dy * dy) * (kRotDegPerWindow / std::max(winW, winH));
        float ax = dy, ay = dx, az = 0.0f;
        // 两个分量都接近 0 时轴退化，mat4FromAxisAngle 会返回单位阵，正好表示无旋转
        mat4FromAxisAngle(delta, angle, ax, ay, az);
        break;
    }

    case TransMode::Scale:
    {
        // 指导书提醒：轴向量本身不加 1，调用时才加 —— 否则鼠标没动时缩放会归零
        const float sx = clampf(1.0f + dx * (kScalePerWindow / winW), 0.05f, 20.0f);
        const float sy = clampf(1.0f - dy * (kScalePerWindow / winH), 0.05f, 20.0f);
        const float sz = std::sqrt(sx * sy);       // 保持体积大致不变
        mat4FromScale(delta, sx, sy, sz);
        break;
    }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// 统一动作入口：键盘与菜单共用同一套编号
// ---------------------------------------------------------------------------
void inputAction(int action)
{
    switch (action)
    {
    // ---- 变换模式 ----
    case M_TRANS_T: g_app.transMode = TransMode::Translate; break;
    case M_TRANS_R: g_app.transMode = TransMode::Rotate;    break;
    case M_TRANS_S: g_app.transMode = TransMode::Scale;     break;
    case M_RESET_OBJ: appResetObjects(); break;
    case M_SELECT_NEXT:
        g_app.scene.selected = g_app.scene.nextSelectable(g_app.scene.selected);
        appUpdateTitle();
        break;

    // ---- 投影 ----
    case M_PROJ_PERSP: appSetProjection(ProjMode::Perspective); break;
    case M_PROJ_ORTHO: appSetProjection(ProjMode::Ortho);       break;
    case M_PROJ_CAV:   appSetProjection(ProjMode::Cavalier);    break;
    case M_PROJ_CAB:   appSetProjection(ProjMode::Cabinet);     break;
    case M_OBL_ANGLE_DOWN: g_app.obliqueAngleDeg = clampf(g_app.obliqueAngleDeg - 5.0f, 5.0f, 85.0f); break;
    case M_OBL_ANGLE_UP:   g_app.obliqueAngleDeg = clampf(g_app.obliqueAngleDeg + 5.0f, 5.0f, 85.0f); break;
    case M_FOV_DOWN: g_app.camera.fovY = clampf(g_app.camera.fovY - 5.0f, 5.0f, 120.0f); break;
    case M_FOV_UP:   g_app.camera.fovY = clampf(g_app.camera.fovY + 5.0f, 5.0f, 120.0f); break;

    // ---- 视图 ----
    case M_VIEW_FRONT:  appSetViewPreset(ViewPreset::Front);  break;
    case M_VIEW_BACK:   appSetViewPreset(ViewPreset::Back);   break;
    case M_VIEW_LEFT:   appSetViewPreset(ViewPreset::Left);   break;
    case M_VIEW_RIGHT:  appSetViewPreset(ViewPreset::Right);  break;
    case M_VIEW_TOP:    appSetViewPreset(ViewPreset::Top);    break;
    case M_VIEW_BOTTOM: appSetViewPreset(ViewPreset::Bottom); break;
    case M_VIEW_FREE:   leavePreset(); break;
    case M_RESET_CAM:   appResetCamera(); break;
    case M_TOGGLE_QUAD: g_app.quadView = !g_app.quadView; break;

    // ---- 渲染 ----
    case M_RENDER_SOLID: g_app.renderMode = RenderMode::Solid;     break;
    case M_RENDER_WIRE:  g_app.renderMode = RenderMode::Wireframe; break;
    case M_RENDER_POINT: g_app.renderMode = RenderMode::Points;    break;
    case M_TOGGLE_CULL:  g_app.cullFace    = !g_app.cullFace;    break;
    case M_TOGGLE_SHADE: g_app.smoothShade = !g_app.smoothShade; break;
    case M_TOGGLE_LIGHT: g_app.lighting    = !g_app.lighting;    break;
    case M_TOGGLE_TEX:   g_app.textureOn   = !g_app.textureOn;   break;

    // ---- 显示 ----
    case M_TOGGLE_GRID:   g_app.showGrid   = !g_app.showGrid;   break;
    case M_TOGGLE_AXES:   g_app.showAxes   = !g_app.showAxes;   break;
    case M_TOGGLE_GROUND: g_app.showGround = !g_app.showGround; break;
    case M_TOGGLE_HUD:    g_app.showHUD    = !g_app.showHUD;    break;

    // ---- 其他 ----
    case M_HELP:       g_app.showHelp = !g_app.showHelp; break;
    case M_SCREENSHOT: appSaveScreenshot("screenshot.bmp"); break;
    case M_QUIT:       glutLeaveMainLoop(); return;

    default: break;
    }

    glutPostRedisplay();
}

// ---------------------------------------------------------------------------
void inputKeyboard(unsigned char key, int /*x*/, int /*y*/)
{
    switch (key)
    {
    case 27:  inputAction(M_QUIT); return;                    // ESC

    // 变换模式（s 让给相机漫游，缩放用 y —— 指导书的建议）
    case 't': inputAction(M_TRANS_T); return;
    case 'r': inputAction(M_TRANS_R); return;
    case 'y': inputAction(M_TRANS_S); return;

    case '0':
        // Shift+0 复位相机，单独按 0 复位物体变换
        if (shiftDown()) inputAction(M_RESET_CAM);
        else             inputAction(M_RESET_OBJ);
        return;

    case '\t': inputAction(M_SELECT_NEXT); return;            // Tab

    // 绘制模式循环
    case '1':
        g_app.renderMode = (g_app.renderMode == RenderMode::Solid)     ? RenderMode::Wireframe
                         : (g_app.renderMode == RenderMode::Wireframe) ? RenderMode::Points
                                                                       : RenderMode::Solid;
        break;

    // 投影
    case '3': inputAction(M_PROJ_PERSP); return;
    case 'o': inputAction(M_PROJ_ORTHO); return;
    case '7': inputAction(M_PROJ_CAV);   return;
    case '9': inputAction(M_PROJ_CAB);   return;
    case '-': inputAction(M_OBL_ANGLE_DOWN); return;
    case '=': inputAction(M_OBL_ANGLE_UP);   return;
    case '[': inputAction(M_FOV_DOWN); return;
    case ']': inputAction(M_FOV_UP);   return;

    // 正平行投影三视图
    case '2': inputAction(M_VIEW_FRONT);  return;
    case '8': inputAction(M_VIEW_BACK);   return;
    case '4': inputAction(M_VIEW_LEFT);   return;
    case '6': inputAction(M_VIEW_RIGHT);  return;
    case '5': inputAction(M_VIEW_TOP);    return;

    case 'v': inputAction(M_TOGGLE_QUAD); return;

    // 空格：播放 / 暂停动画预览。森林场景里默认就是兔子漫步。
    case ' ': appTogglePreview(); return;

    // 相机漫游
    case 'w': leavePreset(); g_app.camera.fly( 0.30f,  0.00f, 0.0f); break;
    case 's': leavePreset(); g_app.camera.fly(-0.30f,  0.00f, 0.0f); break;
    case 'a': leavePreset(); g_app.camera.fly( 0.00f, -0.30f, 0.0f); break;
    case 'd': leavePreset(); g_app.camera.fly( 0.00f,  0.30f, 0.0f); break;
    case 'q': leavePreset(); g_app.camera.fly( 0.00f,  0.00f, -0.30f); break;
    case 'e': leavePreset(); g_app.camera.fly( 0.00f,  0.00f,  0.30f); break;

    // 渲染与显示开关
    case 'c': inputAction(M_TOGGLE_CULL);   return;
    case 'm': inputAction(M_TOGGLE_SHADE);  return;
    case 'l': inputAction(M_TOGGLE_LIGHT);  return;
    case 'T': inputAction(M_TOGGLE_TEX);    return;
    case 'g': inputAction(M_TOGGLE_GRID);   return;
    case 'x': inputAction(M_TOGGLE_AXES);   return;
    case 'p': inputAction(M_TOGGLE_GROUND); return;
    case 'h': inputAction(M_HELP);          return;

    default: return;
    }

    glutPostRedisplay();
}

void inputSpecial(int key, int /*x*/, int /*y*/)
{
    switch (key)
    {
    case GLUT_KEY_F2: g_app.enhanced = !g_app.enhanced; appPostRedisplay(); return;
    case GLUT_KEY_F3: g_app.shadows = !g_app.shadows; appPostRedisplay(); return;
    case GLUT_KEY_F12: inputAction(M_SCREENSHOT); return;
    case GLUT_KEY_F1:  inputAction(M_HELP);       return;

    // 方向键也能平移相机，方便没有中键的鼠标
    case GLUT_KEY_LEFT:  leavePreset(); g_app.camera.fly(0.0f, -0.30f, 0.0f); break;
    case GLUT_KEY_RIGHT: leavePreset(); g_app.camera.fly(0.0f,  0.30f, 0.0f); break;
    case GLUT_KEY_UP:    leavePreset(); g_app.camera.fly(0.30f, 0.0f,  0.0f); break;
    case GLUT_KEY_DOWN:  leavePreset(); g_app.camera.fly(-0.30f, 0.0f, 0.0f); break;
    default: return;
    }
    glutPostRedisplay();
}

// ---------------------------------------------------------------------------
void inputMouseButton(int button, int state, int x, int y)
{
    if (state == GLUT_DOWN)
    {
        g_app.lastX = x;
        g_app.lastY = y;

        if (button == GLUT_LEFT_BUTTON)
        {
            // Shift+左键 = 平移相机（中键留给菜单，右键留给旋转相机）
            g_app.dragMode = shiftDown() ? AppState::DragMode::Pan
                                         : AppState::DragMode::Object;
        }
        else if (button == GLUT_RIGHT_BUTTON)
        {
            g_app.dragMode = AppState::DragMode::Camera;
        }
        else
        {
            g_app.dragMode = AppState::DragMode::None;
        }
    }
    else if (state == GLUT_UP)
    {
        g_app.dragMode = AppState::DragMode::None;
    }
}

void inputMouseMotion(int x, int y)
{
    if (g_app.dragMode == AppState::DragMode::None) return;

    const float dx = (float)(x - g_app.lastX);
    const float dy = (float)(y - g_app.lastY);
    g_app.lastX = x;
    g_app.lastY = y;

    if (dx == 0.0f && dy == 0.0f) return;

    const float winW = (float)std::max(1, g_app.winW);
    const float winH = (float)std::max(1, g_app.winH);

    switch (g_app.dragMode)
    {
    case AppState::DragMode::Object:
    {
        // 同理：预览播放时兔子每帧都被动画摆回原位，手动变换会被立刻覆盖
        appStopPreview();

        float delta[16];
        buildObjectDelta(g_app.transMode, dx, dy, delta);
        appAccumulateObjectTransform(delta);
        break;
    }

    case AppState::DragMode::Camera:
        // 绕参考点旋转视角（实验七的 quatRotate 分支）
        leavePreset();
        g_app.camera.orbit(dy * (180.0f / winH), dx * (180.0f / winW));
        break;

    case AppState::DragMode::Pan:
    {
        leavePreset();
        const float s = g_app.camera.distance() * 2.0f;
        g_app.camera.pan(-dx * (s / winW), dy * (s / winH));
        break;
    }

    default:
        break;
    }

    glutPostRedisplay();
}

void inputMouseWheel(int /*wheel*/, int direction, int /*x*/, int /*y*/)
{
    leavePreset();
    // freeglut: direction > 0 表示向前滚（远离用户）
    g_app.camera.dolly(direction > 0 ? 1.10f : (1.0f / 1.10f));
    glutPostRedisplay();
}

// ---------------------------------------------------------------------------
void inputMenu(int choice)
{
    inputAction(choice);
}

void inputBuildMenu()
{
    // 菜单文字一律用 ASCII：freeglut 的菜单/位图字体只能画 ASCII，
    // 中文会变成空白或乱码。
    const int mTrans = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Translate        (t)",       M_TRANS_T);
    glutAddMenuEntry("Rotate           (r)",       M_TRANS_R);
    glutAddMenuEntry("Scale            (y)",       M_TRANS_S);
    glutAddMenuEntry("Reset object transform (0)", M_RESET_OBJ);
    glutAddMenuEntry("Select next object  (Tab)",  M_SELECT_NEXT);

    const int mProj = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Perspective            (3)", M_PROJ_PERSP);
    glutAddMenuEntry("Orthographic           (o)", M_PROJ_ORTHO);
    glutAddMenuEntry("Oblique cavalier  L=1  (7)", M_PROJ_CAV);
    glutAddMenuEntry("Oblique cabinet   L=0.5(9)", M_PROJ_CAB);
    glutAddMenuEntry("Oblique angle  -       (-)", M_OBL_ANGLE_DOWN);
    glutAddMenuEntry("Oblique angle  +       (=)", M_OBL_ANGLE_UP);
    glutAddMenuEntry("FOV -                  ([)", M_FOV_DOWN);
    glutAddMenuEntry("FOV +                  (])", M_FOV_UP);

    const int mView = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Front   (2)", M_VIEW_FRONT);
    glutAddMenuEntry("Back    (8)", M_VIEW_BACK);
    glutAddMenuEntry("Left    (4)", M_VIEW_LEFT);
    glutAddMenuEntry("Right   (6)", M_VIEW_RIGHT);
    glutAddMenuEntry("Top     (5)", M_VIEW_TOP);
    glutAddMenuEntry("Bottom",      M_VIEW_BOTTOM);
    glutAddMenuEntry("Free view",   M_VIEW_FREE);
    glutAddMenuEntry("Reset camera (Shift+0)", M_RESET_CAM);
    glutAddMenuEntry("4-viewport layout  (v)", M_TOGGLE_QUAD);

    const int mRender = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Solid",                   M_RENDER_SOLID);
    glutAddMenuEntry("Wireframe",               M_RENDER_WIRE);
    glutAddMenuEntry("Points",                  M_RENDER_POINT);
    glutAddMenuEntry("Back-face culling   (c)", M_TOGGLE_CULL);
    glutAddMenuEntry("Smooth/Flat shading (m)", M_TOGGLE_SHADE);
    glutAddMenuEntry("Lighting            (l)", M_TOGGLE_LIGHT);
    glutAddMenuEntry("Texture             (T)", M_TOGGLE_TEX);

    const int mShow = glutCreateMenu(inputMenu);
    glutAddMenuEntry("Grid    (g)", M_TOGGLE_GRID);
    glutAddMenuEntry("Axes    (x)", M_TOGGLE_AXES);
    glutAddMenuEntry("Ground  (p)", M_TOGGLE_GROUND);
    glutAddMenuEntry("HUD",         M_TOGGLE_HUD);

    glutCreateMenu(inputMenu);
    glutAddSubMenu("Transform",   mTrans);
    glutAddSubMenu("Projection",  mProj);
    glutAddSubMenu("View",        mView);
    glutAddSubMenu("Render",      mRender);
    glutAddSubMenu("Show",        mShow);
    glutAddMenuEntry("Save screenshot (F12)", M_SCREENSHOT);
    glutAddMenuEntry("Toggle help (h)",       M_HELP);
    glutAddMenuEntry("Quit (ESC)",            M_QUIT);

    // 指导书原话：右键拖动用来旋转相机，所以菜单绑到鼠标中键
    glutAttachMenu(GLUT_MIDDLE_BUTTON);
}
```

### 39. `src/app.h`

程序状态接口（114 行）

```cpp
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
```

### 40. `src/app.cpp`

渲染主流程、HUD、截图、四视口、录制调度（741 行）

```cpp
#include "app.h"
#include "hud.h"
#include "lighting.h"
#include "projection.h"
#include "recorder.h"
#include "renderer.h"

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
    hudTextf(16,H-41,"%s  |  %s  |  %d triangles  |  %s",
             selected?selected->name.c_str():"No selection",projModeName(g_app.projMode),triangles,
             g_app.transMode==TransMode::Translate?"TRANSLATE":g_app.transMode==TransMode::Rotate?"ROTATE":"SCALE");
    hudPanel(0,0,W,30,0.82f);
    hudColor(0.72f,0.80f,0.87f);
    hudText(16,10,"H Help   F2 Renderer   F3 Shadows   Space Animate   Tab Select   F12 Screenshot");
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
```

### 41. `src/main.cpp`

入口：GLUT 初始化、命令行解析、回调注册、自检（403 行）

```cpp
// 三维模型查看器 —— 计算机图形学总结实验
//
//   三维场景绘制：实验四的立方体 + GLUT 图元 + 实验十的 OBJ 模型导入
//   几何变换    ：实验五/六的 objectXform 累积变换 + 鼠标键盘交互
//   观察变换    ：实验七的 gluLookAt 三视图 + 相机漫游 + 斜平行投影
//   要求 3      ：全投影模式保宽高比 reshape，以及干净退出
//
// 构建见同目录 CMakeLists.txt；操作说明见 README.md。
//
// 命令行（写实验报告时可以批量出图）：
//   viewer.exe [model.obj] [--shot out.bmp] [--proj persp|ortho|cav|cab]
//              [--view front|back|left|right|top|bottom|free]
//              [--render solid|wire|points] [--quad] [--no-hud]
#include "app.h"
#include "input.h"

#include <GL/freeglut.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>

namespace {

bool fileExists(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

std::string dirOf(const std::string& path)
{
    const size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? std::string() : path.substr(0, pos + 1);
}

std::string parentDir(const std::string& dir)
{
    if (dir.size() < 2) return std::string();
    std::string d = dir;
    while (d.size() > 1 && (d.back() == '/' || d.back() == '\\')) d.pop_back();
    const size_t pos = d.find_last_of("/\\");
    return (pos == std::string::npos) ? std::string() : d.substr(0, pos + 1);
}

// 可执行文件在 build/Release/ 下，而模型在仓库根的 Models/ 下。
// 从 exe 所在目录逐级向上找，这样从 VS 里启动、从命令行启动、双击启动都能找到。
std::string findDefaultModel()
{
    char exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
        return std::string();

    const std::string rel = "Models/bunny_10k.obj";   // 默认模型（Stanford Bunny 10k）
    std::string dir = dirOf(exePath);

    for (int i = 0; i < 6 && !dir.empty(); ++i)
    {
        const std::string cand = dir + rel;
        if (fileExists(cand)) return cand;
        dir = parentDir(dir);
    }
    return std::string();
}

// ---- 命令行 ----
struct Options
{
    std::string modelPath;
    std::string shotPath;
    std::string proj, view, render;
    int  winW = 1280, winH = 800;
    bool classic = false, noShadows = false;
    bool quad     = false;
    bool noHud    = false;
    bool showHelp = false;

    std::string upAxis;        // --up y|z|auto，空表示 auto
    std::string sceneName;     // --scene studio|forest

    // 视频录制
    std::string recordPath;
    std::string animName;      // 空 -> 用默认动画 turntable
    float       seconds = 6.0f;
    int         fps     = 30;
};

Options parseArgs(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&](std::string& dst) { if (i + 1 < argc) dst = argv[++i]; };
        if      (a == "--shot")   next(o.shotPath);
        else if (a == "--proj")   next(o.proj);
        else if (a == "--view")   next(o.view);
        else if (a == "--render") next(o.render);
        else if (a == "--classic") o.classic = true;
        else if (a == "--no-shadows") o.noShadows = true;
        else if (a == "--quad")   o.quad  = true;
        else if (a == "--no-hud") o.noHud = true;
        else if (a == "--show-help") o.showHelp = true;
        else if (a == "--up")      next(o.upAxis);
        else if (a == "--scene")   next(o.sceneName);
        else if (a == "--record")  next(o.recordPath);
        else if (a == "--anim")    next(o.animName);
        else if (a == "--fps")
        {
            std::string s; next(s);
            const int v = atoi(s.c_str());
            if (v > 0 && v <= 240) o.fps = v;
        }
        else if (a == "--seconds")
        {
            std::string s; next(s);
            const double v = atof(s.c_str());
            if (v > 0.05 && v <= 600.0) o.seconds = (float)v;
        }
        else if (a == "--size")
        {
            // --size 1600x300 —— 用来验证「窗口变化图形不变形」
            std::string s; next(s);
            const size_t x = s.find_first_of("xX*");
            if (x != std::string::npos)
            {
                const int w = atoi(s.substr(0, x).c_str());
                const int h = atoi(s.substr(x + 1).c_str());
                if (w > 0 && h > 0) { o.winW = w; o.winH = h; }
            }
        }
        else if (!a.empty() && a[0] != '-') o.modelPath = a;
    }
    if (o.modelPath.empty()) o.modelPath = findDefaultModel();
    return o;
}

// --shot 模式下窗口会被隐藏：截图从后缓冲读，不需要窗口真的可见，
// 这样批处理出图不会满屏闪窗口。

// ---------------------------------------------------------------------------
// 自检：交互逻辑（变换矩阵、四元数旋转、相机漫游）没法靠截图验证，
// 这里用断言把它们跑一遍。`viewer.exe --selftest`，不需要窗口。
// ---------------------------------------------------------------------------
int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failures;
}

bool nearf(float a, float b, float eps = 1e-2f) { return std::fabs(a - b) < eps; }

int runSelfTest()
{
    std::printf("=== self-test: math / camera logic ===\n");

    {
        float I[16]; mat4Identity(I);
        float A[16]; mat4FromTranslation(A, 1.0f, 2.0f, 3.0f);
        float R[16]; mat4Multiply(R, I, A);
        check(nearf(R[12], 1) && nearf(R[13], 2) && nearf(R[14], 3),
              "mat4Multiply with identity is a no-op");
    }
    {
        // 右手系：+X 绕 +Y 转 90 度应落到 -Z
        float M[16]; mat4FromAxisAngle(M, 90.0f, 0.0f, 1.0f, 0.0f);
        const Vec3 p = mat4MulDir(M, v3(1, 0, 0));
        check(nearf(p.x, 0) && nearf(p.y, 0) && nearf(p.z, -1),
              "mat4FromAxisAngle: +Y 90deg maps +X to -Z");
    }
    {
        float x = 1, y = 0, z = 0;
        quatRotate(3.14159265f * 0.5f, 0.0f, 1.0f, 0.0f, x, y, z);
        check(nearf(x, 0) && nearf(y, 0) && nearf(z, -1),
              "quatRotate: +Y 90deg maps +X to -Z (same as matrix form)");
    }
    {
        Camera c; c.reset(1.5f);
        const float d0 = c.distance();
        c.orbit(17.0f, -23.0f);
        check(nearf(c.distance(), d0), "Camera::orbit preserves distance to target");
    }
    {
        Camera c; c.reset(1.5f);
        const Vec3 e0 = c.eye;
        c.orbit(360.0f, 0.0f);
        check(nearf(c.eye.x, e0.x) && nearf(c.eye.y, e0.y) && nearf(c.eye.z, e0.z),
              "Camera::orbit 360deg returns to the start position");
    }
    {
        Camera c; c.reset(1.5f);
        const float d0 = c.distance();
        c.dolly(0.5f);
        check(nearf(c.distance(), d0 * 0.5f), "Camera::dolly(0.5) halves the distance");
    }
    {
        Camera c; c.reset(1.5f);
        c.setPreset((int)ViewPreset::Top, 1.5f);
        // 顶视图的 up 必须偏离视线方向，否则 gluLookAt 的基退化（叉积为零、画面空白）
        check(nearf(c.up.z, -1.0f), "Top preset uses up=(0,0,-1)");
        check(v3len(c.right()) > 0.9f, "Top preset: right vector is well-defined (not degenerate)");
    }
    {
        Camera c; c.reset(1.5f);
        float d = -1.0f;
        bool same = true;
        for (int i = (int)ViewPreset::Front; i <= (int)ViewPreset::Bottom; ++i)
        {
            c.setPreset(i, 1.5f);
            if (d < 0) d = c.distance();
            else if (!nearf(c.distance(), d)) same = false;
        }
        check(same, "all six view presets share one camera distance (common 三视图 scale)");
    }
    {
        Camera c; c.reset(1.5f);
        const Vec3 e0 = c.eye, f0 = c.forward();
        const float d0 = c.distance();
        c.fly(1.0f, 0.0f, 0.0f);
        const Vec3 moved = c.eye - e0;
        check(v3dot(v3norm(moved), f0) > 0.99f,
              "Camera::fly(+fwd) moves the eye along the view direction");
        check(nearf(c.distance(), d0),
              "Camera::fly translates (preserves distance) rather than dollying");
    }

    std::printf("=== %s (%d failure(s)) ===\n", g_failures ? "FAILED" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}

} // namespace

int main(int argc, char** argv)
{
    // 自检不需要窗口，在 glutInit 之前就返回
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--selftest")
            return runSelfTest();

    const Options opt = parseArgs(argc, argv);

    std::printf("=== 3D Model Viewer ===\n");
    if (opt.modelPath.empty())
        std::printf("[main] no OBJ model found; built-in primitives only.\n"
                    "       usage: viewer.exe [path\\to\\model.obj]\n");
    else
        std::printf("[main] model: %s\n", opt.modelPath.c_str());

    glutInit(&argc, argv);
    // GLUT_MULTISAMPLE 若当前像素格式不支持，freeglut 会自动降级
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(opt.winW, opt.winH);
    glutInitWindowPosition(60, 40);
    glutCreateWindow("3D Model Viewer");

    // 要求 3：点窗口 X 关闭时，让 glutMainLoop 正常返回，而不是从回调里 exit()。
    // 默认的 GLUT_ACTION_EXIT 会跳过后面所有清理代码。
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    if      (opt.upAxis == "y" || opt.upAxis == "Y") g_app.modelUp = UpAxis::Y;
    else if (opt.upAxis == "z" || opt.upAxis == "Z") g_app.modelUp = UpAxis::Z;
    else                                             g_app.modelUp = UpAxis::Auto;

    // 默认是森林（见 AppState::sceneKind），所以 --scene 要正反两个方向都能切。
    // 只判断 "forest" 的话，默认改成森林之后 --scene studio 就落空了。
    if (opt.sceneName.empty() || opt.sceneName == "forest")
    {
        g_app.sceneKind = SceneKind::Forest;
        g_app.fog       = true;
    }
    else if (opt.sceneName == "studio")
    {
        g_app.sceneKind = SceneKind::Studio;
        g_app.fog       = false;
    }
    else
    {
        std::printf("[main] unknown --scene '%s' (expected: forest | studio)\n",
                    opt.sceneName.c_str());
        return 1;
    }

    g_app.enhanced = !opt.classic;
    g_app.shadows = !opt.noShadows;
    appSetup(opt.modelPath);
    glutCloseFunc(appCleanup);

    // ---- 命令行覆盖（必须在 glutCreateWindow 之后，appUpdateTitle 需要当前窗口）----
    if      (opt.proj == "persp")                          appSetProjection(ProjMode::Perspective);
    else if (opt.proj == "ortho")                          appSetProjection(ProjMode::Ortho);
    else if (opt.proj == "cav" || opt.proj == "cavalier")  appSetProjection(ProjMode::Cavalier);
    else if (opt.proj == "cab" || opt.proj == "cabinet")   appSetProjection(ProjMode::Cabinet);

    if      (opt.view == "front")  appSetViewPreset(ViewPreset::Front);
    else if (opt.view == "back")   appSetViewPreset(ViewPreset::Back);
    else if (opt.view == "left")   appSetViewPreset(ViewPreset::Left);
    else if (opt.view == "right")  appSetViewPreset(ViewPreset::Right);
    else if (opt.view == "top")    appSetViewPreset(ViewPreset::Top);
    else if (opt.view == "bottom") appSetViewPreset(ViewPreset::Bottom);

    if      (opt.render == "solid")  g_app.renderMode = RenderMode::Solid;
    else if (opt.render == "wire")   g_app.renderMode = RenderMode::Wireframe;
    else if (opt.render == "points") g_app.renderMode = RenderMode::Points;

    if (opt.quad)     g_app.quadView = true;
    if (opt.noHud)    g_app.showHUD  = false;
    if (opt.showHelp) g_app.showHelp = true;

    glutDisplayFunc(appDisplay);
    glutReshapeFunc(appReshape);
    glutKeyboardFunc(inputKeyboard);
    glutSpecialFunc(inputSpecial);
    glutMouseFunc(inputMouseButton);
    glutMotionFunc(inputMouseMotion);
    glutMouseWheelFunc(inputMouseWheel);

    inputBuildMenu();
    appUpdateTitle();

    if (!opt.shotPath.empty())
    {
        // 直接画一帧再截图，**不进主循环**：freeglut 对隐藏窗口不派发 display 事件，
        // 用 glutMainLoop 会一直等下去（实测会挂死）。GL 上下文在 glutCreateWindow
        // 之后就已经是当前的，直接调显示函数渲染 + 从后缓冲读取即可。
        glutHideWindow();                        // 批处理出图时不闪窗口
        // 隐藏窗口永远收不到 reshape 事件（glutReshapeFunc 不会触发），必须显式灌尺寸，
        // 否则 --size 会被静默忽略、winW/winH 停在默认 1280x800。
        //
        // 而且要用 glutGet 问窗口的**真实**尺寸而不是请求值：Windows 会把窗口尺寸
        // 截断到屏幕大小（例如请求 300x1600 实际只有 300x1080），按请求值截图会让
        // glReadPixels 读到 framebuffer 之外，图顶多出一条黑边；aspect 也会算错。
        const int realW = glutGet(GLUT_WINDOW_WIDTH);
        const int realH = glutGet(GLUT_WINDOW_HEIGHT);
        appReshape(realW, realH);
        appRequestShot(opt.shotPath.c_str());
        std::printf("[main] headless shot mode -> %s  (requested %dx%d, actual %dx%d)\n",
                    opt.shotPath.c_str(), opt.winW, opt.winH, realW, realH);
        appDisplay();                            // 内部：画完 -> 截图 -> 退出主循环
        appCleanup();
        return appExitCode();
    }

    // ---- 视频录制 ----
    // 走 glutMainLoop 而不是像 --shot 那样直接调 appDisplay：录制要靠
    // glutPostRedisplay 逐帧驱动，需要一个真正的主循环。
    if (!opt.recordPath.empty())
    {
        RecordOptions ro;
        ro.path    = opt.recordPath;
        ro.fps     = opt.fps;
        ro.seconds = opt.seconds;

        if (opt.animName.empty())
        {
            ro.anim = AnimKind::Turntable;      // 默认：模型自转
        }
        else if (!parseAnimKind(opt.animName, ro.anim))
        {
            std::printf("[main] unknown --anim '%s'; falling back to turntable\n"
                        "       valid: turntable | orbit | projections | transforms\n",
                        opt.animName.c_str());
            ro.anim = AnimKind::Turntable;
        }

        // Offline rendering must not depend on visibility-driven GLUT events.
        glutHideWindow();
        appReshape(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
        appStartRecording(ro);
        while (appRecordingActive()) appDisplay();
        appCleanup();
        return appExitCode();
    }
    else if (!opt.animName.empty())
    {
        // 只给 --anim 不给 --record：以前是静默忽略，现在当作实时预览播放
        AnimKind k = AnimKind::None;
        if (parseAnimKind(opt.animName, k) && k != AnimKind::None)
            appStartPreview(k, opt.seconds);
        else
            std::printf("[main] unknown --anim '%s'\n"
                        "       valid: turntable | orbit | projections | transforms | roam\n",
                        opt.animName.c_str());
    }

    std::printf("[main] entering main loop  (h = help, ESC = quit)\n");
    glutMainLoop();

    // 走到这里说明是正常退出（ESC 或关闭窗口），清理代码会真正执行
    appCleanup();
    std::printf("[main] clean exit\n");
    return appExitCode();
}
```

### 42. `tools/bmp2png.py`

截图 BMP → PNG（纯标准库）（74 行）

```python
#!/usr/bin/env python3
"""把查看器导出的 24 位 BMP 转成 PNG（写实验报告时用）。

只依赖标准库：BMP 是未压缩的 24 位，PNG 用 zlib 手写三个 chunk 即可。
用法: python tools/bmp2png.py shot.bmp [shot.png]
"""
import struct
import sys
import zlib


def read_bmp(path):
    with open(path, "rb") as f:
        data = f.read()

    if data[:2] != b"BM":
        raise ValueError("not a BMP file")

    pix_off = struct.unpack_from("<I", data, 10)[0]
    hdr_size = struct.unpack_from("<I", data, 14)[0]
    if hdr_size < 40:
        raise ValueError("unsupported BMP header (need BITMAPINFOHEADER)")

    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp != 24:
        raise ValueError("only 24-bit BMP supported, got %d" % bpp)

    bottom_up = h > 0
    h = abs(h)
    stride = (w * 3 + 3) & ~3

    # 统一转成自上而下的 RGB 行
    rows = []
    for y in range(h):
        src_y = (h - 1 - y) if bottom_up else y
        off = pix_off + src_y * stride
        row = bytearray(w * 3)
        for x in range(w):
            b, g, r = data[off + x * 3], data[off + x * 3 + 1], data[off + x * 3 + 2]
            row[x * 3], row[x * 3 + 1], row[x * 3 + 2] = r, g, b
        rows.append(bytes(row))
    return w, h, rows


def _chunk(tag, payload):
    return (struct.pack(">I", len(payload)) + tag + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))


def write_png(path, w, h, rows):
    raw = b"".join(b"\x00" + r for r in rows)          # 每行前置 filter 类型 0
    out = (b"\x89PNG\r\n\x1a\n"
           + _chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + _chunk(b"IDAT", zlib.compress(raw, 6))
           + _chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(out)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else src.rsplit(".", 1)[0] + ".png"
    w, h, rows = read_bmp(src)
    write_png(dst, w, h, rows)
    print("%s (%dx%d) -> %s" % (src, w, h, dst))
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

### 43. `tools/make_shots.sh`

一键重新生成 shots/ 全部截图（54 行）

```bash
#!/usr/bin/env bash
# 重新生成 shots/ 下实验报告用的全部截图。
# 用法:  bash tools/make_shots.sh
#
# 单帧截图模式会隐藏窗口、从后缓冲读像素，所以批处理不会满屏闪窗口。
#
# 注意：程序默认进的是森林场景，而报告的前 14 张图要的是展示台
# （实验四立方体 / 茶壶 / 球那一套），所以这里每张都显式写 --scene。
set -e
cd "$(dirname "$0")/.."

V=./build/Release/viewer.exe
[ -x "$V" ] || { echo "先构建: 见 README「二、构建」"; exit 1; }

_studio() { "$V" --shot "shots/$1.bmp" --scene studio "${@:2}" >/dev/null 2>&1; }
_forest() { "$V" --shot "shots/$1.bmp" --scene forest "${@:2}" >/dev/null 2>&1; }

emit() {                       # emit <名字> <生成函数> [参数...]
    local name="$1"; shift
    local fn="$1";   shift     # 必须先把函数名取出来再 shift，
                               # 否则 "$@" 里还带着函数名，会被当成第一个参数重复传一遍
    "$fn" "$name" "$@"
    python -X utf8 tools/bmp2png.py "shots/$name.bmp" "shots/$name.png" >/dev/null
    echo "  shots/$name.png"
}

echo "生成截图..."

# --- 展示台场景：投影与绘制模式 ---
emit 01_perspective   _studio --proj persp                    --no-hud
emit 02_front         _studio --proj ortho --view front       --no-hud
emit 03_top           _studio --proj ortho --view top         --no-hud
emit 04_left          _studio --proj ortho --view left        --no-hud
emit 05_cavalier      _studio --proj cav   --view front       --no-hud
emit 06_cabinet       _studio --proj cab   --view front       --no-hud
emit 07_wireframe     _studio --proj persp --render wire      --no-hud
emit 08_points        _studio --proj persp --render points    --no-hud

# --- 四视口 / 帮助界面（保留 HUD）---
emit 09_quadview      _studio --quad
emit 10_help          _studio --show-help

# --- 窗口变化图形不变形：四个极端长宽比 ---
emit 11_wide_1600x300 _studio --proj persp --size 1600x300          --no-hud
emit 12_tall_300x1000 _studio --proj persp --size 300x1000          --no-hud
emit 13_ortho_wide    _studio --proj ortho --view front --size 1600x300 --no-hud
emit 14_ortho_tall    _studio --proj ortho --view front --size 300x1000 --no-hud

# --- 森林场景（程序默认就是它）---
emit 15_forest        _forest                       --no-hud
emit 16_forest_top    _forest --proj ortho --view top --no-hud

rm -f shots/*.bmp
echo "完成。"
```

### 44. `tools/make_package.py`

本脚本：生成这份代码文档（205 行）

```python
#!/usr/bin/env python3
"""把全部源代码打包成一份 Markdown 文件（方便贴进实验报告附录或作为单一文件提交）。

用法:  python tools/make_package.py [输出路径]
默认输出: CG-Experiment-code.md
"""
import io
import os
import sys

# 按「自底向上」的阅读顺序排列，而不是字母序：
# 先公共类型和数学工具，再数据层（解析/网格/纹理），再渲染层，最后主流程。
FILES = [
    ("src/gl_api.h", "cpp", "OpenGL runtime entry points"),
    ("src/shaders.h", "cpp", "GLSL per-pixel lighting and PCF shadows"),
    ("src/renderer.h", "cpp", "Renderer interface"),
    ("src/renderer.cpp", "cpp", "Shadow pass and forward shading"),
    ("tests/obj_loader_test.cpp", "cpp", "OBJ regression tests"),
    ("tools/render_smoke.py", "python", "GPU rendering regression checks"),
    ("CMakeLists.txt",        "cmake",  "构建配置（含 32 位防护）"),
    ("CMakePresets.json",     "json",   "CMake 预设：只提供 x64，避免选错"),
    ("build.bat",             "bat",    "一键构建（Windows cmd / PowerShell）"),
    ("build.sh",              "bash",   "一键构建（git-bash / MSYS2）"),
    ("viewer.vcxproj",        "xml",    "经典 VS 工程（自包含，双击 viewer.sln 即可）"),
    (".gitignore",            "text",   "Git 忽略规则"),

    ("src/types.h",           "cpp",    "全局模式枚举"),

    ("src/math3d.h",          "cpp",    "向量 / 矩阵 / 四元数工具（header-only）"),

    ("src/obj_loader.h",      "cpp",    "OBJ 导入接口（实验十）"),
    ("src/obj_loader.cpp",    "cpp",    "OBJ / MTL 解析：四边形三角化 + 顶点去重"),

    ("src/mesh.h",            "cpp",    "网格接口"),
    ("src/mesh.cpp",          "cpp",    "网格：归一化、上轴判别、客户端顶点数组绘制"),

    ("src/texture.h",         "cpp",    "纹理接口"),
    ("src/texture.cpp",       "cpp",    "stb_image → OpenGL 纹理"),

    ("src/camera.h",          "cpp",    "观察变换相机接口"),
    ("src/camera.cpp",        "cpp",    "相机：环绕 / 漫游 / 四元数旋转 / 退化保护（实验七）"),

    ("src/projection.h",      "cpp",    "投影接口"),
    ("src/projection.cpp",    "cpp",    "透视 / 正交 / 斜平行投影 + 保宽高比"),

    ("src/lighting.h",        "cpp",    "光照接口"),
    ("src/lighting.cpp",      "cpp",    "光照与材质（展示台点光源 / 森林太阳光）"),

    ("src/forest.h",          "cpp",    "程序化森林接口"),
    ("src/forest.cpp",        "cpp",    "森林：地面 + 三种树型，烘焙成一个顶点色网格"),

    ("src/scene.h",           "cpp",    "场景与物体接口"),
    ("src/scene.cpp",         "cpp",    "两套场景：展示台（实验四/五）与森林"),

    ("src/anim.h",            "cpp",    "脚本动画接口"),
    ("src/anim.cpp",          "cpp",    "动画（时间的纯函数）：自转/环绕/投影/变换/森林漫步"),

    ("src/recorder.h",        "cpp",    "录制接口"),
    ("src/recorder.cpp",      "cpp",    "逐帧数据管道进 ffmpeg 编码 mp4"),

    ("src/hud.h",             "cpp",    "叠加文字接口"),
    ("src/hud.cpp",           "cpp",    "屏幕叠加文字与帮助面板"),

    ("src/input.h",           "cpp",    "输入接口"),
    ("src/input.cpp",         "cpp",    "键盘 / 鼠标 / 滚轮 / 右键菜单（实验六）"),

    ("src/app.h",             "cpp",    "程序状态接口"),
    ("src/app.cpp",           "cpp",    "渲染主流程、HUD、截图、四视口、录制调度"),

    ("src/main.cpp",          "cpp",    "入口：GLUT 初始化、命令行解析、回调注册、自检"),

    ("tools/bmp2png.py",      "python", "截图 BMP → PNG（纯标准库）"),
    ("tools/make_shots.sh",   "bash",   "一键重新生成 shots/ 全部截图"),
    ("tools/make_package.py", "python", "本脚本：生成这份代码文档"),
]


# 完整包：两条构建路径都有（VS 工程 + CMake/脚本）
ZIP_ITEMS = [
    "src", "third_party", "tools", "tests", "Models", "shots", "video",
    "CMakeLists.txt", "CMakePresets.json", "build.bat", "build.sh",
    "viewer.sln", "viewer.vcxproj", "viewer.vcxproj.filters",
    "README.md", "CG-Experiment-code.md", ".gitignore",
]

# 纯 VS 包：去掉 CMake 那套工具链，只留「双击 viewer.sln → F5」需要的东西。
# 注意 viewer.vcxproj 里已经不引用 CMakeLists.txt 了，所以这个包里不会出现失效项。
ZIP_ITEMS_VS = [
    "src", "third_party", "tools", "tests", "Models", "shots", "video",
    "viewer.sln", "viewer.vcxproj", "viewer.vcxproj.filters",
    "README.md", "CG-Experiment-code.md",
]


def build_zip(root, out_path, items=None):
    """用 Python 的 zipfile 而不是 PowerShell 的 Compress-Archive。

    后者在 Windows PowerShell 5.1 上会把路径分隔符写成反斜杠（`src\\app.h`），
    而 zip 规范要求正斜杠 —— Linux/macOS 的 unzip 会把这些文件解成一个名字里
    带反斜杠的怪文件。zipfile 永远写 `/`。
    """
    import zipfile
    count = 0
    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for item in (items if items is not None else ZIP_ITEMS):
            full = os.path.join(root, item.replace("/", os.sep))
            if os.path.isfile(full):
                z.write(full, item)
                count += 1
            elif os.path.isdir(full):
                for dirpath, dirnames, filenames in os.walk(full):
                    dirnames.sort()
                    for fn in sorted(filenames):
                        p = os.path.join(dirpath, fn)
                        rel = os.path.relpath(p, root).replace(os.sep, "/")
                        z.write(p, rel)
                        count += 1
    return count


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "CG-Experiment-code.md")

    present = []
    total_lines = 0
    for rel, lang, desc in FILES:
        full = os.path.join(root, rel.replace("/", os.sep))
        if not os.path.exists(full):
            print("  跳过（不存在）: %s" % rel)
            continue
        with io.open(full, encoding="utf-8") as f:
            text = f.read()
        lines = text.count("\n") + (0 if text.endswith("\n") else 1)
        total_lines += lines
        present.append((rel, lang, desc, text, lines))

    readme = ""
    readme_path = os.path.join(root, "README.md")
    if os.path.exists(readme_path):
        with io.open(readme_path, encoding="utf-8") as f:
            readme = f.read()

    w = io.open(out_path, "w", encoding="utf-8", newline="\n")

    w.write("# 三维模型查看器 —— 完整源代码\n\n")
    w.write("计算机图形学总结实验（实验四 / 五 / 六 / 七 / 十 + 斜平行投影 + 程序化森林）\n\n")
    w.write("用 **OpenGL + GLSL** 实现逐像素光照与 PCF 阴影，保留 GLUT 与经典管线对照。\n\n")
    w.write("---\n\n")

    w.write("## 统计\n\n")
    w.write("| 项 | 值 |\n|---|---|\n")
    w.write("| 源文件 | %d 个 |\n" % len(present))
    w.write("| 代码总行数 | %d 行（不含本页的 README 部分）| \n" % total_lines)
    w.write("| 模型 | `Models/bunny_10k.obj`（Stanford Bunny，10000 面，Y-up）|\n")
    w.write("| 依赖 | 已 vendor：freeglut（x64）+ stb_image v2.30，**离线可构建** |\n\n")

    w.write("## 构建与运行\n\n")
    w.write("```bash\n")
    w.write('CM="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/'
            'CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"\n')
    w.write('"$CM" -S . -B build -G "Visual Studio 17 2022" -A x64\n')
    w.write('"$CM" --build build --config Release\n')
    w.write("./build/Release/viewer.exe\n")
    w.write("```\n\n")
    w.write("> CMake 不在 PATH 里，要用 VS 自带的那份；`freeglut.dll` 构建时会自动拷到 exe 旁。\n\n")

    w.write("## 文件清单\n\n")
    w.write("| # | 文件 | 行数 | 说明 |\n|---|---|---|---|\n")
    for i, (rel, _lang, desc, _text, lines) in enumerate(present, 1):
        w.write("| %d | `%s` | %d | %s |\n" % (i, rel, lines, desc))
    w.write("\n---\n\n")

    w.write("## 源代码\n\n")
    for i, (rel, lang, desc, text, lines) in enumerate(present, 1):
        w.write("### %d. `%s`\n\n" % (i, rel))
        w.write("%s（%d 行）\n\n" % (desc, lines))
        w.write("```%s\n" % lang)
        body = text if text.endswith("\n") else text + "\n"
        w.write(body)
        w.write("```\n\n")

    if readme.strip():
        w.write("---\n\n")
        w.write("## 附：README.md\n\n")
        w.write(readme if readme.endswith("\n") else readme + "\n")

    w.close()
    size = os.path.getsize(out_path)
    print("已生成: %s" % out_path)
    print("  %d 个文件, %d 行代码, %.1f KB" % (len(present), total_lines, size / 1024.0))

    for fname, items, note in [
        ("CG-Experiment.zip",    ZIP_ITEMS,    "完整包（VS 工程 + CMake/脚本两条路径）"),
        ("CG-Experiment-vs.zip", ZIP_ITEMS_VS, "纯 VS 包（只要 viewer.sln，无 CMake）"),
    ]:
        zp = os.path.join(root, fname)
        n = build_zip(root, zp, items)
        print("已生成: %s" % zp)
        print("  %s" % note)
        print("  %d 个条目, %.1f MB" % (n, os.path.getsize(zp) / 1024.0 / 1024.0))


if __name__ == "__main__":
    main()
```

---

## 附：README.md

# 三维模型查看器 —— 计算机图形学总结实验

用 OpenGL + GLUT 实现的三维模型查看器，默认使用 GLSL 逐像素光照和阴影，保留固定管线对照模式。覆盖实验四（立方体绘制）、实验五、六
（几何变换与用户交互）、实验七（观察变换）、实验十（导入 OBJ 模型），并额外实现了
斜平行投影、四视口三视图，以及一个程序化生成的森林场景。

**双击运行就进森林**：一只兔子站在林间空地上，左键拖动可做几何变换，WASD / 右键拖动
可移动相机，**空格**让兔子蹦跳漫步。实验四那套展示台（立方体 / 茶壶 / 球）用
`--scene studio` 进。

## 2026-10-14 渲染与结构重构

直接启动 `build/Release/viewer.exe`，或打开 `viewer.sln` 后选择 Release / x64。

- **F2**：增强 GLSL / 经典固定管线对比；命令行 `--classic`。
- **F3**：开关动态阴影；命令行 `--no-shadows`。
- 增强模式：逐像素 Blinn–Phong 光照、天空/地面半球环境光、线性色彩运算、色调映射、逐像素雾和 2048² PCF 阴影。
- 网格使用 VBO/EBO，几何仅首次绘制上传；多视口和阴影通道复用 GPU 数据。
- 修复地面色块接缝、立方体法线、雾颜色数组越界、HUD 状态污染与四视口叠加位置、森林相机复位。
- 离线录制显式逐帧驱动，隐藏窗口也能输出视频；编码或写帧失败返回非零退出码。
- OBJ 缺失法线自动生成，非法面索引明确报错；保留原有投影、变换、录制和模型上轴功能。

![新版森林](shots/refactor/after.png)

![新版展示台](shots/refactor/studio.png)

渲染模块在 `src/renderer.cpp`，GLSL 在 `src/shaders.h`（内嵌以支持任意工作目录启动），
OpenGL 扩展入口在 `src/gl_api.h`，网格 GPU 资源在 `src/mesh.cpp`。
这是**兼容上下文中的可编程着色路径**，仍使用课程要求的矩阵栈和 GLUT 图元，并非 OpenGL Core Profile 全量迁移。
硬件缺少入口或 shader 编译失败时回退经典模式；仅阴影 FBO 不可用时保留 GLSL 无阴影绘制，日志记录原因。

验证命令（CMake / CTest 不在 PATH 时，使用下文的 VS 自带完整路径）：

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/render_smoke.py
```

`tests/obj_loader_test.cpp` 覆盖缺失/混合法线、负索引、空白与注释、非法索引、空模型和四边形。
`tools/render_smoke.py` 运行 12 组真实 OpenGL 出图，检查返回码、GL 错误、shader 初始化与图像内容，
结果及截图在 `shots/refactor/`。窗口实际宽度受屏幕限制，例如本机请求 1600×300 得到 1540×300。
测试不以 FPS 承诺性能收益；VBO 减少重复上传，而阴影新增一次场景绘制和采样成本。

已知边界：仍为单材质 OBJ 导入器，凹多边形的扇形三角化不保证正确；
阴影使用固定世界范围，超出范围不投影，不是级联阴影；透明材质/PBR/SSAO 尚未实现。
森林地面与树木仍合成同一场景网格，`p` 地面开关主要用于展示台。
`m` 在增强模式切换光照法线的平滑/面法线；精确演示固定管线颜色 flat/smooth 请先 F2 切回经典模式。

原始源码与构建文件备份位于 `backups/before-renderer-refactor-20261014/`。
下方为课程功能及原有操作说明；其中固定管线实现说明对应经典模式。

---

![透视图](shots/01_perspective.png)

---

## 一、对应打分点

| 打分点 | 实现 | 截图 |
|---|---|---|
| 实验四立方体绘制（基准分） | 8 顶点 / 6 面 / 顶点色，`GL_SMOOTH` 双线性插值出「五彩斑斓的立方体」 | 下图中右下角的彩色立方体 |
| 场景包含多个三维物体（加分） | 兔子(OBJ) + 彩色立方体 + 茶壶 + 球 + 地面平台 + 网格 + 坐标轴；另有一套程序化森林场景 | `shots/01_perspective.png` `shots/15_forest.png` |
| 导入 3D 模型数据文件（加分） | `obj_loader.cpp` 解析 `v/vt/vn/f/mtllib/usemtl`（含四边形扇形三角化与 `(v,vt,vn)` 三元组去重）；自动判别模型上轴 | `shots/01_perspective.png` |
| 实验五几何变换（基准分） | `objectXform` 累积变换矩阵，`glPushMatrix` / `glMultMatrixf` / `glGetFloatv` | — |
| 交互式平移/旋转/缩放（加分） | `TransMode` + 鼠标拖动 + `t`/`r`/`y` 切换 | — |
| 正平行投影三视图 + 透视投影（基准分） | 六个正投影视图（前/后/左/右/顶/仰），**六个视图共用同一缩放** | `shots/09_quadview.png` |
| 相机漫游（加分） | WASD 前后左右、QE 升降、右键拖动 `quatRotate` 旋转视角、滚轮推拉 | — |
| 斜平行投影（加分） | 斜等测 cavalier(L=1) / 斜二测 cabinet(L=0.5) | `shots/05_cavalier.png` |
| 窗口变化图形不变形 | 透视 / 正交 / 斜平行 / 四视口各自按 aspect 修正投影 | `shots/11_wide_1600x300.png` |
| 程序正常退出 | `GLUT_ACTION_GLUTMAINLOOP_RETURNS` + `glutLeaveMainLoop` + `cleanup()` | — |
| 程序化动画（额外） | 森林场景里兔子蹦跳漫步：抛物线弧 + 落地压缩/腾空拉伸 + 转弯朝向 | `video/forest_roam.mp4` |

---

## 二、构建

### 依赖

仓库已 vendor 全部依赖，**不需要联网、不需要额外安装**：

- `third_party/freeglut/` —— freeglut，**32 位和 64 位都有**：
  `include/GL/`（架构无关）、`lib/Win32|x64/`、`bin/Win32|x64/`
- `third_party/stb/stb_image.h` —— v2.30，public domain，单头文件，用于加载 OBJ 的 `.mtl` 贴图

### 环境

- Visual Studio 2022（含 MSVC 与自带的 CMake）
- Windows 10 及以上

### 从压缩包开始

解压到一个**新的空文件夹**（包内没有顶层目录，直接解压会散在当前目录里），
然后从下面四种方式里挑一种。包内不含 `build/` 和 `x64/`（都是构建产物），
但源码、依赖、工程文件齐全，**不需要联网也不需要额外安装**。

### 方式一：双击 viewer.sln（纯 VS 流程，推荐）

包内带了**手写的、自包含的**经典 VS 工程，不依赖 CMake：

```
viewer.sln                  双击这个
viewer.vcxproj              工程文件，已经配好一切
viewer.vcxproj.filters      解决方案资源管理器里的分组
```

双击 `viewer.sln`，在顶部选一个配置，F5：

| 配置 | 说明 |
|---|---|
| `Release` / `Win32` | **32 位**，指导书默认的「Debug Win32」就是这个 |
| `Release` / `x64` | 64 位 |
| `Debug` / 上面任一 | 调试用 |

产物在 `bin\<平台>\<配置>\viewer.exe`，例如 `bin\Win32\Release\viewer.exe`。

工程里已经配好，不需要你再去「附加依赖项」里手动加东西：

| 配置项 | 值 |
|---|---|
| 附加包含目录 | `src`、`third_party\freeglut\include`、`third_party\stb` |
| 附加依赖项 | `opengl32.lib`、`glu32.lib`、`third_party\freeglut\lib\$(Platform)\freeglut.lib` |
| 入口点 | `mainCRTStartup` |
| 子系统 | Windows |
| 附加选项 | `/utf-8`、`/wd4819` |
| 生成后事件 | 自动把 `freeglut.dll` 拷到 exe 旁边 |

两个平台都能编：依赖按架构分目录，工程里用 `$(Platform)` 拼路径（VS 的平台名正好
就是 `Win32` 和 `x64`）。所有路径都用 `$(ProjectDir)` 相对定位，整个文件夹搬到哪都能编。

### 方式二：一键脚本（不打开 VS）

包内自带两个脚本，会自己找 CMake（PATH 里没有就用 VS 2022 自带的那份），不需要配环境：

```bat
build.bat              :: 默认 64 位
build.bat Win32        :: 32 位
```

```bash
bash build.sh          # 64 位
bash build.sh Win32    # 32 位
```

两者都做 configure + 编译 Release，结束时打印可执行文件路径。`build.bat` 双击也能跑。
两个架构用不同的构建目录（`build/` 和 `build32/`）—— CMake 缓存和生成器平台绑死，
同一个目录里切架构会报 generator platform does not match。

### 方式三：用 CMake 打开文件夹

如果你更想走 CMake 那条路：**文件 → 打开 → 文件夹**，选中项目根目录
（含 `CMakeLists.txt` 的那一层）。`CMakePresets.json` 已经把预设限定成 `x64-Debug` /
`x64-Release`，顶部配置下拉里不会有 Win32，选不错。然后 F5 运行。

### 32 位 / 64 位怎么选

两个都能编，产物分别落在 `bin\Win32\` 和 `bin\x64\`（CMake 路径是 `build32/` 和 `build/`）。

课程指导书的环境配置用的是「Debug Win32」（32 位），要跟指导书一致就选 Win32；
自己机器上跑无所谓，x64 略快一点。**两者都能正常出图和录视频，已实测。**

万一遇到 `LNK2019 ... __imp__glutInitWindowSize@8` 这类错误，看符号名就能判断：
**带 `@数字` 后缀就是 32 位**（x86 stdcall 的名字修饰，x64 下不存在），
说明编译目标和 `third_party/freeglut/lib/<平台>/` 里的导入库对不上。

### 方式四：手动命令行

CMake **不在 PATH 里**，要用 VS 自带的那份。git-bash：

```bash
CM="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
"$CM" -S . -B build -G "Visual Studio 17 2022" -A x64
"$CM" --build build --config Release
```

cmd / PowerShell 下同理，用完整路径替换即可：

```bat
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B build -G "Visual Studio 17 2022" -A x64
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release
```

> 别想着用 `set CM=...` 再 `%CM%` 省字数：cmd 在**解析整行时**就把 `%CM%` 展开成空串了，
> 而 `set` 要到执行时才生效，所以那条命令什么都不会干。要用变量就写进 .bat 里。

产物在 **`build/Release/viewer.exe`** —— 注意 VS 生成器是「多配置」的，exe 在 `Release/`
子目录下而不是 `build/` 根下。`freeglut.dll` 由构建后步骤自动拷到 exe 旁边，不用手动处理。

### 常见问题

| 现象 | 原因 |
|---|---|
| `cmake: command not found` | CMake 不在 PATH，用上面 VS 自带的完整路径 |
| 三十多个 `LNK2019: ... __imp__glutInitWindowSize@8` | 编译目标与 freeglut 的架构对不上。`@8` 是 x86 stdcall 修饰，说明编的是 32 位。**本项目两个架构都支持，正常不会出现**；若出现，多半是 `third_party/freeglut/lib/<平台>/` 被动过 |
| `LNK1112: 模块计算机类型 x64 与 x86 冲突` | 同上 |
| CMake 报 `generator platform does not match` | 同一个 `build/` 里从 x64 切到了 Win32。删掉该目录重来，或用 `build32/` |
| 运行时提示找不到 `freeglut.dll`，或报 `0xc000007b` | 同样是位数不匹配，必须是 x64 |
| 直接双击 `build/viewer.vcxproj` 打不开 | 那是 CMake 生成的、依赖 CMake 缓存；纯 VS 流程请双击根目录的 **`viewer.sln`** |
| 换了生成器后 configure 报 generator does not match | 删掉 `build/CMakeCache.txt` 和 `build/CMakeFiles/` 重新 configure |

---

## 三、运行

```bash
./build/Release/viewer.exe
```

不带参数启动 = **森林场景 + 兔子**。程序会自动从 exe 所在目录逐级向上查找
`Models/bunny_10k.obj`（Stanford Bunny，10000 面），
所以从 VS 启动、命令行启动、双击启动都能找到模型。也可以显式指定任意 OBJ：

```bash
# 换回另一只兔子（38352 四边形面、Z-up、带贴图）
./build/Release/viewer.exe Models/Rabbit/10037_Rabbit_v1_L3.obj
```

模型的上轴会自动判别（老兔子 Z-up、新兔子 Y-up，两者都能正确立起来），也可用 `--up` 强制指定。

```bash
./build/Release/viewer.exe "D:/some/other/model.obj"
```

### 两个场景

| 场景 | 怎么进 | 里面有什么 |
|---|---|---|
| `forest` | 默认，或 `--scene forest` | Stanford Bunny + 程序化森林（地面 / 三种树型 / 雾）|
| `studio` | `--scene studio` | 实验四的彩色立方体、茶壶、球、地面、网格、坐标轴 |

**两个场景里的交互完全一样**，鼠标键盘都不用换：

- 左键拖动 → 对选中物体做几何变换（实验五 / 六）
- 右键拖动 → 旋转相机；WASD / QE → 相机漫游；滚轮 → 推拉
- 空格 → 播放 / 暂停动画（森林里就是兔子蹦跳漫步）

> 手动一拖鼠标、一按方向键，正在播的动画会自动停下，把控制权交回给你 ——
> 否则动画每帧都在覆写相机和物体位置，你拖半天看不到反应。想接着看再按空格。

### 命令行参数（写实验报告时批量出图用）

| 参数 | 说明 |
|---|---|
| `--shot out.bmp` | 渲染一帧、截图存为 BMP 后直接退出（窗口是隐藏的，批处理不会闪窗口） |
| `--proj persp\|ortho\|cav\|cab` | 指定投影模式 |
| `--view front\|back\|left\|right\|top\|bottom\|free` | 指定视图 |
| `--render solid\|wire\|points` | 指定绘制模式 |
| `--size 1600x300` | 指定窗口尺寸（用来验证窗口变化不变形） |
| `--quad` | 四视口布局 |
| `--no-hud` | 不画 HUD |
| `--show-help` | 显示帮助界面 |
| `--selftest` | 跑数学/相机自检，不开窗口 |
| `--scene forest\|studio` | 场景：`forest`（**默认**，兔子 + 程序化森林）/ `studio`（展示台：实验四立方体 / 茶壶 / 球）|
| `--up y\|z\|auto` | 指定模型上轴，默认 `auto` 按几何自动判别 |
| `--record demo.mp4` | 录制演示视频（H.264 mp4），见下 |
| `--anim <类型>` | 动画：`turntable` / `orbit` / `projections` / `transforms` / `roam`。**配合 `--record` 是录制，不配合就是实时预览** |
| `--seconds <秒>` | 录制时长，默认 6 |
| `--fps <帧率>` | 录制帧率，默认 30 |

批量出图示例：

```bash
V=./build/Release/viewer.exe
$V --shot out/front.bmp     --proj ortho --view front --no-hud
$V --shot out/cavalier.bmp  --proj cav   --view front --no-hud
$V --shot out/wide.bmp      --size 1600x300 --no-hud
python tools/bmp2png.py out/front.bmp      # BMP 转 PNG
```

`tools/bmp2png.py` 只用标准库，把查看器导出的 BMP 转成 PNG，方便插进报告。

### 录制演示视频

直接输出 H.264 mp4，逐帧数据用管道喂给 ffmpeg，不落临时文件。

```bash
V=./build/Release/viewer.exe
$V --record video/turntable.mp4 --anim turntable --seconds 6 --fps 30 --size 960x600
```

| `--anim` | 内容 | 对应打分点 |
|---|---|---|
| `turntable` | 模型绕 Y 轴自转，相机不动 | 三维场景 / 光照 / 纹理 |
| `orbit` | 相机绕模型环绕一圈，模型不动 | 实验七 观察变换 + 相机漫游（加分） |
| `projections` | 依次切换 透视 → 正平行 → 斜等测 → 斜二测 | 三视图基准分 + 斜平行投影（加分） |
| `transforms` | 立方体依次做 平移 → 旋转 → 缩放 | 实验五 几何变换 + 交互（加分） |
| `roam` | 兔子在森林里蹦跳漫步，追尾相机跟随（森林是默认场景，不用加参数） | 程序化动画（额外） |

`video/` 下已有录好的样例：前四种 960x600 / 30fps / 6s，`forest_roam.mp4` 是
960x600 / 30fps / **10s**。`transforms` 会把其它物体临时隐藏，只留立方体和地面/坐标轴 ——
兔子半径 1.0、离相机又近，不藏起来立方体根本看不清。

森林漫步的录制命令：

```bash
$V Models/bunny_10k.obj --scene forest \
   --record video/forest_roam.mp4 --anim roam --seconds 10 --fps 30 --size 960x600
```

> 想先在窗口里看看效果，不用急着录：`--anim roam` 不加 `--record` 就是实时预览
> （空格键播放/暂停）。预览走真实时钟、循环播放；录制走「帧序号 / fps」、可复现，两者刻意分开。

**出帧是离线渲染**：每帧画完立刻请求下一帧，不等真实时间，所以 180 帧约 3 秒就渲完了
（比实时快一倍）。动画时间取 `帧序号 / fps`，是个纯函数 —— 换台渲染慢的机器，输出的视频
依然严格是 6.000 秒 / 30fps，不会忽快忽慢。

> 依赖：`ffmpeg.exe` 要在 PATH 里（本机在 `C:\ffmpeg\bin`）。找不到时程序报
> `failed to start ffmpeg` 并正常退出，不会崩。

---

## 四、操作说明

按 `h` 或 `F1` 可以在程序内查看同一份帮助。

### 鼠标

| 操作 | 功能 |
|---|---|
| 左键拖动 | 按当前变换模式操作**选中物体** |
| 右键拖动 | 旋转相机（绕参考点，四元数旋转） |
| Shift + 左键拖动 | 平移相机 |
| **中键点击** | 弹出菜单 |
| 滚轮 | 推拉相机（缩放） |

> 菜单绑在**中键**而不是右键，是为了给右键拖动漫游让路 —— 这也是指导书实验七的做法。

### 几何变换（实验五 / 六）

| 按键 | 功能 |
|---|---|
| `t` / `r` / `y` | 切换 平移 / 旋转 / 缩放 模式 |
| `0` | 复位选中物体的变换矩阵 |
| `Tab` | 切换到下一个物体 |

> 缩放用 `y` 而不是 `s`，因为 `s` 已经被相机漫游的后退占用 —— 指导书里专门提醒过这一点。

### 投影（实验七 + 斜平行投影）

| 按键 | 功能 |
|---|---|
| `3` | 透视投影 |
| `o` | 正平行投影（正交） |
| `7` / `9` | 斜平行投影：斜等测(cavalier, L=1) / 斜二测(cabinet, L=0.5) |
| `-` / `=` | 调整斜边与水平方向的夹角（屏幕上呈现的角度） |
| `[` / `]` | 减小 / 增大透视视场角 FOV |

### 视图

| 按键 | 功能 |
|---|---|
| `2` / `8` | 正视图 / 后视图 |
| `4` / `6` | 左视图 / 右视图 |
| `5` | 顶视图 |
| `v` | 四视口布局（三视图 + 透视同屏） |

### 动画预览

| 按键 | 功能 |
|---|---|
| `空格` | 播放 / 暂停动画 |

森林场景下默认播放**兔子蹦跳漫步**，展示台场景下默认是模型自转。
想换别的动画，用 `--anim` 启动时指定：

```bash
# 森林里看兔子漫步
./build/Release/viewer.exe --scene forest --anim roam

# 展示台看模型自转 / 投影切换
./build/Release/viewer.exe --anim turntable
```

预览走真实时钟、循环播放，方便随时看效果；要输出成文件再用下面的 `--record`。

### 相机漫游（实验七加分项）

| 按键 | 功能 |
|---|---|
| `W` `A` `S` `D` | 前进 / 左移 / 后退 / 右移 |
| `Q` / `E` | 下降 / 上升 |
| `Shift+0` | 复位相机 |

### 渲染与显示

| 按键 | 功能 |
|---|---|
| `1` | 实体 → 线框 → 点 循环 |
| `c` | 背面剔除开关 |
| `m` | 平滑 / 平面着色 |
| `l` | 光照开关 |
| `T` | 贴图开关 |
| `g` / `x` / `p` | 网格 / 坐标轴 / 地面 显示开关 |
| `F12` | 保存截图 `screenshot.bmp` |
| `ESC` | 退出 |

---

## 五、项目结构

```
CG-Experiment/
├── CMakeLists.txt
├── src/
│   ├── main.cpp          GLUT 初始化、命令行解析、回调注册、主循环、自检
│   ├── app.{h,cpp}       全局状态、渲染主流程、HUD、截图、四视口
│   ├── input.{h,cpp}     键盘 / 鼠标 / 滚轮 / 右键菜单
│   ├── scene.{h,cpp}     两套场景：展示台（实验四立方体/图元/地面/网格/坐标轴）与森林
│   ├── mesh.{h,cpp}      网格：归一化、材质、客户端顶点数组绘制
│   ├── obj_loader.{h,cpp} OBJ / MTL 解析（实验十）
│   ├── texture.{h,cpp}   stb_image → OpenGL 纹理
│   ├── camera.{h,cpp}    观察变换、漫游、四元数旋转、退化保护
│   ├── projection.{h,cpp} 透视 / 正交 / 斜平行投影，保宽高比
│   ├── lighting.{h,cpp}  光照与材质（展示台点光源 / 森林太阳光）
│   ├── hud.{h,cpp}       屏幕叠加文字
│   ├── anim.{h,cpp}      脚本动画（时间的纯函数）：自转/环绕/投影切换/几何变换/森林漫步
│   ├── forest.{h,cpp}    程序化森林：地面 + 三种树型，烘焙成一个带顶点色的网格
│   ├── recorder.{h,cpp}  逐帧数据管道进 ffmpeg 编码 mp4
│   ├── math3d.h          向量 / 矩阵 / 四元数工具
│   └── types.h           模式枚举
├── third_party/          vendor 的 freeglut 与 stb_image
├── Models/
│   ├── bunny_10k.obj     默认模型（Stanford Bunny，10000 面，Y-up）
│   └── Rabbit/           另一只兔子（38352 四边形面，Z-up，带法线/UV/贴图）
├── video/                五段演示视频（已录好）
├── shots/                各模式的验证截图
└── tools/
    ├── bmp2png.py        截图格式转换（纯标准库）
    └── make_shots.sh     一键重新生成 shots/ 全部截图
```

---

## 六、实现要点

### 1. 导入 38352 个四边形面的 OBJ

指导书的 `OBJImporter` 只支持三角形面，而本模型 **38352 个面全部是四边形**，
必须做扇形三角化 `(0,1,2)+(0,2,3)`，最终得到 **76704 个三角形**。

同时指导书是逐三角形 `glBegin/glEnd` 绘制，对 7.6 万面意味着每帧约 46 万次立即模式调用。
本项目改为**客户端顶点数组 + 一次 `glDrawElements`**：

```cpp
glVertexPointer(3, GL_FLOAT, 0, positions.data());
glNormalPointer(GL_FLOAT, 0, normals.data());
glTexCoordPointer(2, GL_FLOAT, 0, uvs.data());
glDrawElements(GL_TRIANGLES, idx.size(), GL_UNSIGNED_INT, idx.data());
```

`(v,vt,vn)` 三元组用哈希去重后，40194 个唯一顶点复用，内存和调用次数都大幅下降。
线框模式只需要 `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)`，不用第二份索引。

（Windows SDK 的 `<GL/gl.h>` 只到 OpenGL 1.1，没有 `glGenBuffers`，所以走客户端数组而非 VBO。）

### 2. 斜平行投影

斜平行投影 = 正交投影 ∘ 沿视线方向的错切。眼空间可见点 `z_e = -d (d>0)`，于是

```
x' = x - L·cosφ·z_e
y' = y - L·sinφ·z_e
```

斜等测 `L = 1`，斜二测 `L = 0.5`。矩阵放在**投影矩阵**上，紧跟 `glOrtho` 之后
`glMultMatrixf`（`glMultMatrixf` 右乘，等价于先错切再正交，正是定义）。

两个容易踩的坑：

- **`m[10]` 必须保持 1。** 教材上那个斜投影矩阵是奇异的（把 z 压平成平面），
  直接拿来用会让所有片元落到同一个 NDC 深度，深度测试彻底失效、整个模型 z-fighting。
  保持仿射形式投影方向完全一致，但深度缓冲仍可用。
- **错切必须放在投影矩阵而不是 modelview。** 错切是相对投影平面（眼空间）定义的，
  放 modelview 就变成世界空间错切，相机一转斜边方向就错了。

另外屏幕上看到的夹角 θ 与世界空间的 φ 满足 `tanθ = tanφ · aspect`，
所以反解 `φ = atan(tanθ / aspect)`，这样窗口比例变化时屏幕上始终是 45° 斜二测。

### 3. 窗口变化图形不变形

指导书实验四的 `glFrustum(-2.5, 2.5, -2.5, 2.5, near, far)` **本身就是错的**：
它在近平面上永远是 1:1，窗口一变宽图形就被压扁 —— 正好是要求 3 要考的情况。

正确做法是保证「世界单位/像素」在 x、y 两方向相等：

- 正交：`halfW = halfH * aspect`，再 `glOrtho(-halfW, halfW, -halfH, halfH, ...)`
- 透视：`halfH = nearZ * tan(fovY/2)`，`halfW = halfH * aspect`，再 `glFrustum(...)`
- 四视口布局下每个视口按**自己的**尺寸算 aspect，所以四个视口都不变形

正交投影用 `glOrtho(..., -1000, 1000)` 这种「永不裁剪」的近远平面：`glOrtho` 允许
near 为负，且正交深度精度是线性的，这样六种视图和相机漫游都不会把物体裁掉。

### 4. 三视图共用同一缩放

模型加载后归一化到单位包围球。六个视图方向下包围球投影都是半径 R 的圆，
所以统一用 `halfH = R * 1.06`，各视图放大率完全一致 ——
如果每个视图各自独立取景，正视图和左视图会一大一小，看起来就不是一张三视图了。

### 5. 顶视图的退化情况

顶视图视线方向 `f = (0,-1,0)` 与默认 `up = (0,1,0)` 反平行，叉积为零，
`gluLookAt` 的基奇异 —— 而且它**不报错**，画面直接空白或乱码。
解决方法是顶视图用 `up = (0,0,-1)`（指导书也是这样），并额外写了 `safeLookAt()`，
当 `|f·up| > 0.999` 时自动切到备用 up，防止相机漫游时撞上同一奇点。

### 6. 光照

- **`glLightfv(GL_LIGHT0, GL_POSITION, ...)` 会被当前的 MODELVIEW 矩阵变换**，
  所以必须在 `gluLookAt` 之后、任何物体变换之前调用，否则光会跟着模型转。
- `glEnable(GL_NORMALIZE)`：交互缩放会产生非均匀缩放，法线必须重新归一化。
- 图元用 `glColorMaterial` + `GL_COLOR_MATERIAL`，否则 `glColor3f` 在光照下被完全忽略。
  但绘制 OBJ 时要**先关掉** `GL_COLOR_MATERIAL`，否则它会覆盖 `glMaterialfv` 设的
  `AMBIENT`/`DIFFUSE`，`.mtl` 材质就失效了。

### 7. 正常退出

```cpp
glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
...
glutMainLoop();
appCleanup();          // 现在真的会执行
```

默认的 `GLUT_ACTION_EXIT` 会从回调里直接 `exit()`，跳过所有清理代码。
另外**没有**照抄指导书在 `display()` 末尾无条件 `glutPostRedisplay()` ——
那会让 CPU 空转 100%，本项目改为按需重绘（只在输入回调里 post）。

---

### 8. 森林场景与蹦跳步态

**为什么不是骨骼动画**：Stanford Bunny 是一坨静态三角网格，既没有骨架也没有蒙皮权重。
要做四条腿交替迈步，得先自动绑骨 —— 那是研究级的问题，在这个项目里做不可靠。
好在兔子真实的移动方式是**蹦跳**而不是走，而且这个模型本来就是蹲坐姿、腿收在身下，
没有可迈的腿。所以改用程序化步态，观感反而更贴切：

- **跳跃弧线**用抛物线 `4k(1-k)` 而不是正弦，更接近真实弹道，落地那一下的顿挫明显得多
- **落地压缩 / 腾空拉伸**（动画十二原则的 squash & stretch），用 `1/√sy` 保持体积
- **起跳抬头、下落低头** —— 注意轴：模型自身朝 `-X`，所以俯仰要绕 **Z** 轴转，
  绕 X 只会让它左右翻滚（这个坑踩过一次）
- **转弯朝向**取路径切线，`θ = atan2(h.z, -h.x)`

模型原点在包围球中心而不是脚底，所以要垫高 `-bboxMin[1]` 才踩在地上；压扁时脚会往上跑，
再乘 `sy` 抵回来，保证脚始终贴地。

**森林**是程序化生成的，全部烘焙进**一个**带顶点色的网格（一次 draw call 画完）：
地面细分网格（雾是逐顶点的，不细分远处会突变成硬边）+ 三种树型（针叶 / 阔叶 / 灌木）。
用固定种子的 xorshift 而不是 `rand()` —— 各 C 运行库实现不同，换个编译器森林就变了，
录出来的视频没法复现。

种树时会避开兔子的漫游路线，**避让半径必须大于追尾相机的跟随距离**：相机吊在兔子后面
4.6 单位，树只让开 3.6 的话相机会一头扎进树干里，而背面剔除会让那棵树直接"闪没"。

## 七、自检

```bash
./build/Release/viewer.exe --selftest
```

11 项断言覆盖截图验证不了的逻辑：矩阵乘法、轴角旋转、四元数旋转与矩阵形式的一致性、
绕参考点旋转保持距离、旋转 360° 回到原点、推拉改变距离、顶视图 up 向量非退化、
六个视图预设共用同一距离、漫游沿视线平移且不改变距离。

---

## 八、已知限制

- **HUD 与菜单文字只支持 ASCII。** GLUT 的位图字体（`glutBitmapCharacter`）只能画 ASCII，
  中文会变成空白或乱码。程序内所有界面文字和窗口标题都用英文。若要中文界面，
  需要改用 `wglUseFontBitmaps` 加载中文字体。
- 交互缩放只有鼠标拖动一条路径，没有数值输入框。
- **`--size` 请求的尺寸会被屏幕尺寸截断。** Windows 不允许窗口比屏幕还高，
  例如在 1920×1080 上请求 `--size 300x1600`，实际只得到 300×1061。
  程序会查询真实的可绘制尺寸（`glutGet(GLUT_WINDOW_WIDTH/HEIGHT)`）而不是用请求值，
  所以宽高比始终按真实尺寸算、截图也不会出现黑边；但想要某个特定极端比例，
  得选一个屏幕放得下的尺寸。
- `models/` 里只有兔子一个模型；PLY 格式未实现（实验十的 PLY 部分需要额外的模型文件）。
