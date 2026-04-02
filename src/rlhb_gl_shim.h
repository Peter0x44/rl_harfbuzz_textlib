#ifndef RL_HARFBUZZ_TEXTLIB_RLHB_GL_SHIM_H
#define RL_HARFBUZZ_TEXTLIB_RLHB_GL_SHIM_H

#include <stddef.h>

#if defined(_WIN32) && !defined(APIENTRY)
  #define APIENTRY __stdcall
#endif

#if defined(USE_LIBTYPE_SHARED)
  #if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(__GNUC__)
      #define RLHB_GL_EXTERN __attribute__((dllimport)) extern
    #else
      #define RLHB_GL_EXTERN __declspec(dllimport) extern
    #endif
  #else
    #define RLHB_GL_EXTERN extern
  #endif
#else
  #define RLHB_GL_EXTERN extern
#endif

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

typedef void (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRY *PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRY *PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef void (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint *buffers);
typedef void (APIENTRY *PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint *textures);
typedef void (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void (APIENTRY *PFNGLGENTEXTURESPROC)(GLsizei n, GLuint *textures);
typedef void (APIENTRY *PFNGLTEXBUFFERPROC)(GLenum target, GLenum internalformat, GLuint buffer);
typedef void (APIENTRY *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);

RLHB_GL_EXTERN PFNGLACTIVETEXTUREPROC glad_glActiveTexture;
RLHB_GL_EXTERN PFNGLBINDBUFFERPROC glad_glBindBuffer;
RLHB_GL_EXTERN PFNGLBINDTEXTUREPROC glad_glBindTexture;
RLHB_GL_EXTERN PFNGLBUFFERDATAPROC glad_glBufferData;
RLHB_GL_EXTERN PFNGLBUFFERSUBDATAPROC glad_glBufferSubData;
RLHB_GL_EXTERN PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers;
RLHB_GL_EXTERN PFNGLDELETETEXTURESPROC glad_glDeleteTextures;
RLHB_GL_EXTERN PFNGLGENBUFFERSPROC glad_glGenBuffers;
RLHB_GL_EXTERN PFNGLGENTEXTURESPROC glad_glGenTextures;
RLHB_GL_EXTERN PFNGLTEXBUFFERPROC glad_glTexBuffer;
RLHB_GL_EXTERN PFNGLUNIFORM1IPROC glad_glUniform1i;

#define glActiveTexture glad_glActiveTexture
#define glBindBuffer glad_glBindBuffer
#define glBindTexture glad_glBindTexture
#define glBufferData glad_glBufferData
#define glBufferSubData glad_glBufferSubData
#define glDeleteBuffers glad_glDeleteBuffers
#define glDeleteTextures glad_glDeleteTextures
#define glGenBuffers glad_glGenBuffers
#define glGenTextures glad_glGenTextures
#define glTexBuffer glad_glTexBuffer
#define glUniform1i glad_glUniform1i

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_FUNC_ADD 0x8006
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_RGBA16I 0x8D88
#define GL_SRC_ALPHA 0x0302
#define GL_STATIC_DRAW 0x88E4
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_BUFFER 0x8C2A
#define GL_VERTEX_SHADER 0x8B31

#endif