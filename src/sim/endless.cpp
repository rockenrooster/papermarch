// Endless (the owner, 2026-11-27: "we should also do an endless mode"): stage after stage, each a little longer, faster and
// fiercer than the last; the squad, its weapon and its power-ups carry over. A stage is laid as it is played, a section at
// a time about 110 units ahead of the squad, sized against the squad it really has then (no estimate to drift). Unlike the
// campaign's levels (src/gen, measured against the bot and baked) it is laid from formulas: a fight against a horde costs
// the same share of any squad past 50 (Battle::against, Battle::toll), so a horde's size and toughness set how hard it is.
#include "sim/endless.h"

#include <cmath>

namespace pm::sim {

namespace {

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
void wave(LevelDef& d, float z, float x0, float x1, i32 count, u8 kind, i32 hp) {
    if (d.waveCount < kMaxWaves && count > 0) d.waves[d.waveCount++] = {z, x0, x1, count, kind, hp};
}
i32 tough(u8 kind, float k) { return i32(float(enemyStats(kind).hp) * k + 0.5f); }

// What a weapon does to one big target, times the musket (the boss: no crowd for the lance, coil or flame to work on).
float onOne(u8 weapon) {
    static const float k[kWeaponCount] = {1.0f, 1.2f, 1.0f, 0.6f, 0.75f, 0.1f, 0.95f, 0.5f, 0.5f, 0.5f};
    return k[weapon < kWeaponCount ? weapon : 0];
}

// How hard stage k is (k from 1): a horde's foot and the last stand's, and how much tougher each enemy gets a stage
// (tuned with papermarch_headless --endless: the bot falls between stages 3 and 14, about 7 on average).
constexpr float kHordeBase = 110.0f, kHordeGrow = 30.0f, kStandBase = 150.0f, kStandGrow = 40.0f, kTougher = 0.04f;

}  // namespace

void Endless::begin(int stage, u64 seed, LevelDef& d) {
    k_ = stage;
    r_ = Rng(seed, u64(stage) * 2 + 7);
    // Clear what a stage holds (a LevelDef is large: no temporary).
    d.gateCount = d.barrelCount = d.waveCount = d.medalCount = d.mineCount = d.cageCount = d.rollerCount = d.wallCount = d.stripCount = 0;
    d.rowCount = d.planCount = 0;
    d.planLook = 0;
    d.start = 5;
    d.world = u8((stage - 1) % 10 + 1);
    d.boss = 0;
    d.bossHp = 0;
    d.bossAttacks = 0;
    d.hint = 0;
    d.seed = u32(seed);
    d.spike = 0;
    d.gun = kMusket;
    d.mode = kModeMarch;
    d.sky = kSkyClear;
    d.drain = 22;
    d.star2 = d.star3 = d.botEnd = 0;
    const float f = float(stage - 1);
    d.speed = minv(7.5f, 4.8f + 0.2f * f) * uni(r_, 0.93f, 1.07f);
    d.pace = minv(1.45f, 1.0f + 0.04f * f);
    if (stage >= 5 && r_.f01() < 0.2f) d.sky = r_.f01() < 0.5f ? kSkyFog : kSkyNight;  // fog or night now and then
    // About a minute of road at first, up to two later; the finish line far beyond until the end is laid.
    roadEnd_ = d.speed * minv(110.0f, 55.0f + 5.0f * f);
    d.length = roadEnd_ + 400.0f;
    z_ = 14.0f;
    since_ = section_ = 0;
    last_ = -1;
    sinceWeapon_ = 2;
    sincePower_ = 1;
    done_ = false;
}

void Endless::extend(LevelDef& d, Battle& b) {
    if (done_) return;
    const int k = k_;
    const float f = float(k - 1);
    Rng& r = r_;
    bool laid = false;
    while (z_ < b.z + 90.0f && z_ < roadEnd_) {
        laid = true;
        const float E = maxv(3.0f, float(b.squad));  // the squad it has now
        const float over = maxv(1.0f, E / (150.0f + 150.0f * float(k)));  // past it: no x2 or x3, smaller sums
        const bool grow = over <= 1.0f;
        float& z = z_;
        int kind;
        if (section_ == 0) kind = 0;
        else if (since_ >= 2 + int(r.next() % 2u)) kind = 4;  // a horde after every two or three others
        else {
            // columns, pairs, barrels, barricades, (horde), triples, a weapon, moving gates, rollers, flip, mystery, weather,
            // a hazard (a hammer, a pit, the narrows, the tide, mud)
            float w[13] = {0.8f, 2.2f, 1.6f, 1.1f, 0.0f, k >= 2 && grow ? 1.0f : 0.0f, sinceWeapon_ >= 4 ? 1.5f : 0.0f, k >= 2 ? 0.8f : 0.0f,
                           k >= 3 ? 0.8f : 0.0f, k >= 4 ? 0.7f : 0.0f, k >= 6 ? 0.6f : 0.0f, k >= 5 ? 0.6f : 0.0f, k >= 3 ? 0.8f : 0.0f};
            if (last_ >= 0 && last_ < 13) w[last_] *= 0.12f;
            float sum = 0;
            for (float v : w) sum += v;
            float x = r.f01() * sum;
            kind = 0;
            for (int i = 0; i < 13; ++i) {
                if (x < w[i]) {
                    kind = i;
                    break;
                }
                x -= w[i];
            }
        }
        last_ = kind;
        ++section_;
        ++sinceWeapon_;
        ++sincePower_;
        const bool left = r.f01() < 0.5f;
        switch (kind) {
            case 0: {  // a column of +1 panels down one side
                const int n = r.rangei(8, 16);
                for (int i = 0; i < n; ++i) gate(d, left ? -4.8f : 2.4f, left ? -2.4f : 4.8f, z + 1.3f * float(i), 1, kGateAdd, 1, 3);
                z += 1.3f * float(n) + 6.0f;
                ++since_;
                break;
            }
            case 1: {  // two gates: a sum and a sum, x2 or a cost
                i32 a = friendly(maxv(2.0f, E * uni(r, 0.3f, 0.9f) / over)), bb = friendly(maxv(2.0f, E * uni(r, 0.2f, 0.6f) / over));
                u8 kb = kGateAdd;
                const float y = r.f01();
                if (y < 0.35f && grow) kb = kGateMul, bb = 2;
                else if (y < 0.6f) bb = -bb;
                gate(d, left ? -4.6f : 0.25f, left ? -0.25f : 4.6f, z + 4.0f, a, kGateAdd, 1, a + 2 + a / 2);
                gate(d, left ? 0.25f : -4.6f, left ? 4.6f : -0.25f, z + 4.0f, bb, kb, kb == kGateAdd, bb < 0 ? -bb / 2 : bb + 2 + bb / 2);
                z += 12.0f;
                ++since_;
                break;
            }
            case 2: {  // barrels to shoot open
                const int n = r.rangei(2, 3);
                for (int i = 0; i < n && d.barrelCount < kMaxBarrels; ++i)
                    d.barrels[d.barrelCount++] = {n == 2 ? (i == 0 ? -2.8f : 2.8f) : -3.2f + 3.2f * float(i), z + 6.0f + uni(r, -1.0f, 1.0f),
                                                  maxv<i32>(4, i32(dps(E) * uni(r, 0.5f, 1.2f))), friendly(maxv(4.0f, E * uni(r, 0.2f, 0.5f) / over))};
                z += 14.0f;
                ++since_;
                break;
            }
            case 3: {  // a red barricade across the road
                const i32 a = friendly(maxv(2.0f, E * uni(r, 0.15f, 0.4f))), c = friendly(maxv(2.0f, E * uni(r, 0.2f, 0.45f)));
                gate(d, -4.9f, -0.15f, z + 6.0f, -a, kGateAdd, 1, maxv<i32>(1, a / 3));
                gate(d, 0.15f, 4.9f, z + 6.0f, -c, kGateAdd, 1, maxv<i32>(1, c / 3));
                z += 12.0f;
                ++since_;
                break;
            }
            case 5: {  // x3, a sum, a cost
                const i32 add = friendly(maxv(2.0f, E * uni(r, 0.8f, 1.5f) / over)), cost = friendly(maxv(2.0f, E * uni(r, 0.2f, 0.5f)));
                const float xs[3][2] = {{-4.8f, -1.75f}, {-1.5f, 1.5f}, {1.75f, 4.8f}};
                const int order = int(r.next() % 3u);
                for (int i = 0; i < 3; ++i) {
                    const int which = (i + order) % 3;
                    if (which == 0) gate(d, xs[i][0], xs[i][1], z + 4.0f, 3, kGateMul, 0, 3);
                    else if (which == 1) gate(d, xs[i][0], xs[i][1], z + 4.0f, add, kGateAdd, 1, add + add / 2);
                    else gate(d, xs[i][0], xs[i][1], z + 4.0f, -cost, kGateAdd, 1, 0);
                }
                z += 12.0f;
                ++since_;
                break;
            }
            case 6: {  // a weapon beside a modest sum
                const u8 wpn = u8(1 + r.next() % u32(kWeaponCount - 1));
                const i32 a = friendly(maxv(2.0f, E * uni(r, 0.15f, 0.4f) / over));
                gate(d, left ? -4.6f : 0.25f, left ? -0.25f : 4.6f, z + 4.0f, wpn, kGateWeapon, 0, 0);
                gate(d, left ? 0.25f : -4.6f, left ? 4.6f : -0.25f, z + 4.0f, a, kGateAdd, 1, a + 2 + a / 2);
                sinceWeapon_ = 0;
                z += 12.0f;
                ++since_;
                break;
            }
            case 7: {  // a gate on the move
                const float gx = uni(r, -2.0f, 2.0f);
                const i32 a = friendly(maxv(3.0f, E * uni(r, 0.4f, 0.9f) / over));
                gate(d, gx - 1.3f, gx + 1.3f, z + 4.0f, a, kGateAdd, 1, a + 2 + a / 2);
                if (d.gateCount > 0) d.gates[d.gateCount - 1].moves = u8(1 + r.next() % 255u);
                z += 15.0f;
                ++since_;
                break;
            }
            case 8: {  // rollers down their lanes
                const int n = 1 + int(r.next() % 3u);
                int lane = int(r.next() % 3u);
                for (int i = 0; i < n && d.rollerCount < kMaxRollers; ++i) {
                    d.rollers[d.rollerCount++] = {-2.6f + 2.6f * float(lane), z + 10.0f + 9.0f * float(i)};
                    lane = (lane + 1 + int(r.next() % 2u)) % 3;
                }
                z += 16.0f + 9.0f * float(n);
                ++since_;
                break;
            }
            case 9: {  // a flip gate over half the road
                const i32 a = friendly(maxv(3.0f, E * uni(r, 0.4f, 0.9f) / over)), c = friendly(maxv(2.0f, E * uni(r, 0.15f, 0.4f)));
                gate(d, left ? -4.6f : 0.25f, left ? -0.25f : 4.6f, z + 6.0f, a, kGateAdd, 0, a);
                if (d.gateCount > 0) {
                    d.gates[d.gateCount - 1].trick = 1;
                    d.gates[d.gateCount - 1].alt = -c;
                }
                z += 14.0f;
                ++since_;
                break;
            }
            case 10: {  // two mystery gates: one holds the sum, the other the cost
                const i32 a = friendly(maxv(3.0f, E * uni(r, 0.4f, 0.8f) / over)), c = friendly(maxv(2.0f, E * uni(r, 0.2f, 0.4f)));
                for (int side = 0; side < 2; ++side) {
                    const bool good = (side == 0) == left;
                    gate(d, side == 0 ? -4.6f : 0.25f, side == 0 ? -0.25f : 4.6f, z + 6.0f, good ? a : -c, kGateAdd, 1, good ? a + a / 2 : c / 3);
                    if (d.gateCount > 0) {
                        d.gates[d.gateCount - 1].trick = 2;
                        d.gates[d.gateCount - 1].alt = good ? -c : a;
                    }
                }
                z += 14.0f;
                ++since_;
                break;
            }
            case 11: {  // ice or wind with a pair of gates in it
                const bool windy = r.f01() < 0.5f;
                if (d.stripCount < kMaxStrips)
                    d.strips[d.stripCount++] = {z, z + 22.0f, u8(windy ? kStripWind : kStripIce), windy ? (left ? -1.0f : 1.0f) * uni(r, 2.5f, 3.5f) : 0.0f};
                const i32 a = friendly(maxv(3.0f, E * uni(r, 0.3f, 0.8f) / over)), c = friendly(maxv(2.0f, E * uni(r, 0.15f, 0.4f)));
                gate(d, left ? -4.6f : 0.25f, left ? -0.25f : 4.6f, z + 12.0f, a, kGateAdd, 1, a + 2 + a / 2);
                gate(d, left ? 0.25f : -4.6f, left ? 4.6f : -0.25f, z + 12.0f, -c, kGateAdd, 1, maxv<i32>(1, c / 3));
                z += 26.0f;
                ++since_;
                break;
            }
            case 12: {  // a hazard: hammers, a pit, the narrows, the tide or mud (the campaign's own, src/gen)
                const int hz = kStripMud + int(r.next() % 5u);
                const float side = left ? -1.0f : 1.0f;
                if (d.stripCount + 2 > kMaxStrips) {
                    z += 6.0f;
                } else if (hz == kStripHammer) {
                    const int n = 1 + int(r.next() % 2u);
                    for (int i = 0; i < n; ++i)
                        d.strips[d.stripCount++] = {z + 8.0f + 10.0f * float(i), z + 8.2f + 10.0f * float(i), u8(kStripHammer), uni(r, 0.0f, kHammerPeriod)};
                    z += 14.0f + 10.0f * float(n);
                } else if (hz == kStripPit) {
                    const float len = uni(r, 5.0f, 9.0f);
                    d.strips[d.stripCount++] = {z + 6.0f, z + 6.0f + len, u8(kStripPit), side * uni(r, 3.0f, 5.0f)};
                    z += len + 14.0f;
                } else {
                    const float len = hz == kStripNarrow ? uni(r, 10.0f, 16.0f) : uni(r, 14.0f, 20.0f);
                    d.strips[d.stripCount++] = {z + 4.0f, z + 4.0f + len, u8(hz), hz == kStripTide ? side : 0.0f};
                    z += len + 10.0f;
                }
                ++since_;
                break;
            }
            default: {  // a horde: its size and toughness grow stage after stage (a small squad meets a smaller one)
                if (k >= 3 && r.f01() < 0.3f)  // mines before it
                    for (int i = 0; i < 4 && d.mineCount < kMaxMines; ++i) d.mines[d.mineCount++] = {uni(r, -4.2f, 4.2f), z + uni(r, 3.0f, 9.0f)};
                if (k >= 4 && r.f01() < 0.2f && d.cageCount < kMaxCages)  // prisoners among it
                    d.cages[d.cageCount++] = {uni(r, -3.0f, 3.0f), z + 12.0f, friendly(maxv(5.0f, E * uni(r, 0.3f, 0.6f) / over))};
                if (sincePower_ >= 3 && d.medalCount < kMaxMedals) {  // a power-up on the way in
                    static const u8 kPowers[8] = {kRapid, kPowerShot, kShield, kMagnet, kGiants, kQuick, kHourglass, kDouble};
                    d.medals[d.medalCount++] = {uni(r, -3.0f, 3.0f), z + 3.0f, kPowers[r.next() % 8u]};
                    sincePower_ = 0;
                }
                const float scale = minv(1.0f, maxv(0.2f, E / 50.0f));  // a squad under 50 fires less
                const i32 foot = clampv<i32>(i32((kHordeBase + kHordeGrow * f) * uni(r, 0.7f, 1.3f) * scale), 5, 900);
                const float hk = 1.0f + kTougher * f;  // tougher stage after stage
                const bool full = r.f01() < 0.6f;
                const float cx = full ? 0.0f : (left ? -2.2f : 2.2f), half = full ? 4.3f : 2.4f;
                wave(d, z + 10.0f, cx - half, cx + half, foot, kFoot, tough(kFoot, 3.0f * hk));
                if (k >= 2 && r.f01() < 0.45f) wave(d, z + 14.0f, -4.0f, 4.0f, minv(maxv<i32>(3, foot / 5), 300), kRider, tough(kRider, 2.0f * hk));
                if (k >= 3 && r.f01() < 0.35f) wave(d, z + 18.0f, -3.0f, 3.0f, minv(maxv<i32>(1, foot / 20), 320), kBrute, tough(kBrute, 1.5f * hk));
                if (k >= 3 && r.f01() < 0.3f) wave(d, z + 21.0f, -4.2f, 4.2f, minv(maxv<i32>(3, foot / 12), 30), kArcher, tough(kArcher, 3.0f));
                if (k >= 4 && r.f01() < 0.3f) wave(d, z + 8.5f, cx - half, cx + half, minv(maxv<i32>(3, foot / 8), 60), kShieldman, tough(kShieldman, 2.0f * hk));
                if (k >= 5 && r.f01() < 0.3f) wave(d, z + 12.0f, cx - half, cx + half, minv(maxv<i32>(2, foot / 15), 40), kBomber, tough(kBomber, 2.0f * hk));
                if (k >= 4 && r.f01() < 0.25f) {  // a catapult at the road's side beyond it
                    const float catX = r.f01() < 0.5f ? -3.6f : 3.6f;
                    wave(d, z + 26.0f, catX, catX, 1, kCatapult, 0);
                }
                if (k >= 5 && r.f01() < 0.2f) wave(d, z + 9.0f, -0.6f, 0.6f, 1, kGiant, maxv<i32>(30, i32(dps(E) * 2.5f)));
                if (k >= 6 && r.f01() < 0.2f) wave(d, z + 13.0f, cx - 1.0f, cx + 1.0f, 2, kDrummer, tough(kDrummer, 2.0f * hk));
                z += 24.0f;
                since_ = 0;
                break;
            }
        }
    }
    // The end, laid once the squad is about to meet it (sized from the squad it has then): every third stage the ogre
    // (about 3 to 8 seconds of its fire), else a last stand 50 beyond the road. From then on the stage is won when every
    // enemy is beaten (or the boss falls). Until then the road's end holds its place (no finish line before it).
    if (z_ >= roadEnd_ && !(k % 3 == 0) && d.length > z_ + 100.0f) d.length = z_ + 76.0f;
    if (z_ >= roadEnd_ && k % 3 == 0 && d.length > z_ + 100.0f) d.length = z_ + 24.0f, d.bossHp = 1;  // the boss's place
    if (z_ >= roadEnd_ && b.z >= z_ + (k % 3 == 0 ? 24.0f : 60.0f) - 70.0f) {
        const float E = maxv(3.0f, float(b.squad));
        if (k % 3 == 0) {
            d.boss = 1;
            d.bossHp = maxv<i32>(20, i32(dps(E) * onOne(b.weapon) * minv(7.0f, 3.0f + 0.2f * f)));
            d.bossAttacks = u8(k >= 6 ? 3 : k >= 3 ? 1 : 0);
        } else {
            const float scale = minv(1.0f, maxv(0.2f, E / 50.0f));
            const i32 foot = clampv<i32>(i32((kStandBase + kStandGrow * f) * scale), 8, 900);
            const float hk = 1.0f + kTougher * f;
            wave(d, z_ + 50.0f, -4.3f, 4.3f, foot, kFoot, tough(kFoot, 3.0f * hk));
            if (k >= 2) wave(d, z_ + 55.0f, -4.0f, 4.0f, minv(maxv<i32>(3, foot / 5), 300), kRider, tough(kRider, 2.0f * hk));
            if (k >= 3) wave(d, z_ + 59.0f, -3.5f, 3.5f, minv(maxv<i32>(1, foot / 20), 320), kBrute, tough(kBrute, 1.5f * hk));
        }
        done_ = true;
        laid = true;
    }
    if (laid) indexRows(d);
    b.clearWins = done_;  // until the end is laid, a road beaten so far is not the stage won
}

}  // namespace pm::sim
