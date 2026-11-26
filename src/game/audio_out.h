#pragma once
// The audio device (TECH_SPEC 6): an SDL audio stream, float32 stereo 48 kHz, filled from its callback by the mixer.
// --silent runs everything and zeroes the buffer just before it is handed over; --no-audio opens no device.
// The callback measures its own duration and the gap since the previous call into small histograms (0.05 ms bins)
// that benchmarks report; a late callback is a gap above two buffer periods.

#include <atomic>

#include "core/types.h"

struct SDL_AudioStream;

namespace gt {

using AudioFill = void (*)(void* user, float* stereo, int frames);

struct AudioStats {
    int callbacks = 0, late = 0;
    float periodMs = 0;
    float fillMs[3] = {};  // p50, p99, max
    float gapMs[3] = {};   // p50, p99, max
    float peakBeforeMute = 0;
    long long nonZeroHanded = 0;  // --silent: samples handed to the device that were not zero (must stay 0)
};

class AudioOut {
public:
    bool open(bool silent, AudioFill fill, void* user);
    void close();
    void pause(bool paused);
    bool isOpen() const { return stream_ != nullptr; }
    // Main thread: clears the statistics at the audio thread's next callback.
    void resetStats() { resetPending_.store(true); }
    AudioStats stats() const;

private:
    static void callback(void* user, SDL_AudioStream* stream, int additional, int total);
    static constexpr int kBins = 400;  // 0.05 ms each, the last bin holds everything above 20 ms
    SDL_AudioStream* stream_ = nullptr;
    AudioFill fill_ = nullptr;
    void* user_ = nullptr;
    bool silent_ = false;
    double lastCall_ = 0;
    float bufferSeconds_ = 0;
    std::atomic<bool> resetPending_{false};
    // Written by the audio thread only; read approximately by the main thread when a benchmark ends.
    u32 fillHist_[kBins] = {}, gapHist_[kBins] = {};
    float maxFill_ = 0, maxGap_ = 0, peakBeforeMute_ = 0;
    long long nonZeroHanded_ = 0;
    int late_ = 0, calls_ = 0;
    float buf_[2 * 4096];
};

}  // namespace gt
