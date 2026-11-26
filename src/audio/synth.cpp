#include "audio/synth.h"

#include <cmath>

namespace pm::audio {

float midiHz(float midi) { return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f); }

namespace {

constexpr double kTwoPi = 6.283185307179586;

struct Noise {
    u32 s = 0x1234567u;
    float next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return float(s & 0xFFFFFF) / float(0x7FFFFF) - 1.0f;
    }
};

// A sine by rotation (no drift over a buffer) and an exponential decay by multiplication: cheap enough for the Moto
// (GLOWTIDE's PROGRESS, "Synthesis cost").
struct Osc {
    double c, s, cr, sr;
    Osc(double hz, double rate, double phase = 0) : c(std::cos(phase)), s(std::sin(phase)) {
        const double w = kTwoPi * hz / rate;
        cr = std::cos(w);
        sr = std::sin(w);
    }
    float next() {
        const float out = float(s);
        const double nc = c * cr - s * sr;
        s = s * cr + c * sr;
        c = nc;
        return out;
    }
};
struct Decay {
    double v = 1, r;
    Decay(double seconds, double rate) : r(std::exp(-1.0 / (seconds * rate))) {}
    float next() {
        const float out = float(v);
        v *= r;
        return out;
    }
};
struct OnePole {
    float y = 0, a = 1;
    OnePole() = default;
    OnePole(float hz, float rate) { set(hz, rate); }
    void set(float hz, float rate) { a = 1.0f - std::exp(-float(kTwoPi) * hz / rate); }
    float run(float x) {
        y += (x - y) * a;
        return y;
    }
};
// A two-pole band-pass (state variable), for formants and noise colours.
struct Band {
    float lp = 0, bp = 0, f = 0.1f, q = 0.5f;
    void set(float hz, float rate, float res) {
        f = 2.0f * std::sin(3.14159265f * std::fmin(hz, rate * 0.24f) / rate);
        q = 1.0f / res;
    }
    float run(float x) {
        lp += f * bp;
        const float hp = x - lp - q * bp;
        bp += f * hp;
        return bp;
    }
};

std::vector<float> buffer(float seconds, int rate) { return std::vector<float>(size_t(seconds * float(rate)) + 1, 0.0f); }

void normalize(std::vector<float>& b, float peak) {
    float m = 0;
    for (float v : b) m = std::fmax(m, std::fabs(v));
    if (m <= 0) return;
    const float k = peak / m;
    for (float& v : b) v *= k;
}

// Additive partials (ratio, level, decay seconds) from `start`, with an attack.
struct Partial {
    double ratio;
    float level;
    double decay;
};
void partials(std::vector<float>& b, int rate, double f, const Partial* p, int n, float attack, float start = 0, float gain = 1.0f) {
    const double R = rate;
    const size_t s0 = size_t(start * float(rate));
    for (int k = 0; k < n; ++k) {
        if (f * p[k].ratio > R * 0.45) continue;
        Osc o(f * p[k].ratio, R, double(k) * 0.7);
        Decay e(p[k].decay, R);
        const int att = maxv(1, int(attack * float(rate)));
        for (size_t i = s0; i < b.size(); ++i) {
            const int j = int(i - s0);
            const float a = j < att ? float(j) / float(att) : 1.0f;
            const float ev = e.next();
            if (ev < 1e-5f) break;
            b[i] += p[k].level * o.next() * ev * a * gain;
        }
    }
}

