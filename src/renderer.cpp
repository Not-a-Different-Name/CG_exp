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
