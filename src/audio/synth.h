#pragma once
// Every sound of GAME_DESIGN 12, synthesized once at start-up from small recipes into mono float buffers (TECH_SPEC 7):
// wood, paper, drums and brass, a toy marching band. Pitched instruments are one note each, played at other pitches by
// the mixer's resampling.

#include <vector>

#include "core/types.h"

namespace pm::audio {

enum SoundId {
    // Effects.
    kSndWood,       // the count ladder's wood block (D5)
    kSndFlip,       // a paper flip
    kSndClash,      // drum, a metal ring and a shout
    kSndGlock,      // a glockenspiel note (A5): village chimes, the shrine's run, stars
    kSndCoin,
    kSndReed,       // the mire: a low reed falling a fourth
    kSndKnock,      // a refused tap: two low knocks
    kSndCrackle,    // a fork's bridge burning
    kSndHorn,       // a raid or the rival: two low horn notes
    kSndDeepDrum,
    kSndFanfare,    // the win: a brass arpeggio into a chord
    kSndChoir,
    kSndLoseHorn,   // a horn falling a fifth, then paper tearing
    kSndSwish,      // the page turn
    kSndRatchet,    // a pop-up standing
    kSndTap,        // a UI tap
    kSndStep,       // a footstep
    // The march (GAME_DESIGN 12.1).
    kSndFrameDrum,
    kSndSnare,
    kSndRim,
    kSndTom,
    kSndCymbalSwell,
    kSndCrash,
    kSndBass,       // tuba-like, A2
    kSndFife,       // the melody, A4
    kSndBrass,      // harmony, A3
    kSndDrone,      // an open fifth, A3 + E4
    kSndTimpani,    // the boss's pedal, A2
    kSndShaker,
    kSndCount
};

struct Sound {
    const float* data = nullptr;
    int len = 0;
};

class Bank {
public:
    void build(int sampleRate);
    const Sound& get(int id) const { return s_[id]; }
    int rate() const { return rate_; }

private:
    int rate_ = 48000;
    std::vector<float> store_[kSndCount];
    Sound s_[kSndCount];
};

// The base pitch of a pitched sound (Hz): the mixer's pitch ratio for a target frequency is target / base.
float baseHz(int sound);
float midiHz(float midi);

// A world's march (GAME_DESIGN 12.1): its mode (semitones of the seven degrees), tonic (MIDI), tempo, swing, lead.
struct MarchStyle {
    int mode[7];
    int tonic;      // MIDI note of degree 1 for the lead (an octave above the bass's)
    float bpm;
    float swing;    // 0 straight .. 0.33 a triplet feel
    int lead;       // 0 fife, 1 plucked, 2 reed, 3 bell
};
const MarchStyle& marchStyle(int world);  // 1..10

}  // namespace pm::audio
