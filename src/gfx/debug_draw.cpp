#include "gfx/debug_draw.h"

#include <cstddef>

#include "gfx/shader.h"

namespace gt {

bool DebugDraw::init() {
    prog_ = buildProgram("debug.vert", "debug.frag");
    if (!prog_) return false;
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof quads_, nullptr, GL_STREAM_DRAW);
    const GLsizei stride = sizeof(DebugQuad);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(DebugQuad, x));
    glVertexAttribDivisor(0, 1);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(DebugQuad, w));
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)offsetof(DebugQuad, r));
    glVertexAttribDivisor(2, 1);
    glBindVertexArray(0);
    return true;
}

void DebugDraw::destroy() {
    if (prog_) glDeleteProgram(prog_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    prog_ = vao_ = vbo_ = 0;
}

void DebugDraw::rect(float x, float y, float w, float h, u32 rgba) {
    add({x + w * 0.5f, y + h * 0.5f, 0, 0, w, h, u8(rgba >> 24), u8(rgba >> 16), u8(rgba >> 8), u8(rgba)});
}

void DebugDraw::draw() {
    if (n_ == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof quads_, nullptr, GL_STREAM_DRAW);  // orphan
    glBufferSubData(GL_ARRAY_BUFFER, 0, GLsizeiptr(sizeof(DebugQuad) * size_t(n_)), quads_);
    glUseProgram(prog_);
    glBindVertexArray(vao_);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n_);
    glBindVertexArray(0);
}

}  // namespace gt
