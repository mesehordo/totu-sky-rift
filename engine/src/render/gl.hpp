// SPDX-License-Identifier: GPL-3.0-or-later
// OpenGL 3.3 core – csak a megjelenítőhöz szükséges függvények és állandók (ADR-003). Nincs külső GL-fejléc vagy
// betöltő: a függvényeket a hívó által adott betöltő (pl. SDL_GL_GetProcAddress) adja.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#if defined(_WIN32)
#define TOTU_GLAPI __stdcall
#else
#define TOTU_GLAPI
#endif

namespace totu::render::gl {

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLchar = char;
using GLubyte = unsigned char;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

// állandók
constexpr GLenum NO_ERROR = 0, TEXTURE_2D = 0x0DE1, RGBA = 0x1908, RGBA8 = 0x8058, UNSIGNED_BYTE = 0x1401, FLOAT = 0x1406;
constexpr GLenum TEXTURE_MIN_FILTER = 0x2801, TEXTURE_MAG_FILTER = 0x2800, TEXTURE_WRAP_S = 0x2802, TEXTURE_WRAP_T = 0x2803;
constexpr GLint LINEAR = 0x2601, NEAREST = 0x2600, CLAMP_TO_EDGE = 0x812F, LINEAR_MIPMAP_LINEAR = 0x2703;
constexpr GLenum TEXTURE0 = 0x84C0, ARRAY_BUFFER = 0x8892, STREAM_DRAW = 0x88E0, STATIC_DRAW = 0x88E4;
constexpr GLenum VERTEX_SHADER = 0x8B31, FRAGMENT_SHADER = 0x8B30, COMPILE_STATUS = 0x8B81, LINK_STATUS = 0x8B82;
constexpr GLenum INFO_LOG_LENGTH = 0x8B84, TRIANGLES = 0x0004, BLEND = 0x0BE2, SRC_ALPHA = 0x0302, ONE = 1;
constexpr GLenum ONE_MINUS_SRC_ALPHA = 0x0303, COLOR_BUFFER_BIT = 0x4000, DEPTH_TEST = 0x0B71, SCISSOR_TEST = 0x0C11;
constexpr GLenum FRAMEBUFFER = 0x8D40, COLOR_ATTACHMENT0 = 0x8CE0, FRAMEBUFFER_COMPLETE = 0x8CD5, READ_FRAMEBUFFER = 0x8CA8;
constexpr GLenum PACK_ALIGNMENT = 0x0D05, UNPACK_ALIGNMENT = 0x0CF5, MAX_TEXTURE_SIZE = 0x0D33;
constexpr GLenum VERSION = 0x1F02, RENDERER = 0x1F01, VENDOR = 0x1F00, CULL_FACE = 0x0B44;
constexpr GLenum TEXTURE_2D_ARRAY = 0x8C1A, R8 = 0x8229, RED = 0x1903, MAX_ARRAY_TEXTURE_LAYERS = 0x88FF;

#define TOTU_GL_FUNCS(X)                                                                                         \
  X(const GLubyte*, GetString, (GLenum name))                                                                    \
  X(GLenum, GetError, (void))                                                                                    \
  X(void, GetIntegerv, (GLenum pname, GLint* data))                                                              \
  X(void, Viewport, (GLint x, GLint y, GLsizei w, GLsizei h))                                                    \
  X(void, Scissor, (GLint x, GLint y, GLsizei w, GLsizei h))                                                     \
  X(void, ClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a))                                              \
  X(void, Clear, (GLbitfield mask))                                                                              \
  X(void, Enable, (GLenum cap))                                                                                  \
  X(void, Disable, (GLenum cap))                                                                                 \
  X(void, BlendFunc, (GLenum s, GLenum d))                                                                       \
  X(void, BlendFuncSeparate, (GLenum sc, GLenum dc, GLenum sa, GLenum da))                                       \
  X(void, Finish, (void))                                                                                        \
  X(void, PixelStorei, (GLenum pname, GLint param))                                                              \
  X(void, ReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, void* data))             \
  X(void, GenTextures, (GLsizei n, GLuint* t))                                                                   \
  X(void, DeleteTextures, (GLsizei n, const GLuint* t))                                                          \
  X(void, BindTexture, (GLenum target, GLuint t))                                                                \
  X(void, ActiveTexture, (GLenum unit))                                                                          \
  X(void, TexParameteri, (GLenum target, GLenum pname, GLint param))                                             \
  X(void, TexImage2D, (GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border, GLenum fmt,  \
                       GLenum type, const void* data))                                                           \
  X(void, TexSubImage2D, (GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt,        \
                          GLenum type, const void* data))                                                        \
  X(void, TexImage3D, (GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLsizei d, GLint border,       \
                       GLenum fmt, GLenum type, const void* data))                                               \
  X(void, TexSubImage3D, (GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d, \
                          GLenum fmt, GLenum type, const void* data))                                            \
  X(void, GenerateMipmap, (GLenum target))                                                                       \
  X(void, GenBuffers, (GLsizei n, GLuint* b))                                                                    \
  X(void, DeleteBuffers, (GLsizei n, const GLuint* b))                                                           \
  X(void, BindBuffer, (GLenum target, GLuint b))                                                                 \
  X(void, BufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage))                          \
  X(void, BufferSubData, (GLenum target, GLintptr off, GLsizeiptr size, const void* data))                       \
  X(void, GenVertexArrays, (GLsizei n, GLuint* a))                                                               \
  X(void, DeleteVertexArrays, (GLsizei n, const GLuint* a))                                                      \
  X(void, BindVertexArray, (GLuint a))                                                                           \
  X(void, EnableVertexAttribArray, (GLuint i))                                                                   \
  X(void, VertexAttribPointer, (GLuint i, GLint size, GLenum type, GLboolean norm, GLsizei stride,               \
                                const void* ptr))                                                                \
  X(void, VertexAttribDivisor, (GLuint i, GLuint divisor))                                                       \
  X(void, DrawArrays, (GLenum mode, GLint first, GLsizei count))                                                 \
  X(void, DrawArraysInstanced, (GLenum mode, GLint first, GLsizei count, GLsizei instances))                     \
  X(GLuint, CreateShader, (GLenum type))                                                                         \
  X(void, ShaderSource, (GLuint s, GLsizei n, const GLchar* const* src, const GLint* len))                       \
  X(void, CompileShader, (GLuint s))                                                                             \
  X(void, GetShaderiv, (GLuint s, GLenum pname, GLint* p))                                                       \
  X(void, GetShaderInfoLog, (GLuint s, GLsizei max, GLsizei* len, GLchar* log))                                  \
  X(void, DeleteShader, (GLuint s))                                                                              \
  X(GLuint, CreateProgram, (void))                                                                               \
  X(void, AttachShader, (GLuint p, GLuint s))                                                                    \
  X(void, LinkProgram, (GLuint p))                                                                               \
  X(void, GetProgramiv, (GLuint p, GLenum pname, GLint* v))                                                      \
  X(void, GetProgramInfoLog, (GLuint p, GLsizei max, GLsizei* len, GLchar* log))                                 \
  X(void, UseProgram, (GLuint p))                                                                                \
  X(void, DeleteProgram, (GLuint p))                                                                             \
  X(GLint, GetUniformLocation, (GLuint p, const GLchar* name))                                                   \
  X(void, Uniform1i, (GLint loc, GLint v))                                                                       \
  X(void, Uniform1f, (GLint loc, GLfloat v))                                                                     \
  X(void, Uniform2f, (GLint loc, GLfloat a, GLfloat b))                                                          \
  X(void, Uniform4f, (GLint loc, GLfloat a, GLfloat b, GLfloat c, GLfloat d))                                    \
  X(void, UniformMatrix4fv, (GLint loc, GLsizei n, GLboolean transpose, const GLfloat* m))                       \
  X(void, GenFramebuffers, (GLsizei n, GLuint* f))                                                               \
  X(void, DeleteFramebuffers, (GLsizei n, const GLuint* f))                                                      \
  X(void, BindFramebuffer, (GLenum target, GLuint f))                                                            \
  X(void, FramebufferTexture2D, (GLenum target, GLenum att, GLenum textarget, GLuint tex, GLint level))         \
  X(GLenum, CheckFramebufferStatus, (GLenum target))

struct Api {
#define TOTU_GL_MEMBER(ret, name, args) ret(TOTU_GLAPI* name) args = nullptr;
  TOTU_GL_FUNCS(TOTU_GL_MEMBER)
#undef TOTU_GL_MEMBER
};

using Loader = void* (*)(const char* name);

// Betölti a függvényeket; hiányzó függvénynél false, és `missing` a neve.
bool load(Api& api, Loader loader, std::string* missing = nullptr);

}  // namespace totu::render::gl
