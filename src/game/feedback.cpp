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
    // Twelve a second at most, of wall-clock time.
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
    const float jitter = 0.94f + 0.12f * float(rng_ >> 24) / 255.0f;
    switch (c.cue) {
        case kCueShot: {  // a volley: louder with more guns, never a wall of noise; each weapon its own voice
            const float v = minv(0.75f, 0.42f + 0.04f * float(c.step)), pan = c.pan + (jitter - 1.0f) * 3.0f;
            switch (c.weapon) {
                case sim::kTwin: play(kSndShot, v * 0.85f, pan, jitter * 1.35f); break;
                case sim::kScatter:
                    play(kSndShot, v, pan, jitter * 0.72f);
                    play(kSndKnock, v * 0.5f, pan, jitter * 0.8f);
                    break;
                case sim::kLance: play(kSndShot, v * 0.8f, pan, jitter * 1.9f); break;
                case sim::kRockets: play(kSndSwish, v * 0.9f, pan, jitter * 0.8f); break;
                default: play(kSndShot, v, pan, jitter); break;
            }
            break;
        }
        case kCueWeapon:  // a new weapon: a snare roll and a bright brass note
            play(kSndSnare, 0.7f, c.pan);
            play(kSndHorn, 0.6f, c.pan, 1.5f, 0.06f);
            play(kSndCoin, 0.6f, c.pan, 0.8f, 0.12f);
            haptic(kHapticMedium, now);
            break;
        case kCueBlast: play(kSndBoom, 0.55f, c.pan, jitter * 1.3f); break;
        case kCuePower:  // a power-up: a bright chime, each its own colour
            play(kSndGlock, 0.8f, c.pan, c.step == sim::kFreeze ? 2.6f : (c.step == sim::kShield ? 1.5f : 2.0f));
            if (c.step == sim::kRapid) play(kSndSnare, 0.5f, c.pan, 1.3f, 0.05f);
            else if (c.step == sim::kPowerShot) play(kSndDeepDrum, 0.8f, c.pan, 1.0f, 0.03f);
            else if (c.step == sim::kAirstrike) play(kSndSwish, 0.8f, c.pan, 0.6f, 0.1f);
            else play(kSndGlock, 0.6f, c.pan, c.step == sim::kFreeze ? 3.1f : 1.9f, 0.08f);
            haptic(kHapticMedium, now);
            break;
        case kCueShield:  // a dull tonk as it absorbs; a crash when it breaks
            if (c.step) play(kSndCrash, 0.55f, c.pan, 1.4f);
            else play(kSndKnock, 0.4f, c.pan, jitter * 1.6f);
            break;
        case kCueBomb:
            play(kSndBoom, 0.95f, c.pan, jitter * 0.8f);
            haptic(kHapticStrong, now);
            break;
        case kCueHit: play(kSndThwack, 0.45f, c.pan, jitter * 1.2f); break;
        case kCueKill:
            play(c.step == sim::kBrute ? kSndClash : kSndFlip, c.step == sim::kBrute ? 0.7f : 0.5f, c.pan, jitter);
            break;
        case kCueGrow: {  // a gate shot up: the ladder climbs while the shots keep landing
            if (now - lastGrow_ > 0.5) growStep_ = 0;
            lastGrow_ = now;
            if (mixer_ && sound_) mixer_->ladder(growStep_ % 14, false, 0.32f, c.pan);
            ++growStep_;
            break;
        }
        case kCueGate:
            if (mixer_ && sound_) {
                for (int k = 0; k <= minv(c.step, 4); ++k) mixer_->ladder(c.step + k * 2, false, 0.6f, c.pan);
            }
            play(kSndPop, 0.7f, c.pan, 1.0f + 0.05f * float(minv(c.step, 8)));
            play(kSndCoin, 0.45f, c.pan, 1.0f, 0.05f);
            haptic(kHapticLight, now);
            break;
        case kCueGateBad:
            if (mixer_ && sound_) mixer_->ladder(c.step, true, 0.6f, c.pan);
            play(kSndKnock, 0.7f, c.pan);
            haptic(kHapticDouble, now);
            break;
        case kCueBarrelHit: play(kSndWood, 0.35f, c.pan, jitter * 0.6f); break;
        case kCueBarrel:
            play(kSndBoom, 0.75f, c.pan, jitter * 1.15f);
            play(kSndCoin, 0.6f, c.pan, 1.0f, 0.08f);
            for (int k = 0; k < 3; ++k) play(kSndPop, 0.45f, c.pan, 1.0f + 0.12f * float(k), 0.12f + 0.07f * float(k));
            haptic(kHapticMedium, now);
            break;
        case kCueCrash:
            play(kSndBoom, 0.7f, c.pan, 0.9f);
            play(kSndKnock, 0.6f, c.pan, 1.0f, 0.05f);
            haptic(kHapticStrong, now);
            break;
        case kCueWave:
            play(kSndHorn, 0.7f, c.pan);
            play(kSndDeepDrum, 0.6f, c.pan, 1.0f, 0.2f);
            break;
        case kCueBoss:
            play(kSndDeepDrum, 0.9f, c.pan, 0.8f);
            play(kSndHorn, 0.8f, c.pan, 0.75f, 0.15f);
            play(kSndTimpani, 0.7f, c.pan, 1.0f, 0.4f);
            haptic(kHapticStrong, now);
            break;
        case kCueBossHit: play(kSndThwack, 0.5f, c.pan, 0.6f * jitter); break;
        case kCueBossDown:
            play(kSndBoom, 1.0f, 0.0f, 0.75f);
            play(kSndCrash, 0.6f, 0.0f, 1.0f, 0.05f);
            play(kSndBoom, 0.7f, 0.3f, 1.0f, 0.18f);
            haptic(kHapticWin, now);
            break;
        case kCueLoss:
            play(kSndClash, 0.45f, c.pan, jitter);
            haptic(kHapticLight, now);
            break;
        case kCueWin:
            play(kSndFanfare, 0.95f, 0.0f, 1.0f, 0.0f, true);
            play(kSndChoir, 0.5f, 0.0f, 1.0f, 0.5f);
            haptic(kHapticWin, now);
            break;
        case kCueLose:
            play(kSndLoseHorn, 0.85f, 0.0f);
            haptic(kHapticLose, now);
            break;
        case kCueStart:
            play(kSndSnare, 0.6f, 0.0f);
            play(kSndHorn, 0.5f, 0.0f, 1.0f, 0.05f);
            break;
        default: break;
    }
}

}  // namespace pm
