#pragma once
// PCG32 (O'Neill): small, fast, and the same sequence on every platform. Floats are made from integer bits only.

#include "core/types.h"

namespace pm {

struct Rng {
    u64 state = 0x853c49e6748fea9bULL;
    u64 inc = 0xda3e39cb94b95bdbULL;

    Rng() = default;
    explicit Rng(u64 seed, u64 stream = 54) { reseed(seed, stream); }

    void reseed(u64 seed, u64 stream = 54) {
        state = 0;
        inc = (stream << 1u) | 1u;
        next();
        state += seed;
        next();
    }
    u32 next() {
        const u64 old = state;
        state = old * 6364136223846793005ULL + inc;
        const u32 xorshifted = u32(((old >> 18u) ^ old) >> 27u);
        const u32 rot = u32(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }
    // [0, 1) with 24 bits.
    float f01() { return float(next() >> 8) * (1.0f / 16777216.0f); }
    float range(float lo, float hi) { return lo + (hi - lo) * f01(); }
    // [lo, hi] inclusive.
    int rangei(int lo, int hi) { return lo + int(next() % u32(hi - lo + 1)); }
};

// A stateless hash of an integer (splitmix64 finaliser), for seeds derived from indices.
inline u64 hash64(u64 x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

}  // namespace pm
