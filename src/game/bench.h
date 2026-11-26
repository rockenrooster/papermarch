#pragma once
// Frame timing for benchmarks (TECH_SPEC 8.3): swap intervals, main-thread phases, the display's present times,
// simulation tick time, counts and heap allocations over a measured window; one GTBENCH line and a JSON file.

#include "core/types.h"

namespace gt {

enum Phase { kPhaseEvents, kPhaseSim, kPhaseView, kPhaseBuild, kPhaseSubmit, kPhaseSwap, kPhaseCount };

struct BenchFrame {
    float interval = 0;   // s, swap call to swap call
    float cpu = 0;        // s, main thread without the swap
    float phase[kPhaseCount] = {};
    float tickMax = 0;    // s, the slowest simulation tick in the frame
    int wisps = 0, glooms = 0, particles = 0;
    int drawCalls = 0;
};

class Bench {
public:
    void begin(const char* scenario, float seconds, double now);
    bool active() const { return active_; }
    bool done() const { return done_; }
    bool valid() const { return valid_; }
    void addFrame(const BenchFrame& f);
    void addPresents(const double* t, int n);
    void focusLost() { valid_ = false; }
    // Writes the result; `extra` is a JSON fragment (",\"key\":...") appended to the object.
    void finish(const char* device, const char* glRenderer, const char* glVersion, float refreshHz, const char* extra);

private:
    static constexpr int kMax = 40000;
    bool active_ = false, done_ = false, valid_ = true;
    char scenario_[24] = "";
    float seconds_ = 0;
    double start_ = 0;
    int n_ = 0;
    BenchFrame* frames_ = nullptr;
    double* presents_ = nullptr;
    int presentCount_ = 0;
    u64 allocStart_ = 0;
};

}  // namespace gt