// A band-limited buzzy tone (odd or all harmonics) with an envelope: brass, tuba, reed. Each harmonic is a rotating
// phasor whose step is updated every 64 samples (a glide), so no sin() per sample.
void buzz(std::vector<float>& b, int rate, double f, bool oddOnly, int harmonics, float bright, float attack, float hold,
          float release, float start = 0, float gain = 1.0f, double glideTo = 0) {
    const double R = rate;
    const size_t s0 = size_t(start * float(rate));
    struct H {
        double c, s, cr, sr;
        float lvl;
        int h;
        bool on;
    } hs[24];
    int nh = 0;
    for (int h = 1; h <= minv(harmonics, 24); ++h) {
        if (oddOnly && h % 2 == 0) continue;
        hs[nh++] = {1.0, 0.0, 1.0, 0.0, std::pow(float(h), -bright), h, true};
    }
    const size_t end = minv(b.size(), s0 + size_t(double(attack + hold + release * 7.0f) * R));
    for (size_t blk = s0; blk < end; blk += 64) {
        const float t = float(blk - s0) / float(rate);
        const double ff = glideTo > 0 ? f * std::pow(glideTo / f, std::fmin(1.0, double(t) / double(hold + release))) : f;
        for (int k = 0; k < nh; ++k) {
            const double w = kTwoPi * ff * double(hs[k].h) / R;
            hs[k].on = ff * double(hs[k].h) < R * 0.45;
            hs[k].cr = std::cos(w);
            hs[k].sr = std::sin(w);
        }
        const size_t stop = minv(end, blk + 64);
        for (size_t i = blk; i < stop; ++i) {
            const float tt = float(i - s0) / float(rate);
            const float env = tt < attack ? tt / attack : (tt < attack + hold ? 1.0f : std::exp(-(tt - attack - hold) / release));
            float v = 0;
            for (int k = 0; k < nh; ++k) {
                H& o = hs[k];
                if (o.on) v += o.lvl * float(o.s);
                const double nc = o.c * o.cr - o.s * o.sr;
                o.s = o.s * o.cr + o.c * o.sr;
                o.c = nc;
            }
            b[i] += v * env * gain;
        }
    }
}

}  // namespace

float baseHz(int sound) {
    switch (sound) {
        case kSndWood: return midiHz(74);       // D5
        case kSndGlock: return midiHz(81);      // A5
        case kSndBass: case kSndTimpani: return midiHz(45);  // A2
        case kSndFife: return midiHz(69);       // A4
        case kSndBrass: case kSndDrone: return midiHz(57);   // A3
        default: return 440.0f;
    }
}

