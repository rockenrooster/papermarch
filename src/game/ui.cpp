#include "game/ui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/log.h"

namespace pm {

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

// A cream toy-box card: an ink outline and a hard drop shadow (ART_DIRECTION 15.2).
void Ui::panel(const Rect& r, float radiusDp, float alpha) {
    const u8 a = u8(255 * minv(1.0f, alpha / 0.86f) * opacity);
    batch->rect(r.cx(), r.cy() + L.px(6), r.w, r.h, L.px(radiusDp), rgba(ui::kInk, u8(102 * opacity)));
    batch->rect(r.cx(), r.cy(), r.w, r.h, L.px(radiusDp), rgba(ui::kCard, a), rgba(ui::kInk, a), L.px(2.5f));
}

// A chunky toy button: a face over a lip of its dark colour; pressed, the face sinks onto the lip, changes colour and
// takes a white outline that shows on any background (ART_DIRECTION 15.2, the owner's rule on pressed states).
void Ui::face(const Rect& r, float radius, u32 faceColor, u32 topColor, u32 lipColor, bool pressed) {
    const u8 a = u8(255 * opacity);
    const float lip = L.px(6), sink = pressed ? L.px(4) : 0.0f;
    batch->rect(r.cx(), r.cy() + lip, r.w, r.h, radius, rgba(lipColor, a), rgba(ui::kInk, a), L.px(2.5f));
    batch->rect(r.cx(), r.cy() + sink, r.w, r.h, radius, rgba(faceColor, a), rgba(pressed ? col::kWhite : ui::kInk, a),
                L.px(pressed ? 3.0f : 2.5f));
    if (topColor != faceColor) {  // a lighter top, inside the outline
        const float th = r.h * 0.45f, inset = L.px(3);
        batch->rect(r.cx(), r.y + sink + inset + th * 0.5f, r.w - inset * 2.0f, th, minv(radius - inset, th * 0.5f), rgba(topColor, a));
    }
}

void Ui::text(const char* s, float x, float y, float sizeDp, u32 color, Align align, float alpha) {
    const float size = L.px(sizeDp);
    const float w = batch->text(s, x, y, size, rgba(color, u8(255 * alpha * opacity)), {0, 0, 0, 0}, 0, align);
    const float x0 = align == Align::Left ? x : align == Align::Center ? x - w * 0.5f : x - w;
    record({x0, y - size * 0.5f, w, size}, s, w, color, ui::kCard, false);
}

bool Ui::button(const char* label, const Rect& r, Style style, float textScale, float labelShift, float labelDy) {
    const bool pressed = hot(r, label);
    // Primary: gold with a light top, ink label; pressed it goes amber. Secondary and round: cream, ink; pressed they
    // turn cobalt with a white label. Danger: red, white label; pressed red dark (ART_DIRECTION 15.2).
    const bool primary = style == kPrimary, danger = style == kDanger, muted = style == kMuted;
    u32 fill = primary ? ui::kPrimary : (danger ? ui::kRed : (muted ? ui::kCardShade : ui::kSecondary));
    u32 textc = danger ? col::kWhite : ui::kText;
    if (pressed) {
        fill = primary ? ui::kPrimaryPressed : (danger ? ui::kRedDark : ui::kSecondaryPressed);
        textc = primary ? ui::kText : col::kWhite;
    }
    const float radius = style == kRound ? minv(r.w, r.h) * 0.5f : L.px(16);
    face(r, radius, fill, primary && !pressed ? ui::kGoldLight : fill, primary ? ui::kGoldDark : (danger ? ui::kRedDark : ui::kSecondaryLip),
         pressed);
    const float sink = pressed ? L.px(4) : 0.0f;
    const u8 a = u8(255 * opacity);
    const float size = L.px(22) * textScale;
    float tw = 0;
    if (label[0] != '#') {  // labels starting with '#' are names for icon buttons
        const u8 la = muted && !pressed ? u8(a / 2) : a;  // a level not played yet: its number at half strength
        tw = batch->text(label, r.cx() + labelShift, r.cy() + sink + labelDy, size, rgba(textc, la), {0, 0, 0, 0}, 0, Align::Center);
        tw += 2.0f * std::fabs(labelShift);  // the icon beside it counts against the room
    }
    record(r, label, tw, textc, fill, true);
    const bool fired = released_ && !std::strcmp(releasedLabel_, label);
    if (fired) released_ = false;
    return fired;
}

bool Ui::toggle(const char* label, const Rect& r, bool& value) {
    const bool pressed = hot(r, label);
    // A row: its label in ink and a pill switch (cobalt with the knob right when on); pressed, the row turns cobalt
    // with a white label and the switch takes a white outline.
    const u32 fill = pressed ? ui::kSecondaryPressed : ui::kSecondary;
    const u32 textc = pressed ? col::kWhite : ui::kText;
    const u8 a = u8(255 * opacity);
    batch->rect(r.cx(), r.cy(), r.w, r.h, L.px(16), rgba(fill, a), rgba(pressed ? col::kWhite : ui::kCardShade, a), L.px(pressed ? 3.0f : 2.0f));
    const float size = L.px(18);
    const float tw = batch->text(label, r.x + L.px(18), r.cy(), size, rgba(textc, a), {0, 0, 0, 0}, 0, Align::Left);
    const float sw = L.px(56), sh = L.px(30);
    const float sx = r.x + r.w - L.px(18) - sw * 0.5f;
    batch->rect(sx, r.cy(), sw, sh, sh * 0.5f, rgba(value ? ui::kOn : ui::kOff, a), rgba(pressed ? col::kWhite : ui::kInk, a),
                L.px(pressed ? 3.0f : 2.5f));
    const float kx = value ? sx + sw * 0.5f - sh * 0.5f : sx - sw * 0.5f + sh * 0.5f;
    batch->rect(kx, r.cy(), L.px(24), L.px(24), L.px(12), rgba(col::kWhite, a), rgba(ui::kInk, a), L.px(1.5f));
    record(r, label, tw + sw + L.px(30), textc, fill, true);
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
    // The danger button: red with a white label; held, it sinks and fills with gold from the left.
    const u32 fill = pressed ? ui::kRedDark : ui::kRed;
    const u32 textc = col::kWhite;
    const u8 a = u8(255 * opacity);
    face(r, L.px(16), fill, fill, ui::kRedDark, pressed);
    const float sink = pressed ? L.px(4) : 0.0f;
    if (progress > 0) {
        const float w = maxv(r.h, r.w * progress);
        batch->rect(r.x + w * 0.5f, r.cy() + sink, w, r.h, L.px(16), rgba(ui::kGold, a), rgba(col::kWhite, a), L.px(3));
    }
    const float tw = batch->text(label, r.cx(), r.cy() + sink, L.px(19), rgba(progress > 0.5f ? ui::kText : textc, a), {0, 0, 0, 0}, 0,
                                 Align::Center);
    record(r, label, tw, textc, fill, true);
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
            PM_WARN("lint: %s is %.0fx%.0f dp (under 48 dp)", c.label, double(c.r.w / lay.dp), double(c.r.h / lay.dp));
            ++problems;
        }
        if (c.r.x < lay.safe.left - 0.5f || c.r.x + c.r.w > float(lay.w) - lay.safe.right + 0.5f || c.r.y < lay.safe.top - 0.5f ||
            c.r.y + c.r.h > float(lay.h) - lay.safe.bottom + 0.5f) {
            PM_WARN("lint: %s leaves the safe area", c.label);
            ++problems;
        }
        if (c.interactive && c.textWidth > c.r.w - lay.px(12) + 0.5f) {
            PM_WARN("lint: %s's text (%.0f px) is clipped by its control (%.0f px)", c.label, double(c.textWidth), double(c.r.w));
            ++problems;
        }
        if (c.interactive && c.label[0] != '#') {
            const float cr = contrast(c.textColor, c.fillColor);
            if (cr < 4.5f) {
                PM_WARN("lint: %s's text contrast %.1f:1 (under 4.5:1)", c.label, double(cr));
                ++problems;
            }
        }
        for (int j = i + 1; j < prevCount_; ++j) {
            const UiControl& d = prev_[j];
            const float gx = maxv(d.r.x - (c.r.x + c.r.w), c.r.x - (d.r.x + d.r.w));
            const float gy = maxv(d.r.y - (c.r.y + c.r.h), c.r.y - (d.r.y + d.r.h));
            if (!c.interactive && !d.interactive && c.label[0] == '#' && d.label[0] == '#' && maxv(gx, gy) < lay.px(4)) {
                PM_WARN("lint: %s and %s overlap", c.label, d.label);
                ++problems;
                continue;
            }
            if (!c.interactive || !d.interactive) continue;
            if (maxv(gx, gy) < gapPx) {
                PM_WARN("lint: %s and %s are closer than 8 dp", c.label, d.label);
                ++problems;
            }
        }
    }
    return problems;
}

}  // namespace pm
