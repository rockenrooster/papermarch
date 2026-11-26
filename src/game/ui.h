#pragma once
// Immediate-mode UI (TECH_SPEC 5.11): buttons, switches and a hold-to-confirm button in dp inside the safe area. A
// touch that begins on a control belongs to the UI until it ends. Every control is recorded for the layout lint and
// for the flow test, which finds controls by label.

#include "game/layout.h"

namespace pm {

struct UiControl {
    Rect r;
    char label[32];
    float textWidth = 0;   // pixels
    u32 textColor = 0, fillColor = 0;
    bool interactive = true;
};

class Ui {
public:
    static constexpr int kMaxControls = 72;  // the level selector draws 47
    void begin(UiBatch* batch, const Layout& L, float opacity = 1.0f);
    // Pointer events (frame pixels). Returns true if the UI takes this touch.
    bool pointerDown(i64 id, float x, float y);
    void pointerMove(i64 id, float x, float y);
    bool pointerUp(i64 id, float x, float y);

    enum Style { kPrimary, kSecondary, kRound };
    bool button(const char* label, const Rect& r, Style style = kSecondary, float textScale = 1.0f);
    bool toggle(const char* label, const Rect& r, bool& value);
    // Fires once after being held for `seconds`; `progress` 0..1 while held.
    bool holdButton(const char* label, const Rect& r, float seconds, float dt, float& progress);
    void text(const char* s, float x, float y, float sizeDp, u32 color, Align align = Align::Center, float alpha = 1.0f);
    void panel(const Rect& r, float radiusDp, float alpha = 0.86f);
    // A HUD element for the layout lint (the boon tray, the tide counter): inside the safe area and clear of the others.
    void note(const Rect& r, const char* label) { record(r, label, 0, 0, 0, false); }

    // Hit test against the controls drawn last frame.
    bool hitsControl(float x, float y) const;
    const UiControl* find(const char* label) const;
    int controlCount() const { return prevCount_; }
    const UiControl& control(int i) const { return prev_[i]; }
    // Layout lint (TECH_SPEC 8.4) on the controls of the last frame: 0 problems = pass. Messages are logged.
    int lint(const Layout& L) const;
    void end();

    UiBatch* batch = nullptr;
    Layout L;
    float opacity = 1;

private:
    bool hot(const Rect& r, const char* label);
    void record(const Rect& r, const char* label, float textWidth, u32 text, u32 fill, bool interactive);
    UiControl cur_[kMaxControls], prev_[kMaxControls];
    int curCount_ = 0, prevCount_ = 0;
    i64 active_ = 0;
    bool activeDown_ = false;
    char activeLabel_[32] = "";
    float px_ = 0, py_ = 0;
    bool released_ = false;
    char releasedLabel_[32] = "";
    float holdTime_ = 0;
};

}  // namespace pm
