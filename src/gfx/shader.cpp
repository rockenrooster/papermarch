#include "gfx/shader.h"

#include <cstdio>
#include <cstring>

#include "core/log.h"

namespace pm {

namespace {

struct Source {
    const char* name;
    const char* text;
};
const Source kSources[] = {
#include "shader_sources.inc"
};

const char* g_version = "#version 300 es\n";

GLuint compile(GLenum stage, const char* name) {
    const char* body = shaderSource(name);
    if (!body) {
        PM_ERR("shader: %s is not embedded", name);
        return 0;
    }
    const char* parts[4] = {g_version, shaderSource(stage == GL_VERTEX_SHADER ? "prelude_vert.glsl" : "prelude_frag.glsl"),
                            shaderSource("prelude.glsl"), body};
    const GLuint s = glCreateShader(stage);
    glShaderSource(s, 4, parts, nullptr);
    glCompileShader(s);
    GLint ok = 0, len = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
    char log[2048] = "";
    if (len > 1) glGetShaderInfoLog(s, sizeof log, nullptr, log);
    if (!ok) {
        PM_ERR("shader %s failed to compile:\n%s", name, log);
        glDeleteShader(s);
        return 0;
    }
    if (len > 1 && log[0]) PM_WARN("shader %s compiled with a message: %s", name, log);
    return s;
}

}  // namespace

void setGlslVersionLine(const char* line) { g_version = line; }

const char* shaderSource(const char* name) {
    for (const Source& s : kSources)
        if (!std::strcmp(s.name, name)) return s.text;
    return nullptr;
}

GLuint buildProgram(const char* vertName, const char* fragName, const char* const* samplers, int samplerCount) {
    const GLuint vs = compile(GL_VERTEX_SHADER, vertName);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragName);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    const GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0, len = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
    char log[2048] = "";
    if (len > 1) glGetProgramInfoLog(p, sizeof log, nullptr, log);
    if (!ok) {
        PM_ERR("program %s + %s failed to link:\n%s", vertName, fragName, log);
        glDeleteProgram(p);
        return 0;
    }
    if (len > 1 && log[0]) PM_WARN("program %s + %s linked with a message: %s", vertName, fragName, log);
    const GLuint block = glGetUniformBlockIndex(p, "Frame");
    if (block != GL_INVALID_INDEX) glUniformBlockBinding(p, block, kFrameBinding);
    if (samplerCount > 0) {
        glUseProgram(p);
        for (int i = 0; i < samplerCount; ++i) {
            const GLint loc = glGetUniformLocation(p, samplers[i]);
            if (loc >= 0) glUniform1i(loc, i);
        }
        glUseProgram(0);
    }
    return p;
}

}  // namespace pm