void Bank::build(int rate) {
    rate_ = rate;
    const float R = float(rate);
    Noise n;
    auto make = [&](int id, std::vector<float>&& v, float peak) {
        normalize(v, peak);
        store_[id] = static_cast<std::vector<float>&&>(v);
        s_[id] = {store_[id].data(), int(store_[id].size())};
    };
    {  // the wood block: a damped sine, a bright partial, a 2 ms click
        auto b = buffer(0.16f, rate);
        const Partial p[] = {{1.0, 1.0f, 0.055}, {2.71, 0.45f, 0.018}, {4.1, 0.18f, 0.01}};
        partials(b, rate, baseHz(kSndWood), p, 3, 0.001f);
        for (int i = 0; i < int(0.002f * R); ++i) b[size_t(i)] += n.next() * 0.4f * (1.0f - float(i) / (0.002f * R));
        make(kSndWood, static_cast<std::vector<float>&&>(b), 0.8f);
    }
    {  // a paper flip: 40 ms of noise through a band sweeping 2 to 6 kHz
        auto b = buffer(0.06f, rate);
        Band bp;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            bp.set(2000.0f + 4000.0f * (t / 0.05f), R, 1.4f);
            b[i] = bp.run(n.next()) * std::exp(-t / 0.02f) * std::fmin(1.0f, t / 0.003f);
        }
        make(kSndFlip, static_cast<std::vector<float>&&>(b), 0.55f);
    }
    {  // the clash: a drum, a metal ring, a shout
        auto b = buffer(0.5f, rate);
        float ph = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            const float f = 70.0f + 60.0f * std::exp(-t / 0.03f);
            ph += float(kTwoPi) * f / R;
            b[i] = std::sin(ph) * std::exp(-t / 0.12f) * std::fmin(1.0f, t / 0.002f) * 0.9f;
        }
        const Partial ring[] = {{523.0, 0.25f, 0.3}, {1187.0, 0.2f, 0.25}, {1622.0, 0.15f, 0.2}, {2210.0, 0.12f, 0.15}};
        partials(b, rate, 1.0, ring, 4, 0.001f, 0.01f);
        Band f1, f2;
        f1.set(720.0f, R, 4.0f);
        f2.set(1180.0f, R, 4.0f);
        for (size_t i = size_t(0.02f * R); i < size_t(0.14f * R) && i < b.size(); ++i) {
            const float t = float(i) / R - 0.02f;
            const float x = n.next();
            b[i] += (f1.run(x) + 0.7f * f2.run(x)) * std::sin(3.14159f * t / 0.12f) * 0.9f;
        }
        make(kSndClash, static_cast<std::vector<float>&&>(b), 0.9f);
    }
    {  // a glockenspiel note
        auto b = buffer(1.2f, rate);
        const Partial p[] = {{1.0, 1.0f, 0.7}, {2.76, 0.32f, 0.14}, {5.4, 0.12f, 0.05}, {8.9, 0.05f, 0.03}};
        partials(b, rate, baseHz(kSndGlock), p, 4, 0.001f);
        make(kSndGlock, static_cast<std::vector<float>&&>(b), 0.75f);
    }
    {  // coins: three clinks falling
        auto b = buffer(0.5f, rate);
        const Partial p[] = {{1.0, 1.0f, 0.12}, {1.5, 0.6f, 0.09}, {2.37, 0.35f, 0.06}};
        for (int k = 0; k < 3; ++k) partials(b, rate, 2093.0 * std::pow(0.89, k), p, 3, 0.001f, 0.08f * float(k), 0.8f);
        make(kSndCoin, static_cast<std::vector<float>&&>(b), 0.6f);
    }
    {  // the mire: a reed falling a fourth, wet noise under it
        auto b = buffer(0.8f, rate);
        buzz(b, rate, 220.0, false, 12, 1.2f, 0.03f, 0.25f, 0.25f, 0.0f, 1.0f, 165.0);
        OnePole lp(900.0f, R);
        for (size_t i = 0; i < b.size(); ++i) b[i] = lp.run(b[i]) * 1.5f + n.next() * 0.04f * std::exp(-float(i) / R / 0.3f);
        make(kSndReed, static_cast<std::vector<float>&&>(b), 0.6f);
    }
    {  // two low knocks a minor second apart
        auto b = buffer(0.32f, rate);
        const Partial p[] = {{1.0, 1.0f, 0.05}, {2.3, 0.4f, 0.02}};
        partials(b, rate, 330.0, p, 2, 0.001f, 0.0f);
        partials(b, rate, 311.0, p, 2, 0.001f, 0.12f);
        make(kSndKnock, static_cast<std::vector<float>&&>(b), 0.7f);
    }
    {  // crackle: sparse clicks for half a second
        auto b = buffer(0.6f, rate);
        Noise r;
        r.s = 777;
        for (size_t i = 0; i < b.size(); ++i) {
            if (r.next() > 0.996f) {
                const float amp = 0.4f + 0.6f * std::fabs(r.next());
                for (size_t j = 0; j < size_t(0.004f * R) && i + j < b.size(); ++j) b[i + j] += n.next() * amp * (1.0f - float(j) / (0.004f * R));
            }
        }
        for (size_t i = 0; i < b.size(); ++i) b[i] *= std::fmin(1.0f, (0.6f - float(i) / R) / 0.2f);
        make(kSndCrackle, static_cast<std::vector<float>&&>(b), 0.55f);
    }
    {  // a horn: two low notes
        auto b = buffer(1.0f, rate);
        buzz(b, rate, midiHz(57), false, 14, 1.1f, 0.05f, 0.2f, 0.12f, 0.0f, 1.0f);
        buzz(b, rate, midiHz(52), false, 14, 1.1f, 0.05f, 0.3f, 0.2f, 0.35f, 1.0f);
        OnePole lp(1400.0f, R);
        for (float& v : b) v = lp.run(v);
        make(kSndHorn, static_cast<std::vector<float>&&>(b), 0.7f);
    }
    {  // a deep drum
        auto b = buffer(0.6f, rate);
        float ph = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            ph += float(kTwoPi) * (55.0f + 40.0f * std::exp(-t / 0.04f)) / R;
            b[i] = (std::sin(ph) + 0.25f * std::sin(ph * 3.0f)) * std::exp(-t / 0.22f) + n.next() * 0.1f * std::exp(-t / 0.01f);
        }
        make(kSndDeepDrum, static_cast<std::vector<float>&&>(b), 0.8f);
    }
    {  // the fanfare: a brass arpeggio (1, 3, 5, 8 of D major) into a held chord, a crash on top
        auto b = buffer(2.4f, rate);
        const int notes[4] = {62, 66, 69, 74};
        for (int k = 0; k < 4; ++k) buzz(b, rate, midiHz(float(notes[k])), false, 14, 1.15f, 0.02f, 0.08f, 0.1f, 0.12f * float(k), 0.6f);
        const int chord[4] = {62, 66, 69, 74};
        for (int k = 0; k < 4; ++k) buzz(b, rate, midiHz(float(chord[k])), false, 14, 1.2f, 0.04f, 0.9f, 0.5f, 0.5f, 0.55f);
        OnePole lp(2600.0f, R);
        for (float& v : b) v = lp.run(v);
        OnePole hp(3000.0f, R);
        for (size_t i = size_t(0.5f * R); i < b.size(); ++i) {
            const float t = float(i) / R - 0.5f;
            const float x = n.next();
            b[i] += (x - hp.run(x)) * 0.18f * std::exp(-t / 0.7f);
        }
        make(kSndFanfare, static_cast<std::vector<float>&&>(b), 0.85f);
    }
    {  // a choir "ah": a buzzy chord through two formants
        auto b = buffer(1.5f, rate);
        const int chord[3] = {62, 66, 69};
        for (int k = 0; k < 3; ++k) buzz(b, rate, midiHz(float(chord[k])), false, 20, 0.9f, 0.12f, 0.7f, 0.35f, 0.0f, 0.5f);
        Band f1, f2;
        f1.set(800.0f, R, 3.0f);
        f2.set(1150.0f, R, 3.0f);
        for (float& v : b) v = f1.run(v) + 0.6f * f2.run(v);
        make(kSndChoir, static_cast<std::vector<float>&&>(b), 0.55f);
    }
    {  // the lose horn: a fifth falling, then paper tearing
        auto b = buffer(1.8f, rate);
        buzz(b, rate, midiHz(57), false, 12, 1.2f, 0.06f, 0.3f, 0.3f, 0.0f, 0.9f, midiHz(50));
        OnePole lp(1100.0f, R);
        for (float& v : b) v = lp.run(v);
        OnePole tear;
        for (size_t i = size_t(0.9f * R); i < b.size(); ++i) {
            const float t = float(i) / R - 0.9f;
            tear.set(5000.0f * std::exp(-t / 0.3f) + 300.0f, R);
            b[i] += tear.run(n.next() * (0.6f + 0.4f * std::sin(t * 90.0f))) * 0.6f * std::exp(-t / 0.35f) * std::fmin(1.0f, t / 0.02f);
        }
        make(kSndLoseHorn, static_cast<std::vector<float>&&>(b), 0.7f);
    }
    {  // the page turn's swish: noise through a filter that rises then falls
        auto b = buffer(0.6f, rate);
        Band bp;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            const float u = t / 0.55f;
            bp.set(600.0f + 3800.0f * std::sin(3.14159f * std::fmin(1.0f, u)), R, 0.9f);
            b[i] = bp.run(n.next()) * std::sin(3.14159f * std::fmin(1.0f, u));
        }
        make(kSndSwish, static_cast<std::vector<float>&&>(b), 0.5f);
    }
    {  // a ratchet click
        auto b = buffer(0.05f, rate);
        Band bp;
        bp.set(2500.0f, R, 6.0f);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            b[i] = bp.run(t < 0.002f ? n.next() : 0.0f) * 6.0f * std::exp(-t / 0.012f);
        }
        make(kSndRatchet, static_cast<std::vector<float>&&>(b), 0.4f);
    }
    {  // a UI tap: a soft wood tick, high
        auto b = buffer(0.09f, rate);
        const Partial p[] = {{1.0, 1.0f, 0.03}, {2.7, 0.3f, 0.01}};
        partials(b, rate, 1175.0, p, 2, 0.001f);
        make(kSndTap, static_cast<std::vector<float>&&>(b), 0.45f);
    }
    {  // a footstep: a soft low thud
        auto b = buffer(0.07f, rate);
        OnePole lp(500.0f, R);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            b[i] = lp.run(n.next()) * std::exp(-t / 0.018f) + 0.4f * std::sin(float(kTwoPi) * 120.0f * t) * std::exp(-t / 0.02f);
        }
        make(kSndStep, static_cast<std::vector<float>&&>(b), 0.35f);
    }
    // ---- The march's instruments ----
    {  // the frame drum
        auto b = buffer(0.35f, rate);
        float ph = 0;
        OnePole lp(1800.0f, R);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            ph += float(kTwoPi) * (92.0f + 50.0f * std::exp(-t / 0.02f)) / R;
            b[i] = std::sin(ph) * std::exp(-t / 0.16f) + lp.run(n.next()) * 0.35f * std::exp(-t / 0.03f);
        }
        make(kSndFrameDrum, static_cast<std::vector<float>&&>(b), 0.8f);
    }
    {  // the snare
        auto b = buffer(0.22f, rate);
        Band bp;
        bp.set(3200.0f, R, 0.7f);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            b[i] = bp.run(n.next()) * std::exp(-t / 0.07f) + 0.5f * std::sin(float(kTwoPi) * 185.0f * t) * std::exp(-t / 0.03f);
        }
        make(kSndSnare, static_cast<std::vector<float>&&>(b), 0.7f);
    }
    {  // a rim click
        auto b = buffer(0.05f, rate);
        const Partial p[] = {{1.0, 1.0f, 0.012}, {2.3, 0.6f, 0.008}};
        partials(b, rate, 1250.0, p, 2, 0.0005f);
        for (int i = 0; i < int(0.003f * R); ++i) b[size_t(i)] += n.next() * 0.6f;
        make(kSndRim, static_cast<std::vector<float>&&>(b), 0.55f);
    }
    {  // a low tom
        auto b = buffer(0.4f, rate);
        float ph = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            ph += float(kTwoPi) * (140.0f + 45.0f * std::exp(-t / 0.05f)) / R;
            b[i] = (std::sin(ph) + 0.3f * std::sin(ph * 2.0f)) * std::exp(-t / 0.2f);
        }
        make(kSndTom, static_cast<std::vector<float>&&>(b), 0.7f);
    }
    {  // a cymbal swell: noise, high-passed, swelling over a bar
        auto b = buffer(1.3f, rate);
        OnePole lp(6000.0f, R);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            const float x = n.next();
            const float hi = x - lp.run(x);
            const float env = t < 0.9f ? std::pow(t / 0.9f, 2.0f) : std::exp(-(t - 0.9f) / 0.15f);
            b[i] = hi * env;
        }
        make(kSndCymbalSwell, static_cast<std::vector<float>&&>(b), 0.45f);
    }
    {  // a crash
        auto b = buffer(1.4f, rate);
        OnePole lp(5000.0f, R);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            const float x = n.next();
            b[i] = (x - lp.run(x)) * std::exp(-t / 0.5f) * std::fmin(1.0f, t / 0.002f);
        }
        make(kSndCrash, static_cast<std::vector<float>&&>(b), 0.5f);
    }
    {  // the bass: a tuba-like buzz, odd harmonics, enough of them for a phone's speaker
        auto b = buffer(0.6f, rate);
        buzz(b, rate, baseHz(kSndBass), true, 15, 0.85f, 0.015f, 0.12f, 0.18f);
        OnePole lp(1600.0f, R);
        for (float& v : b) v = lp.run(v);
        make(kSndBass, static_cast<std::vector<float>&&>(b), 0.8f);
    }
    {  // the fife: a sine with its octave and breath, a little vibrato
        auto b = buffer(0.9f, rate);
        Band br;
        br.set(baseHz(kSndFife) * 3.0f, R, 2.0f);
        double ph = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            const float vib = 1.0f + 0.006f * std::sin(float(kTwoPi) * 5.5f * t) * std::fmin(1.0f, t / 0.2f);
            ph += kTwoPi * double(baseHz(kSndFife) * vib) / double(R);
            const float env = std::fmin(1.0f, t / 0.025f) * std::exp(-t / 0.55f);
            b[i] = (float(std::sin(ph)) + 0.32f * float(std::sin(2.0 * ph)) + 0.12f * float(std::sin(3.0 * ph)) +
                    br.run(n.next()) * 0.25f) * env;
        }
        make(kSndFife, static_cast<std::vector<float>&&>(b), 0.75f);
    }
    {  // brass harmony
        auto b = buffer(1.0f, rate);
        buzz(b, rate, baseHz(kSndBrass), false, 16, 1.1f, 0.04f, 0.35f, 0.25f);
        OnePole lp(2200.0f, R);
        for (float& v : b) v = lp.run(v);
        make(kSndBrass, static_cast<std::vector<float>&&>(b), 0.7f);
    }
    {  // the drone: an open fifth, soft, two bars long
        auto b = buffer(2.6f, rate);
        const Partial p[] = {{1.0, 1.0f, 9.0}, {2.0, 0.35f, 9.0}, {1.5, 0.8f, 9.0}, {3.0, 0.25f, 9.0}};
        partials(b, rate, baseHz(kSndDrone), p, 4, 0.35f);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            b[i] *= std::fmin(1.0f, (2.6f - t) / 0.5f);
        }
        make(kSndDrone, static_cast<std::vector<float>&&>(b), 0.6f);
    }
    {  // timpani
        auto b = buffer(1.0f, rate);
        const Partial p[] = {{1.0, 1.0f, 0.5}, {1.5, 0.5f, 0.3}, {1.98, 0.3f, 0.2}, {2.44, 0.15f, 0.15}};
        partials(b, rate, baseHz(kSndTimpani), p, 4, 0.002f);
        for (int i = 0; i < int(0.01f * R); ++i) b[size_t(i)] += n.next() * 0.3f;
        make(kSndTimpani, static_cast<std::vector<float>&&>(b), 0.8f);
    }
    {  // a shaker
        auto b = buffer(0.07f, rate);
        OnePole lp(7000.0f, R);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            const float x = n.next();
            b[i] = (x - lp.run(x)) * std::sin(3.14159f * std::fmin(1.0f, t / 0.06f));
        }
        make(kSndShaker, static_cast<std::vector<float>&&>(b), 0.35f);
    }
}

