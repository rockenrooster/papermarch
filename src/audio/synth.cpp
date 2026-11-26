#include "audio/synth.h"

#include <cmath>

namespace gt::audio {

const int kLadderMidi[11] = {60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84};  // C major pentatonic, C4..C6

float midiHz(float midi) { return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f); }

namespace {

constexpr float kTwoPi = 6.283185307f;

struct Noise {
    u32 s = 0x1234567u;
    float next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return float(s & 0xFFFFFF) / float(0x7FFFFF) - 1.0f;
    }
};

// One-pole low-pass with cutoff in Hz.
struct OnePole {
    float y = 0;
    float run(float x, float hz, float rate) {
        const float a = 1.0f - std::exp(-kTwoPi * hz / rate);
        y += (x - y) * a;
        return y;
    }
    // A fixed cutoff: the coefficient computed once.
    float runA(float x, float a) {
        y += (x - y) * a;
        return y;
    }
};

// A sine by rotation (double precision: no drift over a whole buffer) and an exponential decay by multiplication.
struct Osc {
    double c, s, cr, sr;
    Osc(double hz, double rate, double phase = 0) : c(std::cos(phase)), s(std::sin(phase)) {
        const double w = 6.283185307179586 * hz / rate;
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

std::vector<float> buffer(float seconds, int rate) { return std::vector<float>(size_t(seconds * float(rate)) + 1, 0.0f); }

void normalize(std::vector<float>& b, float peak) {
    float m = 0;
    for (float v : b) m = std::fmax(m, std::fabs(v));
    if (m <= 0) return;
    const float k = peak / m;
    for (float& v : b) v *= k;
}

// A plucked tone in one of three timbres; amplitude 1 peak before normalization.
void pluck(std::vector<float>& b, int rate, float f, int timbre, float decay, float start = 0, float gain = 1.0f) {
    Noise n;
    const double R = double(rate);
    const int s0 = int(start * float(rate));
    // Partials (ratio, level, decay seconds) per timbre.
    struct Partial {
        double ratio;
        float level;
        double decay;
    };
    Partial parts[4];
    int np = 0;
    if (timbre == 1) {         // glass: inharmonic, bright, long
        parts[np++] = {1.0, 1.0f, double(decay) * 1.8};
        parts[np++] = {2.76, 0.35f, 0.15};
        parts[np++] = {5.4, 0.15f, 0.07};
    } else if (timbre == 2) {  // bell
        parts[np++] = {1.0, 1.0f, double(decay) * 2.6};
        parts[np++] = {2.0, 0.45f, 0.3};
        parts[np++] = {3.01, 0.25f, 0.2};
        parts[np++] = {4.2, 0.12f, 0.12};
    } else {                   // kalimba: fundamental, a quiet fourth harmonic, a 2 ms click
        parts[np++] = {1.0, 1.0f, double(decay)};
        parts[np++] = {4.0, 0.25f, 0.06};
    }
    Osc osc[4] = {Osc(f * parts[0].ratio, R), Osc(f * parts[1].ratio, R), Osc(np > 2 ? f * parts[2].ratio : 1.0, R),
                  Osc(np > 3 ? f * parts[3].ratio : 1.0, R)};
    Decay env[4] = {Decay(parts[0].decay, R), Decay(parts[1].decay, R), Decay(np > 2 ? parts[2].decay : 1.0, R),
                    Decay(np > 3 ? parts[3].decay : 1.0, R)};
    const int attack = int(0.002 * R);
    for (size_t i = size_t(s0); i < b.size(); ++i) {
        const int j = int(i - size_t(s0));
        float v = 0;
        for (int k = 0; k < np; ++k) v += parts[k].level * osc[k].next() * env[k].next();
        const float att = j < attack ? float(j) / float(attack) : 1.0f;
        if (timbre == 0 && j < attack) v += n.next() * 0.3f * (1.0f - float(j) / float(attack));
        b[i] += v * att * gain;
    }
}

}  // namespace

void Bank::build(int rate, int timbre) {
    rate_ = rate;
    const float R = float(rate);
    Noise n;
    auto make = [&](int id, std::vector<float>&& v, float peak) {
        normalize(v, peak);
        store_[id] = std::move(v);
        s_[id] = {store_[id].data(), int(store_[id].size())};
    };
    {  // pip: sine 880 Hz, 2 ms attack, 35 ms decay
        auto b = buffer(0.15f, rate);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            b[i] = std::sin(kTwoPi * 880.0f * t) * std::fmin(1.0f, t / 0.002f) * std::exp(-t / 0.035f);
        }
        make(kSndPip, std::move(b), 0.5f);
    }
    for (int k = 0; k < 11; ++k) {  // the ladder and the music's plucks
        auto b = buffer(timbre == 0 ? 0.9f : 1.6f, rate);
        pluck(b, rate, midiHz(float(kLadderMidi[k])), timbre, 0.18f);
        make(kSndLadder0 + k, std::move(b), 0.9f);
        auto p = buffer(0.9f, rate);
        pluck(p, rate, midiHz(float(kLadderMidi[k])), 0, 0.25f);
        make(kSndPluck0 + k, std::move(p), 0.6f);
    }
    {  // tick: band-passed noise at ~3 kHz, 12 ms
        auto b = buffer(0.05f, rate);
        OnePole lp, hp;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            const float x = n.next();
            const float low = lp.run(x, 4200.0f, R);
            const float band = low - hp.run(low, 2000.0f, R);
            b[i] = band * std::exp(-t / 0.012f);
        }
        make(kSndTick, std::move(b), 0.6f);
    }
    {  // pff: low-passed noise, 90 ms, falling cut-off
        auto b = buffer(0.2f, rate);
        OnePole lp;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            b[i] = lp.run(n.next(), 2400.0f * std::exp(-t / 0.05f) + 200.0f, R) * std::exp(-t / 0.09f) * std::fmin(1.0f, t / 0.005f);
        }
        make(kSndPff, std::move(b), 0.6f);
    }
    auto burst = [&](float seconds, float decay, float cutoff, float pings, float pingBase) {
        auto b = buffer(seconds, rate);
        OnePole lp;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            b[i] = lp.run(n.next(), cutoff * std::exp(-t / (decay * 0.6f)) + 300.0f, R) * std::exp(-t / decay);
        }
        for (int k = 0; k < int(pings); ++k)
            pluck(b, rate, pingBase * std::pow(1.26f, float(k)), 1, 0.12f, 0.02f + 0.045f * float(k), 0.35f);
        return b;
    };
    make(kSndChip, burst(0.08f, 0.03f, 6000.0f, 0, 0), 0.55f);
    make(kSndShatterSmall, burst(0.4f, 0.16f, 7000.0f, 2, 1568.0f), 0.8f);
    {
        auto b = burst(1.2f, 0.6f, 8000.0f, 4, 1318.5f);
        for (size_t i = 0; i < b.size(); ++i) {  // and a falling sine
            const float t = float(i) / R;
            const float f = 300.0f * std::exp(-t / 0.25f) + 50.0f;
            b[i] += 0.8f * std::sin(kTwoPi * f * t) * std::exp(-t / 0.35f);
        }
        make(kSndShatterBig, std::move(b), 0.95f);
    }
    {  // thump: a sine falling from 120 to 60 Hz in 80 ms
        auto b = buffer(0.25f, rate);
        float ph = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            const float f = 60.0f + 60.0f * std::exp(-t / 0.04f);
            ph += kTwoPi * f / R;
            b[i] = std::sin(ph) * std::exp(-t / 0.08f) * std::fmin(1.0f, t / 0.003f);
        }
        make(kSndThump, std::move(b), 0.85f);
    }
    {  // crack: a sharp burst plus a falling minor third (G5 -> E5)
        auto b = burst(0.5f, 0.05f, 9000.0f, 0, 0);
        pluck(b, rate, midiHz(79), 1, 0.08f, 0.0f, 0.5f);
        pluck(b, rate, midiHz(76), 1, 0.12f, 0.09f, 0.5f);
        make(kSndCrack, std::move(b), 0.85f);
    }
    {  // pop_low: 300 -> 90 Hz in 120 ms plus a click
        auto b = buffer(0.3f, rate);
        float ph = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            const float f = 90.0f + 210.0f * std::exp(-t / 0.05f);
            ph += kTwoPi * f / R;
            b[i] = std::sin(ph) * std::exp(-t / 0.12f) + (t < 0.003f ? n.next() * 0.5f : 0.0f);
        }
        make(kSndPopLow, std::move(b), 0.85f);
    }
    {  // core hit: two low sines a semitone apart, 250 ms
        auto b = buffer(0.4f, rate);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            const float e = std::exp(-t / 0.2f) * std::fmin(1.0f, t / 0.005f);
            b[i] = (std::sin(kTwoPi * midiHz(40) * t) + std::sin(kTwoPi * midiHz(41) * t)) * e +
                   0.3f * std::sin(kTwoPi * midiHz(64) * t) * e;
        }
        make(kSndCoreHit, std::move(b), 0.85f);
    }
    {  // the win chord: C major ninth in plucks, rolled upward over 400 ms
        auto b = buffer(2.4f, rate);
        const int notes[5] = {48, 64, 67, 71, 74};
        for (int k = 0; k < 5; ++k) pluck(b, rate, midiHz(float(notes[k])), timbre, 0.6f, 0.1f * float(k), 0.8f);
        make(kSndWinChord, std::move(b), 0.9f);
    }
    const int stars[3] = {88, 91, 96};  // E6, G6, C7
    for (int k = 0; k < 3; ++k) {
        auto b = buffer(0.9f, rate);
        pluck(b, rate, midiHz(float(stars[k])), 2, 0.2f);
        make(kSndStar0 + k, std::move(b), 0.7f);
    }
    {  // lose: two soft notes falling a minor third (G4 -> E4), slow decay
        auto b = buffer(2.0f, rate);
        for (int k = 0; k < 2; ++k) {
            const float f = midiHz(k == 0 ? 67.0f : 64.0f), t0 = 0.35f * float(k);
            for (size_t i = size_t(t0 * R); i < b.size(); ++i) {
                const float t = float(i) / R - t0;
                const float vib = 1.0f + 0.004f * std::sin(kTwoPi * 5.0f * t);
                b[i] += (std::sin(kTwoPi * f * vib * t) + 0.2f * std::sin(kTwoPi * f * 2.0f * t)) * std::fmin(1.0f, t / 0.04f) *
                        std::exp(-t / 0.7f);
            }
        }
        make(kSndLose, std::move(b), 0.7f);
    }
    {  // UI tap: a short C5, quiet
        auto b = buffer(0.25f, rate);
        pluck(b, rate, midiHz(72), 0, 0.06f);
        make(kSndUiTap, std::move(b), 0.4f);
    }
    {  // tink: a short glass pluck, C5
        auto b = buffer(0.25f, rate);
        pluck(b, rate, midiHz(72), 1, 0.07f);
        make(kSndTink, std::move(b), 0.45f);
    }
    {  // geode_open: a bright crack, then E6 G6 C7 in glass, 40 ms apart
        auto b = burst(0.6f, 0.05f, 9500.0f, 0, 0);
        const int notes[3] = {88, 91, 96};
        for (int k = 0; k < 3; ++k) pluck(b, rate, midiHz(float(notes[k])), 1, 0.16f, 0.03f + 0.04f * float(k), 0.6f);
        make(kSndGeodeOpen, std::move(b), 0.85f);
    }
    {  // the music's low pulse: C2, 200 ms
        auto b = buffer(0.4f, rate);
        for (size_t i = 0; i < b.size(); ++i) {
            const float t = float(i) / R;
            b[i] = (std::sin(kTwoPi * midiHz(36) * t) + 0.3f * std::sin(kTwoPi * midiHz(48) * t)) * std::fmin(1.0f, t / 0.01f) *
                   std::exp(-t / 0.18f);
        }
        make(kSndPulse, std::move(b), 0.8f);
    }
}

