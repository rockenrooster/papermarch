#pragma once
// Render targets: a colour texture (+ optional depth renderbuffer) framebuffer object.

#include "gfx/gl.h"

namespace gt {

struct Target {
    GLuint fbo = 0, color = 0, depth = 0;
    int w = 0, h = 0;

    bool create(int width, int height, bool withDepth, bool linear);
    void destroy();
    void bind() const;  // binds and sets the viewport
};

// Reads the bound framebuffer (RGBA8) into a PNG. Desktop targets are bottom-up.
bool captureBound(const char* path, int w, int h);

}  // namespace gt
