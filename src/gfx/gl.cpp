#include "gfx/gl.h"

#include <atomic>

#include "core/log.h"

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#define PM_GL_DEFINE(type, name) type name = nullptr;
PM_GL_FUNCS(PM_GL_DEFINE)
#undef PM_GL_DEFINE
#endif

namespace pm {

namespace {
std::atomic<int> g_debugMessages{0};

#if defined(_WIN32)
#define PM_GL_CALLBACK __stdcall
#else
#define PM_GL_CALLBACK
#endif

void PM_GL_CALLBACK onGlDebug(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei, const GLchar* message,
                               const void*) {
    if (severity == PM_GL_DEBUG_SEVERITY_NOTIFICATION) return;
    g_debugMessages.fetch_add(1);
    PM_WARN("gl debug: source 0x%x type 0x%x id %u severity 0x%x: %s", source, type, id, severity, message);
}

using DebugProc = void(PM_GL_CALLBACK*)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, const void*);
using DebugMessageCallback = void(PM_GL_CALLBACK*)(DebugProc, const void*);
}  // namespace

bool loadGl(void* (*getProc)(const char*)) {
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    (void)getProc;
    return true;
#else
#define PM_GL_LOAD(type, name)                                     \
    name = reinterpret_cast<type>(getProc(#name));                 \
    if (!name) {                                                   \
        PM_ERR("gl: the context lacks %s", #name);                 \
        return false;                                              \
    }
    PM_GL_FUNCS(PM_GL_LOAD)
#undef PM_GL_LOAD
    return true;
#endif
}

void installGlDebug(void* (*getProc)(const char*)) {
    auto fn = reinterpret_cast<DebugMessageCallback>(getProc("glDebugMessageCallback"));
    if (!fn) fn = reinterpret_cast<DebugMessageCallback>(getProc("glDebugMessageCallbackKHR"));
    if (!fn) {
        PM_LOG("gl: no debug callback in this context");
        return;
    }
    glEnable(PM_GL_DEBUG_OUTPUT);
    glEnable(PM_GL_DEBUG_OUTPUT_SYNCHRONOUS);
    fn(onGlDebug, nullptr);
    while (glGetError() != GL_NO_ERROR) {
    }
    PM_LOG("gl: debug callback installed");
}

int glDebugMessageCount() { return g_debugMessages.load(); }

void checkGl(const char* where) {
    const GLenum e = glGetError();
    if (e != GL_NO_ERROR) {
        g_debugMessages.fetch_add(1);
        PM_WARN("gl error 0x%x at %s", e, where);
    }
}

}  // namespace pm
