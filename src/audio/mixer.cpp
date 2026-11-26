#include "audio/mixer.h"

#include <cmath>
#include <cstring>

namespace pm::audio {

namespace {

constexpr float kEffectsGain = 0.42f;
constexpr float kMusicGain = 0.50f;
constexpr int kLoop = 64;  // eight bars of 2/4 in sixteenths

// The theme (GAME_DESIGN 12.1): the degree (1 = the tonic, 8 = its octave) starting at each sixteenth, 0 none.
const int kTheme[kLoop] = {
    1, 0, 1, 2, 3, 0, 5, 0,   // bar 1: 1(2) 1(1) 2(1) 3(2) 5(2)
    6, 0, 5, 0, 3, 0, 0, 0,   // bar 2: 6(2) 5(2) 3(4)
    4, 0, 4, 5, 6, 0, 8, 0,   // bar 3: 4(2) 4(1) 5(1) 6(2) 8(2)
    7, 0, 5, 0, 5, 0, 0, 0,   // bar 4: 7(2) 5(2) 5(4)
    8, 0, 8, 7, 6, 0, 5, 0,   // bar 5: 8(2) 8(1) 7(1) 6(2) 5(2)
    6, 0, 4, 0, 2, 0, 0, 0,   // bar 6: 6(2) 4(2) 2(4)
    3, 0, 5, 0, 4, 0, 2, 0,   // bar 7: 3(2) 5(2) 4(2) 2(2)
    1, 0, 0, 0, 0, 0, 0, 0,   // bar 8: 1(8)
};
// The chord of each bar, as its root's degree (0-based): I I IV V I IV V I.
const int kChord[8] = {0, 0, 3, 4, 0, 3, 4, 0};

int limitOf(int s) {
    switch (s) {
        case kSndWood: return 6;
        case kSndFlip: return 6;
        case kSndGlock: return 10;
        case kSndStep: return 3;
        case kSndRatchet: return 4;
        case kSndFanfare: case kSndLoseHorn: case kSndChoir: return 1;
        default: return 4;
    }
}

}  // namespace

void Mixer::init(int sampleRate) {
    rate_ = sampleRate;
    bank_.build(sampleRate);
    for (Voice& v : voices_) v.sound = -1;
    const MarchStyle& m = marchStyle(world_);
    stepLen_ = double(rate_) * 60.0 / double(m.bpm) / 4.0;
    toNext_ = 0;
    step_ = 0;
}

bool Mixer::push(const Command& c) {
    const u32 h = head_.load(std::memory_order_relaxed);
    const u32 t = tail_.load(std::memory_order_acquire);
    if (h - t >= u32(kRing)) {
        dropped_.fetch_add(1);
        return false;
    }
    ring_[h % kRing] = c;
    head_.store(h + 1, std::memory_order_release);
    return true;
}

void Mixer::play(int sound, float gain, float pan, float pitch, float delay, bool onBeat) {
    Command c;
    c.type = 0;
    c.sound = u8(sound);
    c.flags = onBeat ? 1 : 0;
    c.gain = gain;
    c.pan = pan;
    c.pitch = pitch;
    c.delay = delay;
    push(c);
}

void Mixer::ladder(int step, bool down, float gain, float pan) {
    Command c;
    c.type = 5;
    c.a = float(step);
    c.b = down ? 1.0f : 0.0f;
    c.gain = gain;
    c.pan = pan;
    push(c);
}

void Mixer::setWorld(int world, bool boss) {
    Command c;
    c.type = 3;
    c.a = float(world);
    c.b = boss ? 1.0f : 0.0f;
    push(c);
}

void Mixer::setLayers(int layers) {
    Command c;
    c.type = 4;
    c.a = float(layers);
    push(c);
}

void Mixer::duck(float seconds) {
    Command c;
    c.type = 1;
    c.a = seconds;
    push(c);
}

void Mixer::setEnabled(bool effects, bool music) {
    Command c;
    c.type = 2;
    c.a = effects ? 1.0f : 0.0f;
    c.b = music ? 1.0f : 0.0f;
    push(c);
}

float Mixer::noteRatio(int sound, int degree, int octaveShift) const {
    const MarchStyle& m = marchStyle(world_);
    int oct = degree >= 0 ? degree / 7 : -((-degree + 6) / 7);
    const int d = degree - oct * 7;
    const float midi = float(m.tonic + m.mode[d] + 12 * (oct + octaveShift)) - (boss_ ? 7.0f : 0.0f);
    return midiHz(midi) / baseHz(sound);
}

void Mixer::start(int s, float gain, float pan, float pitch, int delayFrames, bool music, u8 choke) {
    if (s < 0 || s >= kSndCount) return;
    if (!music && !effectsOn_) return;
    if (music && !musicOn_) return;
    if (choke)
        for (Voice& v : voices_)
            if (v.sound >= 0 && v.choke == choke && v.fadeStep == 0) v.fadeStep = -1.0f / (0.012f * float(rate_));
    const int limit = music ? kVoices : limitOf(s);
    int count = 0, oldestSame = -1, freeV = -1, oldest = -1;
    u32 oldestSameAge = ~0u, oldestAge = ~0u;
    for (int i = 0; i < kVoices; ++i) {
        const Voice& v = voices_[i];
        if (v.sound < 0) {
            if (freeV < 0) freeV = i;
            continue;
        }
        if (v.sound == s) {
            ++count;
            if (v.age < oldestSameAge) {
                oldestSameAge = v.age;
                oldestSame = i;
            }
        }
        if (v.age < oldestAge) {
            oldestAge = v.age;
            oldest = i;
        }
    }
    const int slot = count >= limit ? oldestSame : (freeV >= 0 ? freeV : oldest);
    if (slot < 0) return;
    Voice& v = voices_[slot];
    v.sound = s;
    v.pos = 0;
    v.step = pitch * float(bank_.rate()) / float(rate_);
    const float a = (clampv(pan, -1.0f, 1.0f) + 1.0f) * 0.25f * 3.14159265f;
    const float g = gain * (music ? kMusicGain : kEffectsGain);
    v.gl = g * std::cos(a);
    v.gr = g * std::sin(a);
    v.delay = maxv(0, delayFrames);
    v.age = ++voiceAge_;
    v.music = music;
    v.choke = choke;
    v.fade = 1;
    v.fadeStep = 0;
}

// One sixteenth of the march (GAME_DESIGN 12.1): what each layer plays on it.
void Mixer::playStep(int s, int offset) {
    const MarchStyle& m = marchStyle(world_);
    const int bar = s / 8, pos = s % 8;
    const int root = kChord[bar];
    const float d = duck_;
    // Layer 0: a soft frame drum on the beat, the open fifth every two bars.
    if (layerGain_[0] > 0.01f) {
        const float g = layerGain_[0] * d;
        if (pos == 0) start(kSndFrameDrum, 0.75f * g, -0.1f, 1.0f, offset, true);
        if (pos == 4) start(kSndFrameDrum, 0.55f * g, 0.1f, 1.08f, offset, true);
        if (s % 16 == 0) start(kSndDrone, 0.5f * g, 0.0f, noteRatio(kSndDrone, 0, -1), offset, true);
        if (boss_ && (pos == 0 || pos == 4)) start(kSndTimpani, 0.7f * g, 0.0f, noteRatio(kSndTimpani, 0, -2), offset, true);
    }
    // Layer 1: rim clicks and ghost notes, a low tom on one; a snare roll into the loop's seam.
    if (layerGain_[1] > 0.01f) {
        const float g = layerGain_[1] * d;
        if (pos == 2 || pos == 6) start(kSndRim, 0.45f * g, 0.25f, 1.0f, offset, true);
        if (pos == 7) start(kSndSnare, 0.16f * g, 0.2f, 1.0f, offset, true);
        if (pos == 0) start(kSndTom, 0.42f * g, -0.25f, 1.0f, offset, true);
        if (s >= 60) start(kSndSnare, (0.3f + 0.1f * float(s - 60)) * g, 0.15f, 1.0f, offset, true);
        if (pos % 2 == 1) start(kSndShaker, 0.25f * g, 0.4f, 1.0f, offset, true);
    }
    // Layer 2: the bass, oom-pah: the root on one, the fifth on two, a brass "pah" on the offbeats.
    if (layerGain_[2] > 0.01f) {
        const float g = layerGain_[2] * d;
        if (pos == 0) start(kSndBass, 0.8f * g, 0.0f, noteRatio(kSndBass, root, -2), offset, true);
        if (pos == 4) start(kSndBass, 0.65f * g, 0.0f, noteRatio(kSndBass, root + 4 - 7, -2), offset, true);
        if (pos == 2 || pos == 6) {
            start(kSndBrass, 0.14f * g, -0.3f, noteRatio(kSndBrass, root + 2, -1), offset, true);
            start(kSndBrass, 0.12f * g, 0.3f, noteRatio(kSndBrass, root + 4, -1), offset, true);
        }
    }
    // Layer 3: the melody, doubled by a glockenspiel an octave up.
    if (layerGain_[3] > 0.01f && kTheme[s] > 0) {
        const float g = layerGain_[3] * d;
        const int deg = kTheme[s] - 1;
        switch (m.lead) {
            case 1: start(kSndGlock, 0.55f * g, -0.1f, noteRatio(kSndGlock, deg, -1), offset, true, 1); break;
            case 2: start(kSndBrass, 0.5f * g, -0.1f, noteRatio(kSndBrass, deg, 0), offset, true, 1); break;
            case 3: start(kSndGlock, 0.6f * g, -0.1f, noteRatio(kSndGlock, deg, 0), offset, true, 1); break;
            default: start(kSndFife, 0.6f * g, -0.1f, noteRatio(kSndFife, deg, 0), offset, true, 1); break;
        }
        if (m.lead != 3) start(kSndGlock, 0.22f * g, 0.2f, noteRatio(kSndGlock, deg, 1), offset, true);
    }
    // Layer 4: brass harmony on each bar, a cymbal swelling into the seam: you can take the Keep now.
    if (layerGain_[4] > 0.01f) {
        const float g = layerGain_[4] * d;
        if (pos == 0)
            for (int k = 0; k < 3; ++k) start(kSndBrass, 0.2f * g, float(k - 1) * 0.35f, noteRatio(kSndBrass, root + 2 * k, 0), offset, true);
        if (s == 56) start(kSndCymbalSwell, 0.5f * g, 0.0f, 1.0f, offset, true);
        if (s == 0) start(kSndCrash, 0.35f * g, 0.3f, 1.0f, offset, true);
    }
}

void Mixer::sequence(int frames, float* out) {
    (void)out;
    if (!musicOn_) return;
    const MarchStyle& m = marchStyle(world_);
    // Layer gains follow what is wanted, over about a bar.
    const float rate = float(frames) / float(stepLen_ * 8.0);
    for (int l = 0; l < kLayers; ++l) {
        const float want = l < layersWanted_ ? 1.0f : 0.0f;
        layerGain_[l] += clampv(want - layerGain_[l], -rate, rate);
    }
    double pos = 0;
    while (pos + toNext_ < double(frames)) {
        pos += toNext_;
        playStep(step_, int(pos));
        step_ = (step_ + 1) % kLoop;
        // Swing: the even sixteenths (the "and"s) come late.
        const double swing = double(m.swing) * stepLen_;
        toNext_ = step_ % 2 == 1 ? stepLen_ + swing : stepLen_ - swing;
    }
    toNext_ -= double(frames) - pos;
}

void Mixer::mix(float* out, int frames) {
    u32 t = tail_.load(std::memory_order_relaxed);
    const u32 h = head_.load(std::memory_order_acquire);
    while (t != h) {
        const Command& c = ring_[t % kRing];
        switch (c.type) {
            case 0: {
                const int delay = int(c.delay * float(rate_)) + (c.flags & 1 ? int(toNext_) : 0);
                start(c.sound, c.gain, c.pan, c.pitch, delay, false);
                break;
            }
            case 1:
                duckTarget_ = 0.45f;
                duckHold_ = c.a;
                break;
            case 2:
                effectsOn_ = c.a > 0.5f;
                musicOn_ = c.b > 0.5f;
                break;
            case 3: {
                const int w = int(c.a);
                const bool b = c.b > 0.5f;
                if (w != world_ || b != boss_) {
                    world_ = w;
                    boss_ = b;
                    const MarchStyle& m = marchStyle(world_);
                    stepLen_ = double(rate_) * 60.0 / double(m.bpm) / 4.0;
                    step_ = 0;
                    toNext_ = stepLen_;
                }
                break;
            }
            case 4: layersWanted_ = clampv(int(c.a), 0, kLayers); break;
            case 5: {
                // Up (or down) the world's scale from the current chord's root: 24 ticks climb two octaves.
                const int k = int(c.a);
                const int root = kChord[step_ / 8];
                const int deg = c.b > 0.5f ? root + 7 - (k * 14) / 24 : root + (k * 14) / 24;
                start(kSndWood, c.gain, c.pan, noteRatio(kSndWood, deg, 0), 0, false);
                break;
            }
            default: break;
        }
        ++t;
    }
    tail_.store(t, std::memory_order_release);
    for (int done = 0; done < frames; done += kChunk) mixChunk(out + done * 2, minv(kChunk, frames - done));
    musicLevelOut_.store(musicLevel_, std::memory_order_relaxed);
    mixCalls_.fetch_add(1, std::memory_order_relaxed);
}

void Mixer::mixChunk(float* out, int frames) {
    std::memset(out, 0, sizeof(float) * 2 * size_t(frames));
    const float dt = float(frames) / float(rate_);
    if (duckHold_ > 0) {
        duckHold_ -= dt;
        if (duckHold_ <= 0) duckTarget_ = 1.0f;
    }
    duck_ += (duckTarget_ - duck_) * minv(1.0f, dt / 0.08f);
    sequence(frames, out);
    // The music's level glides to the MUSIC switch in about 0.1 s; once silent its voices are let go.
    const float glide = 1.0f - std::exp(-1.0f / (0.1f * float(rate_)));
    const float target = musicOn_ ? 1.0f : 0.0f;
    for (int i = 0; i < frames; ++i) {
        musicLevel_ += (target - musicLevel_) * glide;
        musicEnv_[i] = musicLevel_;
    }
    const bool musicSilent = !musicOn_ && musicLevel_ < 1e-3f;
    int inUse = 0;
    for (Voice& v : voices_) {
        if (v.sound < 0) continue;
        if (v.music && musicSilent) {
            v.sound = -1;
            continue;
        }
        ++inUse;
        const Sound& s = bank_.get(v.sound);
        int i = 0;
        if (v.delay > 0) {
            const int d = minv(v.delay, frames);
            v.delay -= d;
            i = d;
        }
        for (; i < frames; ++i) {
            const int k = int(v.pos);
            if (k + 1 >= s.len || v.fade <= 0) {
                v.sound = -1;
                break;
            }
            const float f = v.pos - float(k);
            float x = (s.data[k] + (s.data[k + 1] - s.data[k]) * f) * (v.music ? musicEnv_[i] : 1.0f);
            if (v.fadeStep != 0) {
                x *= v.fade;
                v.fade += v.fadeStep;
            }
            out[i * 2] += x * v.gl;
            out[i * 2 + 1] += x * v.gr;
            v.pos += v.step;
        }
    }
    voicesInUse_.store(inUse);
    // A soft limiter holds the output under -1 dBFS true peak: samples held at -1.5 dBFS. Instant attack, ~100 ms release.
    const float thr = 0.841f;
    const float release = 1.0f - std::exp(-1.0f / (0.1f * float(rate_)));
    for (int i = 0; i < frames; ++i) {
        const float peak = std::fmax(std::fabs(out[i * 2]), std::fabs(out[i * 2 + 1]));
        if (peak * limGain_ > thr) limGain_ = thr / peak;
        else limGain_ += (1.0f - limGain_) * release;
        out[i * 2] *= limGain_;
        out[i * 2 + 1] *= limGain_;
    }
}

}  // namespace pm::audio
