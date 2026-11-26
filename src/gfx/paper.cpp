#include "gfx/paper.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <initializer_list>

#include "core/log.h"
#include "gfx/palette.h"
#include "gfx/shader.h"

namespace pm {

namespace {

struct FrameBlock {
    float screen[4];
    float time[4];
};

GLuint makeBuffer(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    GLuint b = 0;
    glGenBuffers(1, &b);
    glBindBuffer(target, b);
    glBufferData(target, size, data, usage);
    return b;
}

void attrib(GLuint loc, GLint n, GLenum type, GLboolean norm, GLsizei stride, size_t offset, GLuint divisor) {
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, n, type, norm, stride, reinterpret_cast<void*>(offset));
    glVertexAttribDivisor(loc, divisor);
}

void stream(GLuint vbo, GLsizeiptr capacity, const void* data, GLsizeiptr size) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, capacity, nullptr, GL_STREAM_DRAW);  // orphan
    if (size > 0) glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}

void stickerVao(GLuint& vao, GLuint& vbo, int capacity) {
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    vbo = makeBuffer(GL_ARRAY_BUFFER, GLsizeiptr(sizeof(Sticker) * size_t(capacity)), nullptr, GL_STREAM_DRAW);
    attrib(0, 4, GL_FLOAT, GL_FALSE, sizeof(Sticker), offsetof(Sticker, x), 1);
    attrib(1, 4, GL_FLOAT, GL_FALSE, sizeof(Sticker), offsetof(Sticker, rot), 1);
    attrib(2, 4, GL_FLOAT, GL_FALSE, sizeof(Sticker), offsetof(Sticker, p1), 1);
    attrib(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Sticker), offsetof(Sticker, c0), 1);
    attrib(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Sticker), offsetof(Sticker, c1), 1);
    attrib(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Sticker), offsetof(Sticker, c2), 1);
}

void rgb3(u32 c, float* out) {
    out[0] = float((c >> 16) & 0xFF) / 255.0f;
    out[1] = float((c >> 8) & 0xFF) / 255.0f;
    out[2] = float(c & 0xFF) / 255.0f;
}

}  // namespace

Sticker makeSticker(Shape shape, float x, float y, float hw, float hh, u32 c0, float alpha) {
    Sticker s{};
    s.x = x;
    s.y = y;
    s.hw = hw;
    s.hh = hh;
    s.shape = float(shape);
    s.soft = 1.0f;
    setColor(s.c0, c0, alpha);
    setColor(s.c1, c0, alpha);
    setColor(s.c2, c0, alpha);
    return s;
}

bool Paper::makeR8(Target& t, int w, int h) {
    t.destroy();
    t.w = w;
    t.h = h;
    glGenTextures(1, &t.color);
    glBindTexture(GL_TEXTURE_2D, t.color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.color, 0);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        PM_ERR("R8 target %dx%d incomplete (0x%x)", w, h, status);
        return false;
    }
    return true;
}

