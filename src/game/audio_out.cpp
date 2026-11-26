#include "game/audio_out.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstring>

#include "core/clock.h"
#include "core/log.h"

namespace gt {

namespace {

int binOf(float seconds) {
    const int b = int(seconds * 20000.0f);  // 0.05 ms bins
    return b < 0 ? 0 : (b > 399 ? 399 : b);
}

// p in [0,1] from a histogram, in ms (bin centre).
float histPercentile(const u32* h, int bins, float p) {
    u64 total = 0;
    for (int i = 0; i < bins; ++i) total += h[i];
    if (!total) return 0;
    const u64 want = u64(double(total) * double(p) + 0.5);
    u64 acc = 0;
    for (int i = 0; i < bins; ++i) {
        acc += h[i];
        if (acc >= want && acc > 0) return (float(i) + 0.5f) * 0.05f;
    }
    return float(bins) * 0.05f;
}

}  // namespace

bool AudioOut::open(bool silent, AudioFill fill, void* user) {
    silent_ = silent;
    fill_ = fill;
    user_ = user;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        GT_WARN("audio: %s", SDL_GetError());
        return false;
    }
    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = 48000;
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioOut::callback, this);
    if (!stream_) {
        GT_WARN("audio: no device (%s)", SDL_GetError());
        return false;
    }
    int frames = 0;
    SDL_AudioSpec dev;
    if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream_), &dev, &frames)) {
        bufferSeconds_ = frames > 0 && dev.freq > 0 ? float(frames) / float(dev.freq) : 0.01f;
        GT_LOG("audio: device %d Hz, %d channels, %d frames a buffer%s", dev.freq, dev.channels, frames,
               silent_ ? " (silent: output zeroed)" : "");
    }
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

void AudioOut::close() {
    if (stream_) {
        const AudioStats s = stats();
        GT_LOG("audio: %d callbacks, fill p99 %.2f max %.2f ms, gap max %.1f ms, %d late, period %.1f ms", s.callbacks,
               double(s.fillMs[1]), double(s.fillMs[2]), double(s.gapMs[2]), s.late, double(s.periodMs));
        if (silent_)
            GT_LOG("audio: silent: peak before the mute %.3f, %lld non-zero samples handed to the device", double(s.peakBeforeMute),
                   s.nonZeroHanded);
        SDL_DestroyAudioStream(stream_);
    }
    stream_ = nullptr;
}

void AudioOut::pause(bool paused) {
    if (!stream_) return;
    if (paused) {
        SDL_PauseAudioStreamDevice(stream_);
    } else {
        resetPending_.store(true);  // the gap across the pause is not a late callback
        SDL_ResumeAudioStreamDevice(stream_);
    }
}

AudioStats AudioOut::stats() const {
    AudioStats s;
    s.callbacks = calls_;
    s.late = late_;
    s.periodMs = bufferSeconds_ * 1000.0f;
    s.fillMs[0] = histPercentile(fillHist_, kBins, 0.50f);
    s.fillMs[1] = histPercentile(fillHist_, kBins, 0.99f);
    s.fillMs[2] = maxFill_ * 1000.0f;
    s.gapMs[0] = histPercentile(gapHist_, kBins, 0.50f);
    s.gapMs[1] = histPercentile(gapHist_, kBins, 0.99f);
    s.gapMs[2] = maxGap_ * 1000.0f;
    s.peakBeforeMute = peakBeforeMute_;
    s.nonZeroHanded = nonZeroHanded_;
    return s;
}

void AudioOut::callback(void* user, SDL_AudioStream* stream, int additional, int) {
    AudioOut* self = static_cast<AudioOut*>(user);
    const double t0 = nowSeconds();
    if (self->resetPending_.exchange(false)) {
        std::memset(self->fillHist_, 0, sizeof self->fillHist_);
        std::memset(self->gapHist_, 0, sizeof self->gapHist_);
        self->maxFill_ = self->maxGap_ = 0;
        self->late_ = self->calls_ = 0;
        self->lastCall_ = 0;
    }
    if (self->lastCall_ > 0) {
        const float gap = float(t0 - self->lastCall_);
        ++self->gapHist_[binOf(gap)];
        if (gap > self->maxGap_) self->maxGap_ = gap;
        if (self->bufferSeconds_ > 0 && gap > 2.0f * self->bufferSeconds_) ++self->late_;
    }
    self->lastCall_ = t0;
    ++self->calls_;
    int frames = additional / int(sizeof(float) * 2);
    while (frames > 0) {
        const int n = frames < 4096 ? frames : 4096;
        if (self->fill_) {
            self->fill_(self->user_, self->buf_, n);
        } else {
            std::memset(self->buf_, 0, sizeof(float) * 2 * size_t(n));
        }
        if (self->silent_) {
            float peak = 0;
            for (int i = 0; i < n * 2; ++i) peak = std::fmax(peak, std::fabs(self->buf_[i]));
            if (peak > self->peakBeforeMute_) self->peakBeforeMute_ = peak;
            std::memset(self->buf_, 0, sizeof(float) * 2 * size_t(n));
            for (int i = 0; i < n * 2; ++i) self->nonZeroHanded_ += self->buf_[i] != 0.0f;  // the proof, not an assumption
        }
        SDL_PutAudioStreamData(stream, self->buf_, int(sizeof(float) * 2 * size_t(n)));
        frames -= n;
    }
    const float fill = float(nowSeconds() - t0);
    ++self->fillHist_[binOf(fill)];
    if (fill > self->maxFill_) self->maxFill_ = fill;
}

}  // namespace gt
