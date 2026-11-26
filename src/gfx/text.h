#pragma once
// Text and UI shapes (TECH_SPEC 5.7, 5.11): one batch of screen-space quads drawn by shaders/text.*, either a glyph
// from the signed-distance-field atlas or a rounded rectangle; fill, stroke and a soft shadow from one distance.

#include "core/types.h"

namespace pm {

struct FontGlyph {
    u16 code, x, y, w, h;
    float ox, oy, adv;  // em
};

struct UiVertex {
    float x, y;    // pixels, top-left origin
    float u, v;    // atlas uv (text) or local pixels from the centre (rect)
    u8 fill[4];
    u8 stroke[4];
    float p0[4];   // mode (0 text, 1 rect), stroke width px, corner radius px, shadow px
    float p1[4];   // half size px (rect), screen px per atlas px (text), unused
};

struct Rgba {
    u8 r, g, b, a;
};
Rgba rgba(u32 hex, u8 alpha = 255);  // 0xRRGGBB

enum class Align { Left, Center, Right };

class UiBatch {
public:
    static constexpr int kMaxQuads = 3000;
    void clear() { quads_ = 0; }
    int quads() const { return quads_; }
    const UiVertex* vertices() const { return v_; }

    void rect(float cx, float cy, float w, float h, float radius, Rgba fill, Rgba stroke = {0, 0, 0, 0}, float strokeW = 0,
              float shadow = 0);
    void rectRotated(float cx, float cy, float w, float h, float radius, float angle, Rgba fill);  // angle in radians
    void star(float cx, float cy, float r, Rgba fill, Rgba stroke = {0, 0, 0, 0}, float strokeW = 0);
    void star5(float cx, float cy, float r, Rgba fill, Rgba stroke = {0, 0, 0, 0}, float strokeW = 0);  // five points, up
    // `size` is the em size in pixels; y is the baseline unless `centerY` (then the cap height is centred on y).
    // Returns the width drawn.
    float text(const char* s, float x, float y, float size, Rgba fill, Rgba stroke, float strokeW, Align align = Align::Left,
               bool centerY = true, float shadow = 0);
    static float textWidth(const char* s, float size);
    static float capHeight() { return 0.71f; }  // em

private:
    void quad(const UiVertex& a, const UiVertex& b, const UiVertex& c, const UiVertex& d);
    UiVertex v_[kMaxQuads * 4];
    int quads_ = 0;
};

const FontGlyph* findGlyph(u32 code);
// Decodes one UTF-8 code point; advances s.
u32 nextCodepoint(const char*& s);
const unsigned char* fontAtlas(int& w, int& h);
float fontSpread();       // atlas px
float fontAtlasPxPerEm();

}  // namespace pm
