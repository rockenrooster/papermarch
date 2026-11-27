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

// The UI's colours (ART_DIRECTION 15.2: cream toy-box cards, an ink outline, cobalt, gold, red).
namespace ui {
constexpr u32 kInk = 0x16224F;
constexpr u32 kCard = 0xFFF6E0, kCardShade = 0xE9DCC2, kCardEdge = kInk;
constexpr u32 kText = kInk;
constexpr u32 kMuted = 0x5C6386;
constexpr u32 kCobalt = 0x2F6BFF, kCobaltLight = 0x8AB4FF, kCobaltDark = 0x1440C4;
constexpr u32 kGold = 0xFFC83A, kGoldLight = 0xFFE07A, kGoldDark = 0xD98A00;
constexpr u32 kRed = 0xE3262E, kRedLight = 0xFF7A86, kRedDark = 0xA8141F;
constexpr u32 kPrimary = kGold, kPrimaryPressed = 0xE8A81E;
constexpr u32 kSecondary = kCard, kSecondaryLip = 0xC9B893, kSecondaryPressed = kCobalt;
constexpr u32 kOn = kCobalt, kOff = 0xB8B0A0;
constexpr u32 kDisabled = 0xD8CFBC, kDisabledText = 0x9A927F;
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
