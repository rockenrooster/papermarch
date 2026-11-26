#pragma once
// Feedback (GAME_DESIGN 10, 12, 13): the scene's cues become sounds for the mixer and haptic pulses, with the rate
// limits the design asks for. No graphics.

#include "audio/mixer.h"
#include "game/scene.h"

namespace pm {

enum HapticKind { kHapticLight = 0, kHapticMedium = 1, kHapticStrong = 2, kHapticWin = 3, kHapticLose = 4, kHapticDouble = 5 };

class Feedback {
public:
    void init(audio::Mixer* mixer) { mixer_ = mixer; }
    void setEnabled(bool sound, bool haptics) {
        sound_ = sound;
        haptics_ = haptics;
    }
    void cue(const CueEvent& c, double now);
    void uiTap();
    void star(int k, double now);
    void pageTurn();
    // Haptic requests since the last call (the platform plays them).
    int takeHaptics(int* out, int max);

private:
    void play(int sound, float gain, float pan, float pitch = 1.0f, float delay = 0.0f, bool onBeat = false);
    void haptic(int kind, double now);

    audio::Mixer* mixer_ = nullptr;
    bool sound_ = true, haptics_ = true;
    double hapticTimes_[12] = {};
    int hapticHead_ = 0;
    int pending_[16] = {};
    int pendingCount_ = 0;
    double lastFlip_ = -1;
    u32 rng_ = 12345;
};

}  // namespace pm
