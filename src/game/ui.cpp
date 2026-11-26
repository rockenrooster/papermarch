#include "game/ui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/log.h"
#include "gfx/palette.h"

namespace gt {

void Ui::begin(UiBatch* b, const Layout& layout, float op) {
    batch = b;
    L = layout;
    opacity = op;
    curCount_ = 0;
}

void Ui::end() {
    std::memcpy(prev_, cur_, sizeof(UiControl) * size_t(curCount_));
    prevCount_ = curCount_;
    released_ = false;
}

void Ui::record(const Rect& r, const char* label, float tw, u32 text, u32 fill, bool interactive) {
    if (curCount_ >= kMaxControls) return;
    UiControl& c = cur_[curCount_++];
    c.r = r;
    std::snprintf(c.label, sizeof c.label, "%s", label);
    c.textWidth = tw;
    c.textColor = text;
    c.fillColor = fill;
    c.interactive = interactive;
}

bool Ui::hitsControl(float x, float y) const {
    for (int i = 0; i < prevCount_; ++i)
        if (prev_[i].interactive && prev_[i].r.contains(x, y)) return true;
    return false;
}

const UiControl* Ui::find(const char* label) const {
    for (int i = 0; i < prevCount_; ++i)
        if (prev_[i].interactive && !std::strcmp(prev_[i].label, label)) return &prev_[i];
    return nullptr;
}

bool Ui::pointerDown(i64 id, float x, float y) {
    if (activeDown_) return false;
    for (int i = 0; i < prevCount_; ++i) {
        if (prev_[i].interactive && prev_[i].r.contains(x, y)) {
            active_ = id;
            activeDown_ = true;
            std::snprintf(activeLabel_, sizeof activeLabel_, "%s", prev_[i].label);
            px_ = x;
            py_ = y;
            holdTime_ = 0;
            return true;
        }
    }
    return false;
}

void Ui::pointerMove(i64 id, float x, float y) {
    if (activeDown_ && id == active_) {
        px_ = x;
        py_ = y;
    }
}

bool Ui::pointerUp(i64 id, float x, float y) {
    if (!activeDown_ || id != active_) return false;
    activeDown_ = false;
    // A release inside the control it began on activates it.
    for (int i = 0; i < prevCount_; ++i) {
        if (prev_[i].interactive && !std::strcmp(prev_[i].label, activeLabel_) && prev_[i].r.contains(x, y)) {
            released_ = true;
            std::snprintf(releasedLabel_, sizeof releasedLabel_, "%s", activeLabel_);
        }
    }
    activeLabel_[0] = 0;
    return true;
}

bool Ui::hot(const Rect& r, const char* label) {
    return activeDown_ && !std::strcmp(activeLabel_, label) && r.contains(px_, py_);
}

void Ui::panel(const Rect& r, float radiusDp, float alpha) {
    batch->rect(r.cx(), r.cy(), r.w, r.h, L.px(radiusDp), rgba(kInk, u8(255 * alpha * opacity)), rgba(kWhite, u8(40 * opacity)),
                L.px(1));
}

void Ui::text(const char* s, float x, float y, float sizeDp, u32 color, Align align, float alpha) {
    const float size = L.px(sizeDp);
    const float w = batch->text(s, x, y, size, rgba(color, u8(255 * alpha * opacity)), rgba(kInk, u8(255 * alpha * opacity)),
                                maxv(2.0f, size * 0.08f), align);
    const float x0 = align == Align::Left ? x : align == Align::Center ? x - w * 0.5f : x - w;
    record({x0, y - size * 0.5f, w, size}, s, w, color, kInk, false);
}

bool Ui::button(const char* label, const Rect& r, Style style, float textScale) {
    const bool pressed = hot(r, label);
    const float s = pressed ? 0.94f : 1.0f;
    // Pressed: the button shrinks to 0.94 and inverts - GAME_DESIGN 10. Ink buttons go white with ink text; the gold
    // primary goes deep amber with white text (white on gold is only 1.6:1, the pressed state needs 3:1 or more).
    u32 fill = style == kPrimary ? 0xFFC654 : kInk;
    u32 textc = style == kPrimary ? kInk : kWhite;
    if (pressed) {
        fill = style == kPrimary ? 0x8A4E0C : kWhite;
        textc = style == kPrimary ? kWhite : kInk;
    }
    const float radius = style == kRound ? minv(r.w, r.h) * 0.5f : L.px(16);
    const u8 fa = style == kSecondary && !pressed ? u8(225 * opacity) : u8(255 * opacity);
    batch->rect(r.cx(), r.cy(), r.w * s, r.h * s, radius * s, rgba(fill, fa), rgba(style == kPrimary ? 0xFFEEB0 : kWhite, u8(70 * opacity)),
                L.px(1.5f), pressed ? 0.0f : L.px(4));
    const float size = L.px(style == kPrimary ? 24 : 19) * textScale * s;
    float tw = 0;
    if (label[0] != '#') {  // labels starting with '#' are names for icon buttons
        tw = batch->text(label, r.cx(), r.cy(), size, rgba(textc, u8(255 * opacity)), rgba(fill, 0), 0, Align::Center);
    }
    record(r, label, tw, textc, fill, true);
    const bool fired = released_ && !std::strcmp(releasedLabel_, label);
    if (fired) released_ = false;
    return fired;
}

bool Ui::toggle(const char* label, const Rect& r, bool& value) {
    const bool pressed = hot(r, label);
    const u32 fill = pressed ? kWhite : kInk;
    batch->rect(r.cx(), r.cy(), r.w * (pressed ? 0.97f : 1.0f), r.h * (pressed ? 0.94f : 1.0f), L.px(16), rgba(fill, u8(225 * opacity)),
                rgba(kWhite, u8(60 * opacity)), L.px(1));
    const float size = L.px(19);
    const float tw = batch->text(label, r.x + L.px(18), r.cy(), size, rgba(pressed ? kInk : kWhite, u8(255 * opacity)), rgba(kInk, 0), 0,
                                 Align::Left);
    // The switch: a pill with a knob, aqua when on.
    const float sw = L.px(56), sh = L.px(30);
    const float sx = r.x + r.w - L.px(18) - sw * 0.5f;
    const u32 track = value ? 0x54F2E0 : 0x3A3A66;
    batch->rect(sx, r.cy(), sw, sh, sh * 0.5f, rgba(track, u8(255 * opacity)));
    const float kx = value ? sx + sw * 0.5f - sh * 0.5f : sx - sw * 0.5f + sh * 0.5f;
    batch->rect(kx, r.cy(), sh - L.px(6), sh - L.px(6), (sh - L.px(6)) * 0.5f, rgba(kWhite, u8(255 * opacity)));
    record(r, label, tw + sw + L.px(30), pressed ? kInk : kWhite, fill, true);
    const bool fired = released_ && !std::strcmp(releasedLabel_, label);
    if (fired) {
        released_ = false;
        value = !value;
    }
    return fired;
}

bool Ui::holdButton(const char* label, const Rect& r, float seconds, float dt, float& progress) {
    const bool pressed = hot(r, label);
    if (pressed) {
        holdTime_ += dt;
    } else if (!activeDown_ || std::strcmp(activeLabel_, label)) {
        holdTime_ = 0;
    }
    progress = clampv(holdTime_ / seconds, 0.0f, 1.0f);
    const u32 fill = pressed ? kWhite : kInk;
    batch->rect(r.cx(), r.cy(), r.w, r.h, L.px(16), rgba(fill, u8(225 * opacity)), rgba(0xFF547C, u8(140 * opacity)), L.px(1.5f));
    if (progress > 0) {
        const float w = r.w * progress;
        batch->rect(r.x + w * 0.5f, r.cy(), w, r.h, L.px(16), rgba(0xFF547C, u8(200 * opacity)));
    }
    const float tw = batch->text(label, r.cx(), r.cy(), L.px(19), rgba(pressed ? kInk : kWhite, u8(255 * opacity)), rgba(kInk, 0), 0,
                                 Align::Center);
    record(r, label, tw, pressed ? kInk : kWhite, fill, true);
    if (pressed && holdTime_ >= seconds) {
        holdTime_ = -1e9f;  // once per hold
        return true;
    }
    return false;
}

int Ui::lint(const Layout& lay) const {
    int problems = 0;
    const float minPx = lay.px(48) - 0.5f, gapPx = lay.px(8) - 0.5f;
    for (int i = 0; i < prevCount_; ++i) {
        const UiControl& c = prev_[i];
        if (c.interactive && (c.r.w < minPx || c.r.h < minPx)) {
            GT_WARN("lint: %s is %.0fx%.0f dp (under 48 dp)", c.label, double(c.r.w / lay.dp), double(c.r.h / lay.dp));
            ++problems;
        }
        if (c.r.x < lay.safe.left - 0.5f || c.r.x + c.r.w > float(lay.w) - lay.safe.right + 0.5f || c.r.y < lay.safe.top - 0.5f ||
            c.r.y + c.r.h > float(lay.h) - lay.safe.bottom + 0.5f) {
            GT_WARN("lint: %s leaves the safe area", c.label);
            ++problems;
        }
        if (c.interactive && c.textWidth > c.r.w - lay.px(12) + 0.5f) {
            GT_WARN("lint: %s's text (%.0f px) is clipped by its control (%.0f px)", c.label, double(c.textWidth), double(c.r.w));
            ++problems;
        }
        if (c.interactive) {
            const float cr = contrast(hexColor(c.textColor), hexColor(c.fillColor));
            if (cr < 4.5f && c.label[0] != '#') {
                GT_WARN("lint: %s's text contrast %.1f:1 (under 4.5:1)", c.label, double(cr));
                ++problems;
            }
        }
        for (int j = i + 1; j < prevCount_; ++j) {
            const UiControl& d = prev_[j];
            const float gx = maxv(d.r.x - (c.r.x + c.r.w), c.r.x - (d.r.x + d.r.w));
            const float gy = maxv(d.r.y - (c.r.y + c.r.h), c.r.y - (d.r.y + d.r.h));
            if (!c.interactive && !d.interactive && c.label[0] == '#' && d.label[0] == '#' && maxv(gx, gy) < lay.px(4)) {
                GT_WARN("lint: %s and %s overlap", c.label, d.label);
                ++problems;
                continue;
            }
            if (!c.interactive || !d.interactive) continue;
            if (maxv(gx, gy) < gapPx) {
                GT_WARN("lint: %s and %s are closer than 8 dp", c.label, d.label);
                ++problems;
            }
        }
    }
    return problems;
}

}  // namespace gt