bool Paper::init(int w, int h) {
    static const char* texSampler[] = {"uTex"};
    static const char* atlasSampler[] = {"uAtlas"};
    static const char* spriteSampler[] = {"uSprites"};
    static const char* groundSamplers[] = {"uRoadTex", "uLandTex", "uBackdrop"};
    static const char* postSamplers[] = {"uTex", "uBloom"};
    pSticker_ = buildProgram("sticker.vert", "sticker.frag", spriteSampler, 1);
    pPage_ = buildProgram("page.vert", "page.frag", texSampler, 1);
    pTable_ = buildProgram("fullscreen.vert", "table.frag");
    pShadow_ = buildProgram("fullscreen.vert", "shadow.frag", texSampler, 1);
    pCurl_ = buildProgram("curl.vert", "curl.frag", texSampler, 1);
    pText_ = buildProgram("text.vert", "text.frag", atlasSampler, 1);
    pGround_ = buildProgram("fullscreen.vert", "ground.frag", groundSamplers, 3);
    pBright_ = buildProgram("fullscreen.vert", "bright.frag", texSampler, 1);
    pBlur_ = buildProgram("fullscreen.vert", "blur.frag", texSampler, 1);
    pPost_ = buildProgram("fullscreen.vert", "post.frag", postSamplers, 2);
    for (GLuint p : {pSticker_, pPage_, pTable_, pShadow_, pCurl_, pText_, pGround_, pBright_, pBlur_, pPost_})
        if (!p) return false;
    {
        const char* names[kGroundUniforms] = {"uCam0",  "uCam1",  "uStoneA", "uStoneB", "uMortar", "uCurb",  "uGrassA",
                                              "uGrassB", "uHaze",  "uTexP",   "uSkyTop", "uSkyHor", "uBackP"};
        for (int i = 0; i < kGroundUniforms; ++i) uGround_[i] = glGetUniformLocation(pGround_, names[i]);
    }
    uBright_ = glGetUniformLocation(pBright_, "uBright");
    uBlurDir_ = glGetUniformLocation(pBlur_, "uDir");
    uPost_ = glGetUniformLocation(pPost_, "uPost");
    uPostLight_ = glGetUniformLocation(pPost_, "uLight");
    uPostGrade_ = glGetUniformLocation(pPost_, "uGrade");
    uPostEdge_ = glGetUniformLocation(pPost_, "uEdge");
    {
        const u8 clear[4] = {0, 0, 0, 0};
        glGenTextures(1, &blank2D_);
        glBindTexture(GL_TEXTURE_2D, blank2D_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, clear);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    {
        const u8 clear[4] = {0, 0, 0, 0};
        glGenTextures(1, &noSprites_);
        glBindTexture(GL_TEXTURE_2D_ARRAY, noSprites_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, clear);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        sprites_ = noSprites_;
    }
    uStickerMode_ = glGetUniformLocation(pSticker_, "uMode");
    uStickerShift_ = glGetUniformLocation(pSticker_, "uShift");
    uPageRect_ = glGetUniformLocation(pPage_, "uRect");
    uPageUV_ = glGetUniformLocation(pPage_, "uUV");
    uPageParams_ = glGetUniformLocation(pPage_, "uPage");
    uTableA_ = glGetUniformLocation(pTable_, "uTableA");
    uTableB_ = glGetUniformLocation(pTable_, "uTableB");
    uTableLine_ = glGetUniformLocation(pTable_, "uTableLine");
    uShadow_ = glGetUniformLocation(pShadow_, "uShadow");
    uCurl_ = glGetUniformLocation(pCurl_, "uCurl");
    uSpan_ = glGetUniformLocation(pCurl_, "uSpan");
    uBack_ = glGetUniformLocation(pCurl_, "uBack");
    uTextSpread_ = glGetUniformLocation(pText_, "uSpread");
    uTextOpacity_ = glGetUniformLocation(pText_, "uOpacity");

    ubo_ = makeBuffer(GL_UNIFORM_BUFFER, sizeof(FrameBlock), nullptr, GL_DYNAMIC_DRAW);
    stickerVao(vaoMain_, vboMain_, kMaxStickers);
    stickerVao(vaoOver_, vboOver_, kMaxStickers);
    stickerVao(vaoOne_, vboOne_, 4);
    stickerVao(vaoBake_, vboBake_, kMaxStickers);
    stickerVao(vaoCast_, vboCast_, kMaxStickers);

    // Text and UI: per-vertex attributes, a shared quad index buffer.
    u16* quadIdx = new u16[UiBatch::kMaxQuads * 6];
    for (int q = 0; q < UiBatch::kMaxQuads; ++q) {
        const u16 b = u16(q * 4);
        const u16 idx[6] = {b, u16(b + 1), u16(b + 2), u16(b + 2), u16(b + 1), u16(b + 3)};
        std::memcpy(quadIdx + q * 6, idx, sizeof idx);
    }
    glGenVertexArrays(1, &vaoText_);
    glBindVertexArray(vaoText_);
    iboQuads_ = makeBuffer(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16) * UiBatch::kMaxQuads * 6, quadIdx, GL_STATIC_DRAW);
    delete[] quadIdx;
    vboText_ = makeBuffer(GL_ARRAY_BUFFER, sizeof(UiVertex) * UiBatch::kMaxQuads * 4, nullptr, GL_STREAM_DRAW);
    attrib(0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), offsetof(UiVertex, x), 0);
    attrib(1, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), offsetof(UiVertex, u), 0);
    attrib(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UiVertex), offsetof(UiVertex, fill), 0);
    attrib(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UiVertex), offsetof(UiVertex, stroke), 0);
    attrib(4, 4, GL_FLOAT, GL_FALSE, sizeof(UiVertex), offsetof(UiVertex, p0), 0);
    attrib(5, 4, GL_FLOAT, GL_FALSE, sizeof(UiVertex), offsetof(UiVertex, p1), 0);

    // The page turn's strip: kCurlRows rows across the fold direction.
    {
        float* v = new float[(kCurlRows + 1) * 4];
        for (int i = 0; i <= kCurlRows; ++i) {
            const float t = float(i) / float(kCurlRows);
            v[i * 4 + 0] = -1.0f;
            v[i * 4 + 1] = t;
            v[i * 4 + 2] = 1.0f;
            v[i * 4 + 3] = t;
        }
        glGenVertexArrays(1, &vaoCurl_);
        glBindVertexArray(vaoCurl_);
        vboCurl_ = makeBuffer(GL_ARRAY_BUFFER, GLsizeiptr(sizeof(float) * 4 * (kCurlRows + 1)), v, GL_STATIC_DRAW);
        attrib(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0, 0);
        delete[] v;
    }
    glGenVertexArrays(1, &vaoEmpty_);
    glBindVertexArray(0);

    int aw = 0, ah = 0;
    const unsigned char* pixels = fontAtlas(aw, ah);
    glGenTextures(1, &atlas_);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, aw, ah, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLint mt = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mt);
    maxTex_ = mt > 0 ? (mt > 8192 ? 8192 : mt) : 2048;
    w_ = h_ = 0;
    resize(w, h);
    checkGl("Paper::init");
    return true;
}

