#pragma once
// Wall-clock helpers for the app and tools. Never used by src/sim (determinism, TECH_SPEC 4.2).

#include <algorithm>
#include <chrono>

#include "core/types.h"

namespace pm {

inline double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// CPU time used by the calling thread (excludes time blocked in the driver or in vsync). Coarse on Windows.
double threadCpuSeconds();

// Percentile of the first n values of a scratch copy (sorted in place). p in [0, 1].
inline float percentile(float* v, int n, float p) {
    if (n <= 0) return 0.0f;
    std::sort(v, v + n);
    const int i = clampv(int(p * float(n - 1) + 0.5f), 0, n - 1);
    return v[i];
}

}  // namespace pm
