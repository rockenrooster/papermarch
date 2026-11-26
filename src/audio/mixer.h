#pragma once
// The mixer (TECH_SPEC 7): voices of baked sounds, the march's step sequencer (GAME_DESIGN 12.1: five layers that join
// as the host nears its goal), the count ladder, a soft limiter. Pure code, no SDL. The game thread sends commands
// through a lock-free single-producer single-consumer ring; the audio thread mixes, never locking, allocating or logging.

#include <atomic>

#include "audio/synth.h"

namespace pm::audio {

struct Command {
    u8 type = 0;   // 0 play, 1 duck, 2 enable, 3 world, 4 layers, 5 ladder
    u8 sound = 0;
    u8 flags = 0;  // play: 1 on the next sixteenth
    float gain = 1, pan = 0, pitch = 1, delay = 0;  // delay in seconds
    float a = 0, b = 0;
};

class Mixer {
public:
    static constexpr int kVoices = 48;
    static constexpr int kLayers = 5;
    void init(int sampleRate);
    // Game thread.
    bool push(const Command& c);  // false if the ring is full (the command is dropped and counted)
    void play(int sound, float gain, float pan, float pitch = 1.0f, float delay = 0.0f, bool onBeat = false);
    // The count ladder (GAME_DESIGN 12.2): tick `step` of a count, up (or down) the world's scale from the chord's root.
    void ladder(int step, bool down, float gain, float pan);
    void setWorld(int world, bool boss);
    void setLayers(int layers);   // how many of the march's five layers play (0: none, 1..5)
    void duck(float seconds);
    void setEnabled(bool effects, bool music);
    int dropped() const { return dropped_.load(); }
    int pushed() const { return int(head_.load()); }
    float musicLevel() const { return musicLevelOut_.load(std::memory_order_relaxed); }
    int mixCalls() const { return mixCalls_.load(std::memory_order_relaxed); }
    int voicesInUse() const { return voicesInUse_.load(); }
    // Audio thread.
    void mix(float* stereo, int frames);
    const Bank& bank() const { return bank_; }

private:
    struct Voice {
        int sound = -1;
        float pos = 0, step = 1;
        float gl = 0, gr = 0;
        int delay = 0;
        u32 age = 0;
        bool music = false;
        u8 choke = 0;
        float fade = 1, fadeStep = 0;
    };
    void start(int sound, float gain, float pan, float pitch, int delayFrames, bool music, u8 choke = 0);
    void mixChunk(float* out, int frames);
    void sequence(int frames, float* out);
    void playStep(int step, int offset);
    float noteRatio(int sound, int degree, int octaveShift) const;  // a degree of the world's mode, for `sound`

    Bank bank_;
    int rate_ = 48000;
    Voice voices_[kVoices];
    static constexpr int kRing = 512;
    Command ring_[kRing];
    std::atomic<u32> head_{0}, tail_{0};
    std::atomic<int> dropped_{0}, voicesInUse_{0};
    bool effectsOn_ = true, musicOn_ = true;
    float duck_ = 1, duckTarget_ = 1, duckHold_ = 0;
    // The march.
    int world_ = 1;
    bool boss_ = false;
    int layersWanted_ = 1;
    float layerGain_[kLayers] = {1, 0, 0, 0, 0};
    double stepLen_ = 6000;   // samples a sixteenth
    double toNext_ = 0;       // samples until the next step
    int step_ = 0;            // 0..63 in the eight-bar loop
    float limGain_ = 1;
    u32 voiceAge_ = 0;
    static constexpr int kChunk = 1024;
    float musicLevel_ = 1;
    std::atomic<float> musicLevelOut_{1};
    std::atomic<int> mixCalls_{0};
    float musicEnv_[kChunk] = {};
};

}  // namespace pm::audio