void Paper::destroy() {
    for (GLuint* p : {&pSticker_, &pPage_, &pTable_, &pShadow_, &pCurl_, &pText_, &pGround_, &pBright_, &pBlur_, &pPost_}) {
        if (*p) glDeleteProgram(*p);
        *p = 0;
    }
    for (GLuint* b : {&ubo_, &vboMain_, &vboOver_, &vboOne_, &vboBake_, &vboCast_, &vboText_, &iboQuads_, &vboCurl_}) {
        if (*b) glDeleteBuffers(1, b);
        *b = 0;
    }
    for (GLuint* v : {&vaoMain_, &vaoOver_, &vaoOne_, &vaoBake_, &vaoCast_, &vaoText_, &vaoEmpty_, &vaoCurl_}) {
        if (*v) glDeleteVertexArrays(1, v);
        *v = 0;
    }
    if (atlas_) glDeleteTextures(1, &atlas_);
    atlas_ = 0;
    if (noSprites_) glDeleteTextures(1, &noSprites_);
    noSprites_ = sprites_ = 0;
    if (blank2D_) glDeleteTextures(1, &blank2D_);
    blank2D_ = 0;
    shadowRT_.destroy();
    snap_.destroy();
    page_.destroy();
    layerRT_.destroy();
    sceneRT_.destroy();
    bloomA_.destroy();
    bloomB_.destroy();
    pageW_ = pageH_ = 0;
    w_ = h_ = 0;
}

void Paper::resize(int w, int h) {
    if (w == w_ && h == h_ && shadowRT_.fbo) return;
    w_ = w;
    h_ = h;
    makeR8(shadowRT_, maxv(1, w / 2), maxv(1, h / 2));
    snap_.create(w, h, false, true);
    sceneRT_.create(w, h, false, true);
    bloomA_.create(maxv(1, w / 4), maxv(1, h / 4), false, true);
    bloomB_.create(maxv(1, w / 4), maxv(1, h / 4), false, true);
}

void Paper::setScreen(int w, int h, float time) {
    FrameBlock fb{};
    fb.screen[0] = float(w);
    fb.screen[1] = float(h);
    fb.screen[2] = 1.0f / float(w);
    fb.screen[3] = 1.0f / float(h);
    fb.time[0] = time;
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof fb, &fb);
    glBindBufferBase(GL_UNIFORM_BUFFER, kFrameBinding, ubo_);
}

void Paper::drawStickers(GLuint vao, GLuint vbo, const Sticker* s, int n, bool upload, float mode, float extraSoft, float alpha,
                         float liftScale) {
    if (n <= 0) return;
    n = minv(n, kMaxStickers);
    if (upload) stream(vbo, GLsizeiptr(sizeof(Sticker) * size_t(kMaxStickers)), s, GLsizeiptr(sizeof(Sticker) * size_t(n)));
    glUseProgram(pSticker_);
    glUniform4f(uStickerMode_, mode, extraSoft, alpha, liftScale);
    glUniform4f(uStickerShift_, 0, 0, 1, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, sprites_ ? sprites_ : noSprites_);
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n);
    glBindVertexArray(0);
    ++drawCalls_;
}

