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