const MusicStyle& worldMusic(int world) {
    // The chords as MIDI notes; every one is C, D, E, G or A. World 1 keeps the first loop's C and A, with A for its B.
    static const MusicStyle m[10] = {
        {{{60, 64, 67, 69, 74}, {57, 60, 64, 67, 74}}, 76},  // Violet Dusk: C6/9, Am7 add 11
        {{{57, 60, 64, 67, 74}, {55, 60, 62, 64, 69}}, 72},  // Deep Teal: Am7 add 11, G6 sus 4
        {{{60, 64, 67, 69, 74}, {62, 64, 67, 69, 72}}, 80},  // Rose Dawn: C6/9, D9 sus 4
        {{{64, 67, 69, 72, 74}, {60, 64, 67, 69, 74}}, 74},  // Emerald Aurora: Am7/E, C6/9
        {{{55, 57, 62, 64, 72}, {57, 60, 64, 67, 74}}, 84},  // Ember: G6/9 add 11, Am7 add 11
        {{{62, 64, 69, 72, 76}, {57, 60, 64, 69, 72}}, 70},  // Frost Glass: D9 sus 4, Am
        {{{57, 60, 64, 67, 74}, {62, 67, 69, 72, 74}}, 78},  // Storm Indigo: Am7 add 11, D7 sus 4
        {{{60, 64, 67, 69, 74}, {55, 60, 62, 64, 69}}, 82},  // Gold Noon: C6/9, G6 sus 4
        {{{64, 67, 69, 74, 76}, {57, 60, 64, 67, 72}}, 70},  // Moonlit Jade: Em7 sus 4, Am7
        {{{62, 64, 67, 69, 72}, {57, 60, 64, 67, 74}}, 80},  // Crimson Dusk: D9 sus 4, Am7 add 11
    };
    return m[(world < 1 ? 0 : (world - 1) % 10)];
}

