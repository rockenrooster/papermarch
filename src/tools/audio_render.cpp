#include "tools/audio_render.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "audio/mixer.h"
#include "core/types.h"

namespace pm {

using namespace audio;

bool writeWav(const char* path, const float* stereo, int frames, int rate) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const u32 dataBytes = u32(frames) * 4;
    auto u32le = [&](u32 v) { std::fwrite(&v, 4, 1, f); };
    auto u16le = [&](u16 v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f);
    u32le(36 + dataBytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    u32le(16);
    u16le(1);
    u16le(2);
    u32le(u32(rate));
    u32le(u32(rate) * 4);
    u16le(4);
    u16le(16);
    std::fwrite("data", 1, 4, f);
    u32le(dataBytes);
    for (int i = 0; i < frames * 2; ++i) {
        const float v = stereo[i] < -1.0f ? -1.0f : (stereo[i] > 1.0f ? 1.0f : stereo[i]);
        const i16 s = i16(std::lround(v * 32767.0f));
        std::fwrite(&s, 2, 1, f);
    }
    std::fclose(f);
    return true;
}

namespace {
struct Timed {
    float at;
    int sound;
    float gain, pan, pitch;
    int ladder;  // >= 0: a ladder tick (step), sound ignored
};
}  // namespace

int renderAudio(const char* name, const char* outPath, int world, int layers, bool boss, float seconds) {
    static Mixer mixer;  // big: not on the stack
    const int rate = 48000;
    mixer.init(rate);
    mixer.setEnabled(true, true);
    mixer.setWorld(world, boss);
    std::vector<Timed> ev;
    bool music = false;
    if (!std::strcmp(name, "music")) {
        music = true;
        mixer.setLayers(layers);
        if (seconds <= 0) seconds = 16.5f;
    } else if (!std::strcmp(name, "ladder")) {
        for (int k = 1; k <= 24; ++k) ev.push_back({0.1f + 0.5f * float(k) / 24.0f, 0, 0.55f, 0, 1, k});
        if (seconds <= 0) seconds = 1.2f;
        mixer.setLayers(0);
    } else if (!std::strcmp(name, "battle")) {
        // The shooter at its busiest, as src/game/feedback.cpp plays it: a volley 13 times a second, hits, kills, a horde's
        // horn, a gate, a barrel, the boss falling.
        music = true;
        mixer.setLayers(4);
        for (float t = 0.2f; t < 8.0f; t += 1.0f / 13.0f) ev.push_back({t, kSndShot, 0.5f, 0.0f, 1.0f, -1});
        for (float t = 0.25f; t < 8.0f; t += 1.0f / 20.0f) ev.push_back({t, kSndThwack, 0.35f, 0.2f, 1.1f, -1});
        for (float t = 1.0f; t < 7.0f; t += 1.0f / 25.0f) ev.push_back({t, kSndFlip, 0.4f, 0.1f, 1.0f, -1});
        ev.push_back({0.5f, kSndHorn, 0.7f, 0, 1, -1});
        ev.push_back({0.7f, kSndDeepDrum, 0.6f, 0, 1, -1});
        for (int k = 0; k <= 4; ++k) ev.push_back({2.0f, 0, 0.6f, 0.3f, 1, 3 + 2 * k});
        ev.push_back({2.0f, kSndPop, 0.7f, 0.3f, 1.15f, -1});
        ev.push_back({2.05f, kSndCoin, 0.45f, 0.3f, 1, -1});
        ev.push_back({4.0f, kSndBoom, 0.75f, -0.3f, 1.1f, -1});
        ev.push_back({4.08f, kSndCoin, 0.6f, -0.3f, 1, -1});
        for (int k = 0; k < 3; ++k) ev.push_back({4.12f + 0.07f * float(k), kSndPop, 0.45f, -0.3f, 1.0f + 0.12f * float(k), -1});
        ev.push_back({6.0f, kSndBoom, 1.0f, 0, 0.75f, -1});
        ev.push_back({6.05f, kSndCrash, 0.6f, 0, 1, -1});
        ev.push_back({6.18f, kSndBoom, 0.7f, 0.3f, 1.0f, -1});
        if (seconds <= 0) seconds = 8.5f;
    } else if (!std::strcmp(name, "ui")) {
        ev.push_back({0.1f, kSndTap, 0.7f, 0, 1, -1});
        ev.push_back({0.4f, kSndKnock, 0.8f, 0, 1, -1});
        ev.push_back({0.8f, kSndRatchet, 0.6f, 0, 1, -1});
        if (seconds <= 0) seconds = 1.3f;
        mixer.setLayers(0);
    } else if (!std::strcmp(name, "win")) {
        music = true;
        mixer.setLayers(5);
        ev.push_back({0.2f, kSndClash, 1.0f, 0, 0.8f, -1});
        ev.push_back({0.25f, kSndCrash, 0.5f, 0, 1.0f, -1});
        ev.push_back({0.45f, kSndFanfare, 0.95f, 0, 1, -1});
        ev.push_back({0.9f, kSndChoir, 0.45f, 0, 1, -1});
        for (int k = 0; k < 3; ++k) ev.push_back({1.8f + 0.28f * float(k), kSndGlock, 0.8f, float(k - 1) * 0.4f, 2.0f * (k == 0 ? 1.0f : k == 1 ? 1.26f : 1.5f), -1});
        if (seconds <= 0) seconds = 3.5f;
    } else if (!std::strcmp(name, "lose")) {
        mixer.setLayers(1);
        ev.push_back({0.2f, kSndLoseHorn, 0.85f, 0, 1, -1});
        if (seconds <= 0) seconds = 2.4f;
    } else if (!std::strcmp(name, "turn")) {
        mixer.setLayers(0);
        ev.push_back({0.1f, kSndSwish, 0.8f, 0.2f, 1, -1});
        for (int k = 0; k < 10; ++k) ev.push_back({0.45f + 0.04f * float(k), kSndRatchet, 0.35f, float(k % 3 - 1) * 0.4f, 1.0f, -1});
        if (seconds <= 0) seconds = 1.2f;
    } else if (!std::strcmp(name, "effects")) {
        mixer.setLayers(0);
        const int list[] = {kSndWood, kSndFlip, kSndClash, kSndGlock, kSndCoin, kSndReed, kSndKnock, kSndCrackle, kSndHorn,
                            kSndDeepDrum, kSndFanfare, kSndChoir, kSndLoseHorn, kSndSwish, kSndRatchet, kSndTap, kSndStep,
                            kSndShot,     kSndBoom,    kSndPop,   kSndThwack};
        float t = 0.1f;
        for (int s : list) {
            ev.push_back({t, s, 0.8f, 0, 1, -1});
            t += 1.6f;
        }
        if (seconds <= 0) seconds = t + 1.0f;
    } else {
        std::fprintf(stderr, "render-audio: unknown name %s\n", name);
        return 2;
    }
    if (!music && std::strcmp(name, "win") && std::strcmp(name, "lose")) mixer.setLayers(0);
    std::sort(ev.begin(), ev.end(), [](const Timed& a, const Timed& b) { return a.at < b.at; });
    const int frames = int(seconds * float(rate));
    std::vector<float> out(size_t(frames) * 2, 0.0f);
    const int block = 512;
    size_t next = 0;
    for (int f = 0; f < frames; f += block) {
        const float t = float(f) / float(rate);
        while (next < ev.size() && ev[next].at <= t) {
            const Timed& e = ev[next++];
            if (e.ladder >= 0) mixer.ladder(e.ladder, false, e.gain, e.pan);
            else mixer.play(e.sound, e.gain, e.pan, e.pitch);
        }
        mixer.mix(out.data() + size_t(f) * 2, minv(block, frames - f));
    }
    return writeWav(outPath, out.data(), frames, rate) ? 0 : 5;
}

}  // namespace pm
