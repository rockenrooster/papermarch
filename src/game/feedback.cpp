#include "game/feedback.h"

#include <cmath>

namespace pm {

using namespace audio;

void Feedback::play(int sound, float gain, float pan, float pitch, float delay, bool onBeat) {
    if (!sound_ || !mixer_) return;
    mixer_->play(sound, gain, pan, pitch, delay, onBeat);
}

void Feedback::haptic(int kind, double now) {
    if (!haptics_) return;
    // Twelve a second at most, of wall-clock time (GAME_DESIGN 13).
    if (now - hapticTimes_[hapticHead_] < 1.0) return;
    hapticTimes_[hapticHead_] = now;
    hapticHead_ = (hapticHead_ + 1) % 12;
    if (pendingCount_ < 16) pending_[pendingCount_++] = kind;
}

int Feedback::takeHaptics(int* out, int max) {
    const int n = minv(pendingCount_, max);
    for (int i = 0; i < n; ++i) out[i] = pending_[i];
    pendingCount_ = 0;
    return n;
}

void Feedback::uiTap() { play(kSndTap, 0.7f, 0.0f, 1.0f); }

void Feedback::pageTurn() { play(kSndSwish, 0.8f, 0.2f); }

void Feedback::star(int k, double now) {
    const float pitch[3] = {1.0f, 1.26f, 1.5f};
    play(kSndGlock, 0.8f, float(k - 1) * 0.4f, pitch[k % 3] * 2.0f, 0.0f, true);
    haptic(kHapticLight, now);
}

void Feedback::cue(const CueEvent& c, double now) {
    rng_ = rng_ * 1664525u + 1013904223u;
    const float jitter = 0.96f + 0.08f * float(rng_ >> 24) / 255.0f;
    switch (c.cue) {
        case kCueTap:
            play(kSndTap, 0.6f, c.pan, 0.8f);
            haptic(kHapticLight, now);
            break;
        case kCueStep: play(kSndStep, 0.5f, c.pan, jitter); break;
        case kCueClash:
            play(kSndClash, 0.9f, c.pan, jitter);
            haptic(kHapticMedium, now);
            break;
        case kCueFlip:  // the flip wave: a rustle of paper turning, figure by figure
            if (now - lastFlip_ < 0.05) break;
            lastFlip_ = now;
            for (int k = 0; k < 6; ++k) play(kSndFlip, 0.45f, c.pan + 0.1f * float(k % 3 - 1), 0.9f + 0.05f * float(k), 0.035f * float(k));
            break;
        case kCueCount:
            if (mixer_ && sound_) mixer_->ladder(c.step, false, 0.55f, c.pan);
            break;
        case kCueCountDown:
            if (mixer_ && sound_) mixer_->ladder(c.step, true, 0.5f, c.pan);
            break;
        case kCueVillage:
            for (int k = 0; k < 3; ++k) play(kSndGlock, 0.6f, c.pan, std::pow(2.0f, float(k * 4 - (k == 2 ? 1 : 0)) / 12.0f), 0.09f * float(k));
            haptic(kHapticLight, now);
            break;
        case kCueShrine: {
            const int run[8] = {0, 2, 4, 5, 7, 9, 11, 12};
            for (int k = 0; k < 8; ++k) play(kSndGlock, 0.55f, c.pan, std::pow(2.0f, float(run[k]) / 12.0f), 0.05f * float(k));
            play(kSndCrash, 0.25f, c.pan, 1.2f, 0.3f);
            haptic(kHapticMedium, now);
            break;
        }
        case kCuePost: play(kSndCoin, 0.7f, c.pan); break;
        case kCueMire: play(kSndReed, 0.8f, c.pan); break;
        case kCueBurn: play(kSndCrackle, 0.7f, c.pan); break;
        case kCueOpen:
            for (int k = 0; k < 3; ++k) play(kSndRatchet, 0.6f, c.pan, 1.0f - 0.1f * float(k), 0.06f * float(k));
            break;
        case kCueRival:
            play(kSndDeepDrum, 0.8f, c.pan);
            break;
        case kCueRaid:
            play(kSndHorn, 0.75f, c.pan);
            play(kSndDeepDrum, 0.6f, c.pan, 1.0f, 0.2f);
            break;
        case kCueWin:
            play(kSndFanfare, 0.95f, 0.0f, 1.0f, 0.0f, true);
            play(kSndChoir, 0.5f, 0.0f, 1.0f, 0.5f);
            haptic(kHapticWin, now);
            break;
        case kCueKeepFall:
            play(kSndClash, 1.0f, 0.0f, 0.8f);
            play(kSndCrash, 0.5f, 0.0f, 1.0f, 0.05f);
            play(kSndFanfare, 0.95f, 0.0f, 1.0f, 0.25f);
            play(kSndChoir, 0.45f, 0.0f, 1.0f, 0.7f);
            haptic(kHapticWin, now);
            break;
        case kCueLose:
            play(kSndLoseHorn, 0.85f, 0.0f);
            haptic(kHapticLose, now);
            break;
        case kCueRefuse:
            play(kSndKnock, 0.8f, c.pan);
            haptic(kHapticDouble, now);
            break;
        case kCueTurn: play(kSndRim, 0.5f, 0.0f, 0.8f); break;
        case kCueUnfold: play(kSndRatchet, 0.35f, c.pan, jitter); break;
        case kCueStar: star(c.step, now); break;
        default: break;
    }
}

}  // namespace pm
