#pragma once
// Screen-space rectangles for P0's test scenes and the development read-out (shaders/debug.*).

#include "gfx/gl.h"
#include "core/types.h"

namespace gt {

struct DebugQuad {
    float x, y, dx, dy;  // centre and last step, pixels (top-left origin)
    float w, h;
    u8 r, g, b, a;
};

class DebugDraw {
public:
    bool init();
    void destroy();
    void clear() { n_ = 0; }
    void add(const DebugQuad& q) {
        if (n_ < kMax) quads_[n_++] = q;
    }
    void rect(float x, float y, float w, float h, u32 rgba);  // top-left corner
    void draw();  // premultiplied blending; the caller binds the target

private:
    static constexpr int kMax = 4096;
    DebugQuad quads_[kMax];
    int n_ = 0;
    GLuint prog_ = 0, vao_ = 0, vbo_ = 0;
};

}  // namespace gt
