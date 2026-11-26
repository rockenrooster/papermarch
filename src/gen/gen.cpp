// The generator (TECH_SPEC 4): sections laid up the road from an estimate of the squad, then the bake's dial.
#include "gen/gen.h"

#include <cmath>
#include <cstdio>

#include "core/rng.h"
#include "sim/bot.h"

namespace pm::gen {

namespace {

using namespace sim;

float uni(Rng& r, float lo, float hi) { return lo + (hi - lo) * r.f01(); }

i32 friendly(float v) {
    const i32 n = i32(v + 0.5f);
    if (n < 30) return maxv<i32>(1, n);
    if (n < 300) return n / 5 * 5;
    if (n < 3000) return n / 10 * 10;
    return n / 50 * 50;
}

// Shots a second times damage: what a squad of `s` deals (the rules' own numbers).
float dps(float s) { return (5.0f + 2.4f * minv(s, 50.0f)) * maxv(1.0f, s / 50.0f); }

void gate(LevelDef& d, float x0, float x1, float z, i32 v, u8 kind, u8 grows, i32 cap) {
    if (d.gateCount < kMaxGates) d.gates[d.gateCount++] = {x0, x1, z, v, kind, grows, cap};
}

}  // namespace

void generate(int n, u64 seed, float m, LevelDef& out) {
    Rng r(seed, u64(n) * 2 + 1);
    LevelDef d;
    d.seed = u32(seed);
    d.world = u8((n - 1) / 10 % 10 + 1);
    d.boss = n % 10 == 0;
    d.length = 96.0f + float(minv(n, 150)) * 0.7f + (d.boss ? 24.0f : 0.0f);
    d.speed = 4.6f + float(minv(n, 120)) * 0.008f;
    d.start = n <= 5 ? 3 : 3 + int(r.next() % 4u);
    switch (n) {  // introductions: their hint lines (src/sim/levels.cpp)
        case 1: d.hint = 1; break;
        case 2: d.hint = 2; break;
        case 3: d.hint = 3; break;
        case 4: d.hint = 4; break;
        case 5: d.hint = 5; break;
        case 6: d.hint = 6; break;
        case 7: d.hint = 7; break;
        case 8: d.hint = 8; break;
        case 12: d.hint = 9; break;
        case 16: d.hint = 10; break;
        default: break;
    }
    const bool pairs = n >= 2, barrels = n >= 4, mul = n >= 5, blocks = n >= 6, riders = n >= 7, brutes = n >= 12, triple = n >= 20;
    const bool bossHere = d.boss || n == 8 || (n > 8 && r.f01() < 0.45f);
    float E = float(d.start);  // the squad the generator expects at this point of the road
    float z = 14.0f;
    const float end = d.length - 24.0f;
    int sinceWave = 0;
    int section = 0, last = -1;
    while (z < end) {
        // What comes next: a horde after every two or three other sections; else a mix of what this level knows, never
        // the same twice running. The first section is always gates.
        int kind;
        if (section == 0) kind = n < 10 || r.f01() < 0.5f ? 0 : 1;
        else if (sinceWave >= 2 + int(r.next() % 2u)) kind = 4;
        else {
            float w[6] = {n <= 3 ? 3.0f : 0.9f, pairs ? 2.2f : 0.0f, barrels ? 1.7f : 0.0f, blocks ? 1.2f : 0.0f, 0.0f, triple ? 1.1f : 0.0f};
            if (last >= 0 && last < 6) w[last] *= 0.12f;
            float sum = 0;
            for (float v : w) sum += v;
            float x = r.f01() * sum;
            kind = 0;
            for (int k = 0; k < 6; ++k) {
                if (x < w[k]) {
                    kind = k;
                    break;
                }
                x -= w[k];
            }
        }
        last = kind;
        ++section;
        switch (kind) {
            case 0: {  // a column of +1 panels down one side
                const int k = r.rangei(8, 16 + minv(n, 40) / 4);
                const bool left = r.f01() < 0.5f;
                for (int i = 0; i < k; ++i) gate(d, left ? -4.8f : 2.4f, left ? -2.4f : 4.8f, z + 1.3f * float(i), 1, kGateAdd, 1, 3);
                E += float(k) * 1.6f;
                z += 1.3f * float(k) + 6.0f;
                ++sinceWave;
                break;
            }
            case 1: {  // two gates side by side
                const bool left = r.f01() < 0.5f;
                i32 a = friendly(maxv(2.0f, E * uni(r, 0.25f, 0.9f))), b = friendly(maxv(2.0f, E * uni(r, 0.2f, 0.7f)));
                u8 ka = kGateAdd, kb = kGateAdd;
                const float y = r.f01();
                if (mul && y < 0.4f) {  // x2 against a fair sum
                    kb = kGateMul;
                    b = 2;
                    a = friendly(maxv(2.0f, E * uni(r, 0.6f, 1.3f)));
                } else if (blocks && y < 0.6f) {  // one side hurts
                    b = -friendly(maxv(2.0f, E * uni(r, 0.2f, 0.6f)));
                }
                const float zx = z + 4.0f;
                gate(d, left ? -4.6f : 0.25f, left ? -0.25f : 4.6f, zx, a, ka, ka == kGateAdd, a + 2 + a / 2);
                gate(d, left ? 0.25f : -4.6f, left ? 4.6f : -0.25f, zx, b, kb, kb == kGateAdd, b < 0 ? -b / 2 : b + 2 + b / 2);
                const float best = maxv(E + float(a), kb == kGateMul ? E * 2.0f : E + float(maxv<i32>(b, 0)));
                E = best;
                z += 12.0f;
                ++sinceWave;
                break;
            }
            case 2: {  // barrels to shoot open
                const int k = r.rangei(2, 3);
                const float zx = z + 6.0f;
                for (int i = 0; i < k; ++i) {
                    const float bx = k == 2 ? (i == 0 ? -2.8f : 2.8f) : -3.2f + 3.2f * float(i);
                    const i32 hp = maxv<i32>(4, i32(dps(E) * uni(r, 0.5f, 1.4f)));
                    const i32 reward = friendly(maxv(4.0f, E * uni(r, 0.25f, 0.6f)));
                    if (d.barrelCount < kMaxBarrels) d.barrels[d.barrelCount++] = {bx, zx + uni(r, -1.0f, 1.0f), hp, reward};
                    E += float(reward) * 0.6f;
                }
                z += 14.0f;
                ++sinceWave;
                break;
            }
            case 3: {  // a red barricade across the road: shoot it up, or pay
                const i32 a = friendly(maxv(2.0f, E * uni(r, 0.15f, 0.5f))), b = friendly(maxv(2.0f, E * uni(r, 0.2f, 0.6f)));
                const float zx = z + 6.0f;
                gate(d, -4.9f, -0.15f, zx, -a, kGateAdd, 1, maxv<i32>(1, a / 3));
                gate(d, 0.15f, 4.9f, zx, -b, kGateAdd, 1, maxv<i32>(1, b / 3));
                E -= float(minv(a, b)) * 0.4f;
                z += 12.0f;
                ++sinceWave;
                break;
            }
            case 5: {  // three gates: x3, a sum, a cost
                const float zx = z + 4.0f;
                const int order = int(r.next() % 3u);
                const i32 add = friendly(maxv(2.0f, E * uni(r, 0.8f, 1.6f)));
                const i32 cost = friendly(maxv(2.0f, E * uni(r, 0.2f, 0.5f)));
                const float xs[3][2] = {{-4.8f, -1.75f}, {-1.5f, 1.5f}, {1.75f, 4.8f}};
                for (int k = 0; k < 3; ++k) {
                    const int which = (k + order) % 3;
                    if (which == 0) gate(d, xs[k][0], xs[k][1], zx, 3, kGateMul, 0, 3);
                    else if (which == 1) gate(d, xs[k][0], xs[k][1], zx, add, kGateAdd, 1, add + add / 2);
                    else gate(d, xs[k][0], xs[k][1], zx, -cost, kGateAdd, 1, 0);
                }
                E = maxv(E * 3.0f, E + float(add));
                z += 12.0f;
                ++sinceWave;
                break;
            }
            default: {  // a horde
                const float share = uni(r, 0.7f, 1.4f) * m;
                const bool full = r.f01() < 0.6f;
                const float cx = full ? 0.0f : (r.f01() < 0.5f ? -2.2f : 2.2f);
                const float half = full ? 4.3f : 2.4f;
                const i32 foot = maxv<i32>(4, i32(E * share));
                if (d.waveCount < kMaxWaves) d.waves[d.waveCount++] = {z + 10.0f, cx - half, cx + half, minv(foot, 900), kFoot};
                if (riders && r.f01() < 0.4f && d.waveCount < kMaxWaves)
                    d.waves[d.waveCount++] = {z + 14.0f, -4.0f, 4.0f, maxv<i32>(3, foot / 4), kRider};
                if (brutes && r.f01() < 0.35f && d.waveCount < kMaxWaves)
                    d.waves[d.waveCount++] = {z + 18.0f, -3.0f, 3.0f, maxv<i32>(1, foot / 12), kBrute};
                E = maxv(float(d.start), E - float(foot) * 0.25f);
                z += 22.0f;
                sinceWave = 0;
                break;
            }
        }
    }
    // Every level has a horde to crush, if only a small one near the end.
    if (d.waveCount == 0) {
        const i32 foot = maxv<i32>(5, i32(E * uni(r, 0.6f, 1.1f) * m));
        d.waves[d.waveCount++] = {end - 2.0f, -3.6f, 3.6f, minv(foot, 900), kFoot};
        E = maxv(float(d.start), E - float(foot) * 0.25f);
    }
    // The boss: as many hit points as the expected squad deals in about eight seconds.
    if (bossHere) d.bossHp = maxv<i32>(20, i32(dps(E) * uni(r, 5.0f, 9.0f) * m));
    // Through the text form, so the bot plays exactly what the table will hold.
    static thread_local char buf[16384];
    if (!encodeLevel(d, buf, sizeof buf) || !decodeLevel(buf, out)) out = d;
}

Baked bakeLevel(int n, int maxSeeds) {
    Baked best;
    // The margin the bot must win by: the share of its peak squad still standing at the end.
    const float lo = n <= 10 ? 0.45f : (n <= 40 ? 0.3f : (n <= 120 ? 0.18f : 0.1f));
    const float hi = n <= 10 ? 1.0f : (n <= 40 ? 0.75f : (n <= 120 ? 0.55f : 0.4f));
    float bestErr = 1e9f;
    for (int s = 0; s < maxSeeds; ++s) {
        const u64 seed = hash64(u64(n) * 1000003ULL + u64(s));
        float mlo = 0.2f, mhi = 4.0f;
        for (int it = 0; it < 9; ++it) {
            const float m = (mlo + mhi) * 0.5f;
            LevelDef d;
            generate(n, seed, m, d);
            i32 left = 0, peak = 0;
            const bool won = botPlay(d, &left, &peak);
            ++best.plays;
            const float ratio = won ? float(left) / float(maxv<i32>(peak, 1)) : -1.0f;
            if (won && ratio >= lo && ratio <= hi) {
                best.d = d;
                best.ok = true;
                best.botEnd = left;
                best.botPeak = peak;
                best.m = m;
                best.d.botEnd = left;
                best.d.star3 = maxv<i32>(1, i32(std::ceil(float(left) * 0.9f)));
                best.d.star2 = maxv<i32>(1, i32(std::ceil(float(left) * 0.45f)));
                return best;
            }
            if (!won || ratio < lo) mhi = m;  // too hard
            else mlo = m;                     // too easy
            const float err = won ? (ratio < lo ? lo - ratio : ratio - hi) : 10.0f + m;
            if (won && err < bestErr) {
                bestErr = err;
                best.d = d;
                best.botEnd = left;
                best.botPeak = peak;
                best.m = m;
            }
        }
    }
    // No seed met the band: keep the closest win, said so in the report.
    if (best.botEnd > 0) {
        best.d.botEnd = best.botEnd;
        best.d.star3 = maxv<i32>(1, i32(std::ceil(float(best.botEnd) * 0.9f)));
        best.d.star2 = maxv<i32>(1, i32(std::ceil(float(best.botEnd) * 0.45f)));
        best.ok = true;
        std::snprintf(best.why, sizeof best.why, "band missed by %.2f", double(bestErr));
    } else {
        std::snprintf(best.why, sizeof best.why, "the bot never won");
    }
    return best;
}

}  // namespace pm::gen