const MarchStyle& marchStyle(int world) {
    static const MarchStyle styles[10] = {
        {{0, 2, 4, 5, 7, 9, 11}, 74, 120.0f, 0.0f, 0},   // Greenmeadow: D major, fife
        {{0, 1, 4, 5, 7, 8, 10}, 74, 108.0f, 0.0f, 1},   // Sunbaked: D Phrygian dominant, plucked
        {{0, 2, 4, 5, 7, 9, 11}, 77, 112.0f, 0.0f, 3},   // Frostvale: F major, bell
        {{0, 2, 3, 5, 7, 9, 10}, 69, 116.0f, 0.0f, 2},   // Emberfall: A Dorian, reed
        {{0, 2, 4, 5, 7, 9, 10}, 79, 124.0f, 0.0f, 3},   // Tidereach: G Mixolydian, bell
        {{0, 2, 4, 5, 7, 9, 11}, 77, 104.0f, 0.12f, 1},  // Bloomgrove: F major, plucked
        {{0, 2, 3, 5, 7, 8, 10}, 76, 100.0f, 0.0f, 2},   // Mirefen: E minor, reed
        {{0, 2, 4, 6, 7, 9, 11}, 72, 126.0f, 0.0f, 0},   // Skyhold: C Lydian, fife
        {{0, 2, 3, 5, 7, 8, 11}, 74, 132.0f, 0.0f, 2},   // Cinderpeak: D harmonic minor, reed
        {{0, 2, 4, 5, 7, 9, 11}, 70, 112.0f, 0.3f, 3},   // Lanternfair: B flat major, swung, bell
    };
    return styles[((world < 1 ? 1 : world) - 1) % 10];
}

}  // namespace pm::audio
