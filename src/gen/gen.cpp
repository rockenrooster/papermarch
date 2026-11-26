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

// The squad the bot has at `zEnd` on the road built so far (no boss; the level ends there): what the next section is
// sized from. 0 if the bot does not get there.
float squadAt(const LevelDef& d, float zEnd) {
    thread_local LevelDef* tp = nullptr;  // large: one per thread
    if (!tp) tp = new LevelDef();
    LevelDef& t = *tp;
    t = d;
    t.length = zEnd;
    t.bossHp = 0;
    t.planLook = 0;
    t.planCount = 0;
    indexRows(t);
    const PlayResult r = playLevel(t, 150);
    return r.won ? float(r.left) : 0.0f;
}

}  // namespace

bool isSpike(int n) {
    auto hard = [](int k) { return k >= 15 && hash64(u64(k) * 7919ULL + 17ULL) % 9ULL == 0; };
    return hard(n) && !hard(n - 1);
}

float marginTarget(int n) {
    float t = 0.02f + 0.6f * std::exp(-float(n) / 65.0f);
    if (n % 10 == 0) t *= 0.8f;          // a boss's level, a little harder
    if (isSpike(n)) t = maxv(0.012f, t * 0.3f);
    return t;
}

// The share of its squad each horde on the way costs the bot: a nibble early on, a third late, more on the hard levels.
float bite(int n) {
    const float b = 0.05f + 0.3f * minv(1.0f, float(n - 1) / 199.0f);
    return isSpike(n) ? b * 1.4f : b;
}

void build(int n, u64 seed, Shape& out) {
    Rng r(seed, u64(n) * 2 + 1);
    out = Shape{};
    LevelDef& d = out.d;
    d.seed = u32(seed);
    d.world = u8((n - 1) / 10 % 10 + 1);
    d.boss = n % 10 == 0;
    // Faster as the game goes on: the march from 4.6 to 7.0 units a second, the horde's pace up to 1.35 times; a hard
    // level faster still. Longer roads keep a level near half a minute.
    const float late = float(n - 1) / 299.0f;
    d.speed = (4.6f + 2.4f * late) * (isSpike(n) ? 1.08f : 1.0f);
    d.pace = (1.0f + 0.35f * late) * (isSpike(n) ? 1.1f : 1.0f);
    d.length = 96.0f + float(minv(n, 150)) * 0.7f + float(maxv(0, n - 150)) * 0.25f + (d.boss ? 24.0f : 0.0f);
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
    const bool bossHere = d.boss || n == 8 || isSpike(n) || (n > 8 && r.f01() < (n > 100 ? 0.6f : 0.45f));
    // E: the squad the bot really has where the next section starts, measured by playing the road built so far, so
    // every section is sized against it (gates worth taking, barrels worth shooting, hordes that bite).
    float E = float(d.start);
    float z = 14.0f;
    const float end = d.length - 24.0f;
    int sinceWave = 0;
    int section = 0, last = -1;
    while (z < end) {
        const int gatesBefore = d.gateCount, barrelsBefore = d.barrelCount, wavesBefore = d.waveCount;
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
                z += 12.0f;
                ++sinceWave;
                break;
            }
            default: {  // a horde, as big as it must be to cost the bot about `bite` of its squad (measured)
                const bool full = r.f01() < 0.6f;
                const float cx = full ? 0.0f : (r.f01() < 0.5f ? -2.2f : 2.2f);
                const float half = full ? 4.3f : 2.4f;
                const bool withRiders = riders && r.f01() < 0.4f, withBrutes = brutes && r.f01() < 0.35f;
                const float want = bite(n) * uni(r, 0.6f, 1.4f) * E;
                const float zw = z;
                auto lay = [&](float count) {  // this section's waves for `count` foot; past 900 the rest are brutes
                    d.waveCount = wavesBefore;
                    const i32 foot = maxv<i32>(3, i32(count));
                    d.waves[d.waveCount++] = {zw + 10.0f, cx - half, cx + half, minv(foot, 900), kFoot};
                    if (withRiders) d.waves[d.waveCount++] = {zw + 14.0f, -4.0f, 4.0f, minv(maxv<i32>(3, foot / 4), 300), kRider};
                    const i32 heavy = (withBrutes ? maxv<i32>(1, foot / 12) : 0) + maxv<i32>(0, foot - 900) / 8;
                    if (heavy > 0) d.waves[d.waveCount++] = {zw + 18.0f, -3.0f, 3.0f, minv(heavy, 320), kBrute};
                };
                float lo = 2.0f, hi = maxv(60.0f, E * 14.0f), pick = lo;
                for (int it = 0; it < 7 && d.waveCount + 3 <= kMaxWaves; ++it) {
                    const float mid = std::sqrt(lo * hi);
                    lay(mid);
                    const float after = squadAt(d, zw + 47.0f);  // the horde fought to its end
                    if (after < 1.0f || E - after > want * 1.2f) {
                        hi = mid;
                    } else {
                        lo = pick = mid;
                        if (E - after >= want * 0.8f) break;
                    }
                }
                if (d.waveCount + 3 <= kMaxWaves) lay(pick);
                z += 22.0f;
                sinceWave = 0;
                break;
            }
        }
        // What the bot has after this section (a horde fought to its end); a section it cannot get through is dropped.
        const float after = squadAt(d, z + (d.waveCount > wavesBefore ? 25.0f : 2.0f));
        if (after >= 1.0f) {
            E = after;
        } else {
            d.gateCount = gatesBefore;
            d.barrelCount = barrelsBefore;
            d.waveCount = wavesBefore;
        }
    }
    // The end: the boss, or a last stand of the horde across the road; finish() sizes it.
    out.boss = bossHere;
    out.endSquad = E;
    out.lastZ = end - 2.0f;
    out.lastRiders = riders;
    out.lastBrutes = brutes;
}