void buildPad(int rate, const MusicStyle& m, std::vector<float>& darkOut, std::vector<float>& brightOut) {
    const float R = float(rate);
    // Four bars; the chords crossfade on their bar lines; soft detuned partials through a low-pass.
    const float beat = 60.0f / m.bpm, loop = beat * 16.0f;
    const int len = int(loop * R);
    darkOut.assign(size_t(len), 0.0f);
    brightOut.assign(size_t(len), 0.0f);
    const auto& chords = m.chords;
    for (int lit = 0; lit < 2; ++lit) {
        std::vector<float>& out = lit ? brightOut : darkOut;
        const bool bright = lit != 0;
        OnePole lp;
        const float a = 1.0f - std::exp(-kTwoPi * (bright ? 3000.0f : 900.0f) / R);
        const float half = loop * 0.5f;
        const int halfLen = len / 2;
        const float h2 = bright ? 0.35f : 0.12f, h3 = bright ? 0.18f : 0.04f;
        for (int c = 0; c < 2; ++c) {
            // Each chord's partials as rotating phasors, started where the time-based formula would be.
            const double t0 = c == 0 ? 0.0 : double(halfLen) / double(R);
            std::vector<Osc> osc;
            osc.reserve(30);
            for (int k = 0; k < 5; ++k) {
                const double f = double(midiHz(float(chords[c][k])));
                for (int d = 0; d < 2; ++d) {
                    const double ff = f * (d ? 1.003 : 0.997);
                    osc.emplace_back(ff, double(R), 6.283185307179586 * ff * t0 + double(k));
                    osc.emplace_back(ff * 2.0, double(R), 6.283185307179586 * ff * 2.0 * t0);
                    osc.emplace_back(ff * 3.0, double(R), 6.283185307179586 * ff * 3.0 * t0);
                }
            }
            Osc swell(1.0 / double(beat) * 0.25, double(R), 6.283185307179586 * t0 / double(beat) * 0.25);
            const int i0 = c == 0 ? 0 : halfLen, i1 = c == 0 ? halfLen : len;
            for (int i = i0; i < i1; ++i) {
                const float t = float(i) / R;
                const float local = c == 0 ? t : t - half;
                // Crossfade 0.6 s around each chord change (and the loop's seam).
                const float fadeIn = std::fmin(1.0f, local / 0.6f), fadeOut = std::fmax(0.0f, std::fmin(1.0f, (half - local) / 0.6f));
                float v = 0;
                for (size_t o = 0; o < osc.size(); o += 3) v += osc[o].next() + h2 * osc[o + 1].next() + h3 * osc[o + 2].next();
                v *= 0.5f + 0.5f * swell.next() * 0.2f;
                out[size_t(i)] = lp.runA(v, a) * fadeIn * fadeOut;
            }
        }
        normalize(out, 0.5f);
    }
}

}  // namespace gt::audio