void Paper::drawBatch(const UiBatch& b, float opacity) {
    if (b.quads() == 0) return;
    stream(vboText_, GLsizeiptr(sizeof(UiVertex) * UiBatch::kMaxQuads * 4), b.vertices(),
           GLsizeiptr(sizeof(UiVertex) * size_t(b.quads()) * 4));
    glUseProgram(pText_);
    glUniform1f(uTextSpread_, fontSpread());
    glUniform1f(uTextOpacity_, opacity);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glBindVertexArray(vaoText_);
    glDrawElements(GL_TRIANGLES, b.quads() * 6, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
    ++drawCalls_;
}

bool Paper::bakePage(int pw, int ph, const BakeLayer* layers, int layerCount) {
    pw = clampv(pw, 16, maxTex_);
    ph = clampv(ph, 16, maxTex_);
    if (pw != pageW_ || ph != pageH_ || !page_.fbo) {
        if (!page_.create(pw, ph, false, true)) return false;
        if (!makeR8(layerRT_, pw, ph)) return false;
        pageW_ = pw;
        pageH_ = ph;
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    setScreen(pw, ph);
    page_.bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    for (int l = 0; l < layerCount; ++l) {
        const BakeLayer& L = layers[l];
        if (L.n <= 0) continue;
        stream(vboBake_, GLsizeiptr(sizeof(Sticker) * size_t(kMaxStickers)), L.s, GLsizeiptr(sizeof(Sticker) * size_t(minv(L.n, kMaxStickers))));
        if (L.shadowAlpha > 0) {
            layerRT_.bind();
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glBlendEquation(GL_MAX);
            glBlendFunc(GL_ONE, GL_ONE);
            drawStickers(vaoBake_, vboBake_, L.s, L.n, false, 1.0f, 0.0f, 1.0f, 1.0f);
            glBlendEquation(GL_FUNC_ADD);
            page_.bind();
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(pShadow_);
            float c[3];
            rgb3(col::kShadow, c);
            glUniform4f(uShadow_, c[0], c[1], c[2], L.shadowAlpha);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, layerRT_.color);
            glBindVertexArray(vaoEmpty_);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        page_.bind();
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        drawStickers(vaoBake_, vboBake_, L.s, L.n, false, 0.0f, 0.0f, 1.0f, 1.0f);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    checkGl("Paper::bakePage");
    return true;
}

void Paper::drawGround(const GroundParams& g) {
    glDisable(GL_BLEND);
    glUseProgram(pGround_);
    float c[3];
    glUniform4f(uGround_[0], g.cam0[0], g.cam0[1], g.cam0[2], g.cam0[3]);
    glUniform4f(uGround_[1], g.cam1[0], g.cam1[1], g.cam1[2], g.cam1[3]);
    const u32 cols[7] = {g.stoneA, g.stoneB, g.mortar, g.curb, g.grassA, g.grassB, g.haze};
    const float ws[7] = {g.roadHalf, 0, 0, 0, 0, g.speckle, g.hazeStart};
    for (int i = 0; i < 7; ++i) {
        rgb3(cols[i], c);
        glUniform4f(uGround_[2 + i], c[0], c[1], c[2], ws[i]);
    }
    const bool tex = g.roadTex && g.landTex;
    glUniform4f(uGround_[9], tex ? 1.0f : 0.0f, g.roadWidth, g.roadTile, g.landTile);
    rgb3(g.skyTop, c);
    glUniform4f(uGround_[10], c[0], c[1], c[2], g.hazeEnd);
    rgb3(g.skyHorizon, c);
    glUniform4f(uGround_[11], c[0], c[1], c[2], 0);
    const bool back = tex && g.backdropTex && g.backdropHeight > 1.0f;
    glUniform4f(uGround_[12], back ? 1.0f : 0.0f, g.backdropHeight, g.backdropShift, g.backdropHaze);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, back ? g.backdropTex : blank2D_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex ? g.landTex : blank2D_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex ? g.roadTex : blank2D_);
    glBindVertexArray(vaoEmpty_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    ++drawCalls_;
}

void Paper::fullscreen() {
    glBindVertexArray(vaoEmpty_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    ++drawCalls_;
}

// Bloom at a quarter of the size (a bright pass, a separable blur), then the world, the glow, a grade and a vignette.
void Paper::postProcess(const PaperFrame& f, GLuint fbo, int w, int h) {
    const PostParams& p = f.post;
    glDisable(GL_BLEND);
    if (p.bloom > 0) {
        bloomA_.bind();
        glUseProgram(pBright_);
        glUniform4f(uBright_, 1.0f / float(w), 1.0f / float(h), p.threshold, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneRT_.color);
        fullscreen();
        glUseProgram(pBlur_);
        for (int pass = 0; pass < 2; ++pass) {
            const float spread = pass == 0 ? 1.0f : 2.0f;
            bloomB_.bind();
            glUniform4f(uBlurDir_, spread / float(bloomA_.w), 0, 0, 0);
            glBindTexture(GL_TEXTURE_2D, bloomA_.color);
            fullscreen();
            bloomA_.bind();
            glUniform4f(uBlurDir_, 0, spread / float(bloomB_.h), 0, 0);
            glBindTexture(GL_TEXTURE_2D, bloomB_.color);
            fullscreen();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    glUseProgram(pPost_);
    glUniform4f(uPost_, p.bloom, p.vignette, p.lightStrength, float(w) / float(maxv(1, h)));
    float c[3];
    rgb3(p.light, c);
    glUniform4f(uPostLight_, c[0], c[1], c[2], 1);
    glUniform4f(uPostGrade_, p.contrast, p.saturation, p.hudTop, 0);
    rgb3(p.edgeColor, c);
    glUniform4f(uPostEdge_, c[0], c[1], c[2], p.edge);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.color);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneRT_.color);
    fullscreen();
}

void Paper::drawScene(const PaperFrame& f, GLuint fbo, int w, int h, bool table) {
    const bool post = f.post.on && sceneRT_.fbo && sceneRT_.w == w && sceneRT_.h == h &&
                      (f.post.bloom > 0 || f.post.lightStrength > 0 || f.post.vignette > 0 || f.post.contrast != 1.0f ||
                       f.post.saturation != 1.0f || f.post.edge > 0);
    sprites_ = f.spriteTex ? f.spriteTex : noSprites_;
    drawWorld(f, post ? sceneRT_.fbo : fbo, w, h, table);
    if (post) postProcess(f, fbo, w, h);
    if (f.labels) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, w, h);
        setScreen(w, h, f.time);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        drawBatch(*f.labels, f.labelOpacity);
    }
}

void Paper::drawWorld(const PaperFrame& f, GLuint fbo, int w, int h, bool table) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    setScreen(w, h, f.time);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBlendEquation(GL_FUNC_ADD);
    if (f.ground.on) {
        drawGround(f.ground);
    } else if (table) {
        glDisable(GL_BLEND);
        glUseProgram(pTable_);
        float a[3], b[3], c[3];
        rgb3(f.tableA, a);
        rgb3(f.tableB, b);
        rgb3(f.tableLine, c);
        glUniform4f(uTableA_, a[0], a[1], a[2], 1);
        glUniform4f(uTableB_, b[0], b[1], b[2], 1);
        glUniform4f(uTableLine_, c[0], c[1], c[2], f.planks);
        glBindVertexArray(vaoEmpty_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        ++drawCalls_;
    } else {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (f.pageW > 0 && page_.fbo) {
        if (table) {  // the page lies on the table: a soft shadow under it
            Sticker s = makeSticker(kShBox, f.pageX + f.pageW * 0.5f + 2.0f, f.pageY + f.pageH * 0.5f + 14.0f, f.pageW * 0.5f,
                                    f.pageH * 0.5f, f.shadowColor, 0.55f);
            s.p0 = 0.035f;
            s.soft = 30.0f;
            drawStickers(vaoOne_, vboOne_, &s, 1, true, 0.0f, 0.0f, 1.0f, 0.0f);
        }
        glUseProgram(pPage_);
        glUniform4f(uPageRect_, f.pageX, f.pageY, f.pageW, f.pageH);
        glUniform4f(uPageUV_, 0.0f, f.pageV0, 1.0f, f.pageV1);
        glUniform4f(uPageParams_, f.grain, 1.0f, 0, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, page_.color);
        glBindVertexArray(vaoEmpty_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        ++drawCalls_;
    }
    if (f.stickerCount > 0) {
        // Every standing thing's shadow at once: the union of the silhouettes at half resolution, then one dark layer.
        stream(vboMain_, GLsizeiptr(sizeof(Sticker) * size_t(kMaxStickers)), f.stickers,
               GLsizeiptr(sizeof(Sticker) * size_t(minv(f.stickerCount, kMaxStickers))));
        glBindFramebuffer(GL_FRAMEBUFFER, shadowRT_.fbo);
        glViewport(0, 0, shadowRT_.w, shadowRT_.h);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glBlendEquation(GL_MAX);
        glBlendFunc(GL_ONE, GL_ONE);
        if (f.shadows) drawStickers(vaoCast_, vboCast_, f.shadows, f.shadowCount, true, 1.0f, 1.5f, 1.0f, 1.0f);
        else drawStickers(vaoMain_, vboMain_, f.stickers, f.stickerCount, false, 1.0f, 1.5f, 1.0f, 1.0f);
        glBlendEquation(GL_FUNC_ADD);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, w, h);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(pShadow_);
        float c[3];
        rgb3(f.shadowColor, c);
        glUniform4f(uShadow_, c[0], c[1], c[2], f.shadowStrength);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, shadowRT_.color);
        glBindVertexArray(vaoEmpty_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        ++drawCalls_;
        drawStickers(vaoMain_, vboMain_, f.stickers, f.stickerCount, false, 0.0f, 0.0f, 1.0f, 1.0f);
    }
    if (f.overCount > 0) drawStickers(vaoOver_, vboOver_, f.over, f.overCount, true, 0.0f, 0.0f, 1.0f, 1.0f);
}

void Paper::drawTurn(int w, int h) {
    const float t = clampv(turnT_, 0.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (turnSlide_) {  // reduce effects: the old page fades
        glUseProgram(pPage_);
        glUniform4f(uPageRect_, 0, 0, float(w), float(h));
        glUniform4f(uPageUV_, 0.0f, 1.0f, 1.0f, 0.0f);
        glUniform4f(uPageParams_, 0.0f, 1.0f - t, 0, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, snap_.color);
        glBindVertexArray(vaoEmpty_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        ++drawCalls_;
        return;
    }
    // The fold direction points from the top-left to the bottom-right corner; the line sweeps from past the
    // bottom-right corner to past the top-left one (TECH_SPEC 5.5).
    float nx = 0.55f, ny = 1.0f;
    const float nl = std::sqrt(nx * nx + ny * ny);
    nx /= nl;
    ny /= nl;
    const float tx = -ny, ty = nx;
    const float cornersX[4] = {0, float(w), 0, float(w)}, cornersY[4] = {0, 0, float(h), float(h)};
    float a0 = 1e9f, a1 = -1e9f, c0 = 1e9f, c1 = -1e9f;
    for (int k = 0; k < 4; ++k) {
        const float along = cornersX[k] * nx + cornersY[k] * ny, across = cornersX[k] * tx + cornersY[k] * ty;
        a0 = minv(a0, along);
        a1 = maxv(a1, along);
        c0 = minv(c0, across);
        c1 = maxv(c1, across);
    }
    const float r = float(w) * 0.12f;
    const float e = t * t * (3.0f - 2.0f * t);  // ease in and out
    const float line = a1 - e * ((a1 - a0) + 2.2f * r);
    glUseProgram(pCurl_);
    glUniform4f(uCurl_, line, r, nx, ny);
    glUniform4f(uSpan_, a0, a1, c0, c1);
    float b[3];
    rgb3(turnBack_, b);
    glUniform4f(uBack_, b[0], b[1], b[2], 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, snap_.color);
    glBindVertexArray(vaoCurl_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (kCurlRows + 1) * 2);
    glBindVertexArray(0);
    ++drawCalls_;
}

void Paper::snapshot(const PaperFrame& f) {
    if (!snap_.fbo) return;
    drawScene(f, snap_.fbo, snap_.w, snap_.h, false);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Paper::draw(const PaperFrame& f, GLuint fbo, int w, int h) {
    drawCalls_ = 0;
    drawScene(f, fbo, w, h, true);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    setScreen(w, h, f.time);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (turnT_ >= 0.0f && turnT_ < 1.0f) drawTurn(w, h);
    if (f.ui) drawBatch(*f.ui, f.uiOpacity);
    glDisable(GL_BLEND);
    checkGl("Paper::draw");
}

}  // namespace pm
