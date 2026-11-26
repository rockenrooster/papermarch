#pragma once
// OpenGL ES 3.0 core (TECH_SPEC 5.1). Android links GLESv3 directly, a browser build Emscripten's WebGL 2. The desktop
// resolves the same entry points at start-up with SDL_GL_GetProcAddress (an ES 3.0 context through WGL, or an OpenGL 3.3
// core context as fallback).

#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#define GL_GLES_PROTOTYPES 0
#include <GLES3/gl3.h>
#endif

// Constants outside ES 3.0 core that the debug callback uses (KHR_debug, same values on desktop GL).
#define GT_GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GT_GL_DEBUG_OUTPUT 0x92E0
#define GT_GL_DEBUG_SEVERITY_HIGH 0x9146
#define GT_GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GT_GL_DEBUG_SEVERITY_LOW 0x9148
#define GT_GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#define GT_GL_CONTEXT_FLAGS 0x821E

#define GT_GL_FUNCS(X)                                                                                         \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture)                                                                 \
    X(PFNGLATTACHSHADERPROC, glAttachShader)                                                                   \
    X(PFNGLBINDBUFFERPROC, glBindBuffer)                                                                       \
    X(PFNGLBINDBUFFERBASEPROC, glBindBufferBase)                                                               \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                                                             \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer)                                                           \
    X(PFNGLBINDTEXTUREPROC, glBindTexture)                                                                     \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                                                             \
    X(PFNGLBLENDEQUATIONPROC, glBlendEquation)                                                                 \
    X(PFNGLBLENDFUNCPROC, glBlendFunc)                                                                         \
    X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate)                                                         \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)                                                             \
    X(PFNGLBUFFERDATAPROC, glBufferData)                                                                       \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData)                                                                 \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)                                               \
    X(PFNGLCLEARPROC, glClear)                                                                                 \
    X(PFNGLCLEARCOLORPROC, glClearColor)                                                                       \
    X(PFNGLCOLORMASKPROC, glColorMask)                                                                         \
    X(PFNGLCOMPILESHADERPROC, glCompileShader)                                                                 \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                                                                 \
    X(PFNGLCREATESHADERPROC, glCreateShader)                                                                   \
    X(PFNGLCULLFACEPROC, glCullFace)                                                                           \
    X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)                                                                 \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)                                                       \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram)                                                                 \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)                                                     \
    X(PFNGLDELETESHADERPROC, glDeleteShader)                                                                   \
    X(PFNGLDELETETEXTURESPROC, glDeleteTextures)                                                               \
    X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)                                                       \
    X(PFNGLDEPTHFUNCPROC, glDepthFunc)                                                                         \
    X(PFNGLDEPTHMASKPROC, glDepthMask)                                                                         \
    X(PFNGLDISABLEPROC, glDisable)                                                                             \
    X(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray)                                           \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays)                                                                       \
    X(PFNGLDRAWARRAYSINSTANCEDPROC, glDrawArraysInstanced)                                                     \
    X(PFNGLDRAWELEMENTSPROC, glDrawElements)                                                                   \
    X(PFNGLENABLEPROC, glEnable)                                                                               \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)                                             \
    X(PFNGLFINISHPROC, glFinish)                                                                               \
    X(PFNGLFLUSHPROC, glFlush)                                                                                 \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer)                                             \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)                                                   \
    X(PFNGLGENBUFFERSPROC, glGenBuffers)                                                                       \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                                                             \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers)                                                           \
    X(PFNGLGENTEXTURESPROC, glGenTextures)                                                                     \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                                                             \
    X(PFNGLGETERRORPROC, glGetError)                                                                           \
    X(PFNGLGETINTEGERVPROC, glGetIntegerv)                                                                     \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                                                         \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                                                                   \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                                                           \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv)                                                                     \
    X(PFNGLGETSTRINGPROC, glGetString)                                                                         \
    X(PFNGLGETSTRINGIPROC, glGetStringi)                                                                       \
    X(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex)                                                   \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)                                                       \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram)                                                                     \
    X(PFNGLPIXELSTOREIPROC, glPixelStorei)                                                                     \
    X(PFNGLREADPIXELSPROC, glReadPixels)                                                                       \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage)                                                     \
    X(PFNGLSCISSORPROC, glScissor)                                                                             \
    X(PFNGLSHADERSOURCEPROC, glShaderSource)                                                                   \
    X(PFNGLTEXIMAGE2DPROC, glTexImage2D)                                                                       \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri)                                                                 \
    X(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D)                                                                 \
    X(PFNGLUNIFORM1FPROC, glUniform1f)                                                                         \
    X(PFNGLUNIFORM1IPROC, glUniform1i)                                                                         \
    X(PFNGLUNIFORM2FPROC, glUniform2f)                                                                         \
    X(PFNGLUNIFORM4FPROC, glUniform4f)                                                                         \
    X(PFNGLUNIFORM4FVPROC, glUniform4fv)                                                                       \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)                                                           \
    X(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding)                                                     \
    X(PFNGLUSEPROGRAMPROC, glUseProgram)                                                                       \
    X(PFNGLVERTEXATTRIBDIVISORPROC, glVertexAttribDivisor)                                                     \
    X(PFNGLVERTEXATTRIBIPOINTERPROC, glVertexAttribIPointer)                                                   \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)                                                     \
    X(PFNGLVIEWPORTPROC, glViewport)

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#define GT_GL_DECLARE(type, name) extern type name;
GT_GL_FUNCS(GT_GL_DECLARE)
#undef GT_GL_DECLARE
#endif

namespace gt {

// Resolves the entry points (desktop). Returns false and logs the first missing one. Android: always true.
bool loadGl(void* (*getProc)(const char*));
// Installs the KHR_debug callback when the context offers it (debug builds). Messages are counted.
void installGlDebug(void* (*getProc)(const char*));
int glDebugMessageCount();
// glGetError once; logs and counts a non-zero result with `where`.
void checkGl(const char* where);

}  // namespace gt
