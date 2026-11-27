#include "gfx/text.h"

#include <cmath>

#include <cstring>

namespace pm {

namespace {
#include "gfx/font_atlas.inc"
#include "gfx/font_metrics.inc"
}  // namespace

Rgba rgba(u32 hex, u8 alpha) { return {u8(hex >> 16), u8(hex >> 8), u8(hex), alpha}; }

const FontGlyph* findGlyph(u32 code) {
    for (const FontGlyph& g : kFontGlyphs)
        if (g.code == code) return &g;
    return nullptr;
}

u32 nextCodepoint(const char*& s) {
    const u8 c = u8(*s++);
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && *s) return (u32(c & 0x1F) << 6) | (u8(*s++) & 0x3F);
    if ((c & 0xF0) == 0xE0 && s[0] && s[1]) {
        const u32 cp = (u32(c & 0x0F) << 12) | (u32(u8(s[0]) & 0x3F) << 6) | (u8(s[1]) & 0x3F);
        s += 2;
        return cp;
    }
    return '?';
}

const unsigned char* fontAtlas(int& w, int& h) {
    w = kFontAtlasW;
    h = kFontAtlasH;
    return kFontAtlas;
}
float fontSpread() { return kFontSpread; }
float fontAtlasPxPerEm() { return kFontAtlasPxPerEm; }

void UiBatch::quad(const UiVertex& a, const UiVertex& b, const UiVertex& c, const UiVertex& d) {
    if (quads_ >= kMaxQuads) return;
    UiVertex* o = v_ + quads_ * 4;
    o[0] = a;
    o[1] = b;
    o[2] = c;
    o[3] = d;
    ++quads_;
}

void UiBatch::rect(float cx, float cy, float w, float h, float radius, Rgba fill, Rgba stroke, float strokeW, float shadow) {
    const float m = strokeW + shadow + 2.0f;
    const float hw = w * 0.5f + m, hh = h * 0.5f + m;
    UiVertex base{};
    std::memcpy(base.fill, &fill, 4);
    std::memcpy(base.stroke, &stroke, 4);
    base.p0[0] = 1.0f;
    base.p0[1] = strokeW;
    base.p0[2] = minv(radius, minv(w, h) * 0.5f);
    base.p0[3] = shadow;
    base.p1[0] = w * 0.5f;
    base.p1[1] = h * 0.5f;
    UiVertex a = base, b = base, c = base, d = base;
    a.x = cx - hw; a.y = cy - hh; a.u = -hw; a.v = -hh;
    b.x = cx + hw; b.y = cy - hh; b.u = hw;  b.v = -hh;
    c.x = cx - hw; c.y = cy + hh; c.u = -hw; c.v = hh;
    d.x = cx + hw; d.y = cy + hh; d.u = hw;  d.v = hh;
    quad(a, b, c, d);
}

void UiBatch::sprite(const float uv[5], float cx, float cy, float w, float h, Rgba tint, float angle, float hue) {
    UiVertex base{};
    std::memcpy(base.fill, &tint, 4);
    base.p0[0] = 4.0f;  // a sprite (text.frag)
    base.p1[0] = uv[4];
    base.p1[1] = hue;
    const float hw = w * 0.5f, hh = h * 0.5f, c = std::cos(angle), s = std::sin(angle);
    UiVertex q[4] = {base, base, base, base};
    const float us[4] = {-hw, hw, -hw, hw}, vs[4] = {-hh, -hh, hh, hh};
    const float tu[4] = {uv[0], uv[2], uv[0], uv[2]}, tv[4] = {uv[1], uv[1], uv[3], uv[3]};
    for (int i = 0; i < 4; ++i) {
        q[i].u = tu[i];
        q[i].v = tv[i];
        q[i].x = cx + us[i] * c - vs[i] * s;
        q[i].y = cy + us[i] * s + vs[i] * c;
    }
    quad(q[0], q[1], q[2], q[3]);
}

