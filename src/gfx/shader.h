#pragma once
// Programs from the embedded sources (shaders/): version line + prelude_<stage>.glsl + prelude.glsl + body.
// Every program binds the uniform block "Frame" to binding 0 (TECH_SPEC 5: one block of per-frame values).

#include "gfx/gl.h"

namespace gt {

// "#version 300 es\n" (ES 3.0) or "#version 330 core\n" (desktop OpenGL 3.3 fallback).
void setGlslVersionLine(const char* line);
const char* shaderSource(const char* name);

// Builds and links; returns 0 (and logs the compiler's message) on failure. A non-empty log on success is a
// warning. Sampler uniforms named in `samplers` are bound to texture units 0, 1, 2 ... in order.
GLuint buildProgram(const char* vertName, const char* fragName, const char* const* samplers = nullptr, int samplerCount = 0);

constexpr GLuint kFrameBinding = 0;

}  // namespace gt
