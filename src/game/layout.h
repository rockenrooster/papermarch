#pragma once
// Screen layout (TECH_SPEC 5.2, 5.7): dp units inside the safe area, rectangles, the UI's paper colours, and where the
// page sits on the screen.

#include "game/input.h"
#include "gfx/palette.h"
#include "gfx/text.h"

namespace pm {

struct Layout {
    int w = 1, h = 1;
    float dp = 1;
    SafeArea safe;
    float px(float dpv) const { return dpv * dp; }
    float top() const { return safe.top; }
    float bottom() const { return float(h) - safe.bottom; }
};

struct Rect {
    float x, y, w, h;  // pixels, top-left
    bool contains(float px, float py) const { return px >= x && px <= x + w && py >= y && py <= y + h; }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
};

// The UI's paper colours (GAME_DESIGN 11).
namespace ui {
constexpr u32 kCard = 0xFFF4D6, kCardEdge = 0xE2CDA4;
constexpr u32 kText = col::kText;
constexpr u32 kMuted = 0x8A6A52;
constexpr u32 kPrimary = col::kGold, kPrimaryPressed = 0x8A4E0C;
constexpr u32 kSecondary = col::kIvory, kSecondaryPressed = 0x3A2A22;
constexpr u32 kOn = 0x2E9E58, kOff = 0xC2B49C;
}  // namespace ui

// The HUD's bands (GAME_DESIGN 11): the top row and the bottom row, in dp, inside the safe area.
constexpr float kTopBandDp = 100.0f, kBottomBandDp = 132.0f;  // the top band holds the HUD row and the wave queue

// Where the page lies on the screen: its top-left corner in pixels, pixels per map unit, and how many map units of it
// show at once (a Gauntlet's page is taller and scrolls).
struct PageFit {
    float x = 0, y = 0, k = 1;
    float viewH = 160;   // map units visible
    float screenTop = 0, screenBottom = 0;  // the band the page may use, pixels
};
PageFit fitPage(const Layout& L, float pageH);

Rect centered(const Layout& L, float cy, float wDp, float hDp);
void formatThousands(long long v, char* buf, int n);   // 12345 -> "12,345"
void formatHost(long long v, char* buf, int n);        // in full to 999,999, then "1.24M"

}  // namespace pm
