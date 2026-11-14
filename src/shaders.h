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