void finish(const Shape& s, float m, LevelDef& out) {
    thread_local LevelDef* dp = nullptr;  // large: one per thread
    if (!dp) dp = new LevelDef();
    LevelDef& d = *dp;
    d = s.d;
    if (s.boss) {  // as many hit points as the squad at the end deals in m seconds
        d.bossHp = maxv<i32>(20, i32(dps(s.endSquad) * m));
    } else if (d.waveCount + 3 <= kMaxWaves) {  // m times the squad's number, foot and riders and brutes
        const i32 foot = maxv<i32>(5, i32(s.endSquad * m));
        d.waves[d.waveCount++] = {s.lastZ, -4.3f, 4.3f, minv(foot, 900), kFoot};
        if (s.lastRiders) d.waves[d.waveCount++] = {s.lastZ + 5.0f, -4.0f, 4.0f, minv(maxv<i32>(3, foot / 4), 300), kRider};
        const i32 heavy = (s.lastBrutes ? maxv<i32>(1, foot / 10) : 0) + maxv<i32>(0, foot - 900) / 8;
        if (heavy > 0) d.waves[d.waveCount++] = {s.lastZ + 9.0f, -3.5f, 3.5f, minv(heavy, 320), kBrute};
    }
    // Through the text form, so the bot plays exactly what the table will hold.
    static thread_local char buf[16384];
    if (!encodeLevel(d, buf, sizeof buf) || !decodeLevel(buf, out)) out = d;
}

void generate(int n, u64 seed, float m, LevelDef& out) {
    thread_local Shape* sp = nullptr;
    if (!sp) sp = new Shape();
    build(n, seed, *sp);
    finish(*sp, m, out);
}

Baked bakeLevel(int n, int maxSeeds) {
    Baked best;
    // The margin the best run (the planner's) must win by: the share of its peak squad still standing at the end. It
    // falls from about 60% to under 3% at level 300; the hard levels thrown in ask for a third of their neighbours'.
    const float target = marginTarget(n);
    const float lo = n <= 10 ? 0.45f : target * 0.6f;
    const float hi = n <= 10 ? 1.0f : minv(1.0f, target * 1.5f);
    best.target = target;
    float bestErr = 1e9f;
    thread_local Shape* sp = nullptr;  // large: one per thread
    if (!sp) sp = new Shape();
    thread_local LevelDef* dp = nullptr;
    if (!dp) dp = new LevelDef();
    for (int s = 0; s < maxSeeds; ++s) {
        const u64 seed = hash64(u64(n) * 1000003ULL + u64(s));
        // The road once (its hordes measured against the bot), then the end fight's strength found by halving.
        build(n, seed, *sp);
        if (sp->endSquad < 1.0f) continue;
        float mlo = 0.05f, mhi = 90.0f;
        for (int it = 0; it < 12; ++it) {
            const float m = std::sqrt(mlo * mhi);
            LevelDef& d = *dp;
            finish(*sp, m, d);
            d.spike = isSpike(n) ? 1 : 0;
            const PlayResult pr = planLevel(d);
            const bool won = pr.won;
            const i32 left = pr.left, peak = pr.peak;
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
