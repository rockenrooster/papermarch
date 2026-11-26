#pragma once
// Every sound of GAME_DESIGN 12, synthesized once at start-up from small recipes into mono float buffers (TECH_SPEC 6).

#include <vector>

#include "core/types.h"

namespace gt::audio {

enum SoundId {
    kSndPip, kSndTick, kSndPff, kSndChip, kSndShatterSmall, kSndShatterBig, kSndThump, kSndCrack, kSndPopLow, kSndCoreHit,
    kSndWinChord, kSndStar0, kSndStar1, kSndStar2, kSndLose, kSndUiTap, kSndPulse,
    kSndTink,        // a geode hit: a short glass pluck at C5, played up the ladder as its health falls
    kSndGeodeOpen,   // a bright crack and three glass notes rising
    kSndLadder0,  // eleven ladder notes: C4 D4 E4 G4 A4 C5 D5 E5 G5 A5 C6
    kSndPluck0 = kSndLadder0 + 11,  // the music's plucks: the same eleven notes, softer
    kSndCount = kSndPluck0 + 11
};

struct Sound {
    const float* data = nullptr;
    int len = 0;
};

class Bank {
public:
    // timbre: 0 kalimba, 1 glass, 2 bell (the ladder; GAME_DESIGN 12, P4 owner checkpoint).
    void build(int sampleRate, int timbre);
    const Sound& get(int id) const { return s_[id]; }
    int rate() const { return rate_; }

private:
    int rate_ = 48000;
    std::vector<float> store_[kSndCount];
    Sound s_[kSndCount];
};

// A world's music (VARIETY_DESIGN 15): two chords, each two bars of the four-bar loop, and a tempo of 70 to 84 beats a
// minute. Every note is in C major pentatonic (the ladder's family), voiced from A3 up: a phone speaker has no bass.
struct MusicStyle {
    int chords[2][5];
    float bpm;
};
const MusicStyle& worldMusic(int world);  // 1..10
// A style's pad: the four-bar loop, dark and bright (the tide crossfades them); allocates, so not on the audio thread.
void buildPad(int rate, const MusicStyle& m, std::vector<float>& dark, std::vector<float>& bright);

float midiHz(float midi);
extern const int kLadderMidi[11];

}  // namespace gt::audio
