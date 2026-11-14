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