void UiBatch::rectRotated(float cx, float cy, float w, float h, float radius, float angle, Rgba fill) {
    // The rounded rectangle's distance field lives in its own frame (u, v); only the corners turn.
    const float m = 2.0f;
    const float hw = w * 0.5f + m, hh = h * 0.5f + m;
    const float c = std::cos(angle), s = std::sin(angle);
    UiVertex base{};
    std::memcpy(base.fill, &fill, 4);
    base.p0[0] = 1.0f;
    base.p0[2] = minv(radius, minv(w, h) * 0.5f);
    base.p1[0] = w * 0.5f;
    base.p1[1] = h * 0.5f;
    UiVertex q[4] = {base, base, base, base};
    const float us[4] = {-hw, hw, -hw, hw}, vs[4] = {-hh, -hh, hh, hh};
    for (int i = 0; i < 4; ++i) {
        q[i].u = us[i];
        q[i].v = vs[i];
        q[i].x = cx + us[i] * c - vs[i] * s;
        q[i].y = cy + us[i] * s + vs[i] * c;
    }
    quad(q[0], q[1], q[2], q[3]);
}

void UiBatch::star(float cx, float cy, float r, Rgba fill, Rgba stroke, float strokeW) {
    const float m = r + strokeW + 2.0f;
    UiVertex base{};
    std::memcpy(base.fill, &fill, 4);
    std::memcpy(base.stroke, &stroke, 4);
    base.p0[0] = 2.0f;
    base.p0[1] = strokeW;
    base.p1[0] = r;
    UiVertex a = base, b = base, c = base, d = base;
    a.x = cx - m; a.y = cy - m; a.u = -m; a.v = -m;
    b.x = cx + m; b.y = cy - m; b.u = m;  b.v = -m;
    c.x = cx - m; c.y = cy + m; c.u = -m; c.v = m;
    d.x = cx + m; d.y = cy + m; d.u = m;  d.v = m;
    quad(a, b, c, d);
}

void UiBatch::star5(float cx, float cy, float r, Rgba fill, Rgba stroke, float strokeW) {
    const float m = r + strokeW + 2.0f;
    UiVertex base{};
    std::memcpy(base.fill, &fill, 4);
    std::memcpy(base.stroke, &stroke, 4);
    base.p0[0] = 3.0f;
    base.p0[1] = strokeW;
    base.p1[0] = r;
    UiVertex a = base, b = base, c = base, d = base;
    a.x = cx - m; a.y = cy - m; a.u = -m; a.v = -m;
    b.x = cx + m; b.y = cy - m; b.u = m;  b.v = -m;
    c.x = cx - m; c.y = cy + m; c.u = -m; c.v = m;
    d.x = cx + m; d.y = cy + m; d.u = m;  d.v = m;
    quad(a, b, c, d);
}

float UiBatch::textWidth(const char* s, float size) {
    float w = 0;
    while (*s) {
        const FontGlyph* g = findGlyph(nextCodepoint(s));
        if (g) w += g->adv;
    }
    return w * size;
}

float UiBatch::text(const char* s, float x, float y, float size, Rgba fill, Rgba stroke, float strokeW, Align align,
                    bool centerY, float shadow) {
    const float width = textWidth(s, size);
    float pen = align == Align::Left ? x : align == Align::Center ? x - width * 0.5f : x - width;
    const float base = centerY ? y + capHeight() * size * 0.5f : y;
    const float k = size / kFontAtlasPxPerEm;  // screen px per atlas px
    UiVertex t{};
    std::memcpy(t.fill, &fill, 4);
    std::memcpy(t.stroke, &stroke, 4);
    t.p0[0] = 0.0f;
    t.p0[1] = strokeW;
    t.p0[3] = shadow;
    t.p1[2] = k;
    const float iw = 1.0f / float(kFontAtlasW), ih = 1.0f / float(kFontAtlasH);
    while (*s) {
        const FontGlyph* g = findGlyph(nextCodepoint(s));
        if (!g) continue;
        if (g->w > 1 && g->code != ' ') {
            const float x0 = pen + g->ox * size, y0 = base + g->oy * size;
            const float x1 = x0 + float(g->w) * k, y1 = y0 + float(g->h) * k;
            const float u0 = float(g->x) * iw, v0 = float(g->y) * ih;
            const float u1 = float(g->x + g->w) * iw, v1 = float(g->y + g->h) * ih;
            UiVertex a = t, b = t, c = t, d = t;
            a.x = x0; a.y = y0; a.u = u0; a.v = v0;
            b.x = x1; b.y = y0; b.u = u1; b.v = v0;
            c.x = x0; c.y = y1; c.u = u0; c.v = v1;
            d.x = x1; d.y = y1; d.u = u1; d.v = v1;
            quad(a, b, c, d);
        }
        pen += g->adv * size;
    }
    return width;
}

}  // namespace pm
