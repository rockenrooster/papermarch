// The generator (TECH_SPEC 4): sections laid up the road from an estimate of the squad, then the bake's dial.
#include "gen/gen.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

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

// Each enemy's hit points (musket balls) in a horde: three times its kind's (times k: an end fight past 900 is tougher,
// not bigger). The squad stands while a horde runs at it (the owner, 2026-11-27); its fire (at most 125 musket balls a
// second, Battle::against) holds back a horde that falls to one ball each, so it never arrives; at three a horde closes
// in about five seconds and its size sets what it costs. 0: the kind's own (the text form leaves it out).
constexpr float kTough = 3.0f;
i32 tough(u8 kind, float k = 1.0f) {
    const i32 base = enemyStats(kind).hp;
    const i32 hp = i32(float(base) * kTough * k + 0.5f);
    return hp > base ? hp : 0;
}

// The squad a level is built around (the owner, 2026-11-27: levels up to 15 minutes): past it the road offers no x2 or x3
// and smaller sums, and the hordes bite harder, so a long road does not snowball to the cap.
float ceilingOf(int n) { return 100.0f + 25.0f * float(n); }

// The text form keeps places in tenths (the speeds in hundredths): every number the generator lays is put on that grid
// before the bot plays it, so what it measures is what the table will hold (a horde's cost can turn on a tenth).
float r10(float f) { return float(int(f * 10.0f + (f >= 0 ? 0.5f : -0.5f))) / 10.0f; }
float r100(float f) { return float(int(f * 100.0f + 0.5f)) / 100.0f; }
void snap(LevelDef& d) {
    d.length = r10(d.length);
    d.speed = r100(d.speed);
    d.pace = r100(d.pace);
    for (int i = 0; i < d.gateCount; ++i) {
        GateDef& g = d.gates[i];
        g.x0 = r10(g.x0);
        g.x1 = r10(g.x1);
        g.z = r10(g.z);
    }
    for (int i = 0; i < d.barrelCount; ++i) d.barrels[i].x = r10(d.barrels[i].x), d.barrels[i].z = r10(d.barrels[i].z);
    for (int i = 0; i < d.medalCount; ++i) d.medals[i].x = r10(d.medals[i].x), d.medals[i].z = r10(d.medals[i].z);
    for (int i = 0; i < d.mineCount; ++i) d.mines[i].x = r10(d.mines[i].x), d.mines[i].z = r10(d.mines[i].z);
    for (int i = 0; i < d.cageCount; ++i) d.cages[i].x = r10(d.cages[i].x), d.cages[i].z = r10(d.cages[i].z);
    for (int i = 0; i < d.rollerCount; ++i) d.rollers[i].x = r10(d.rollers[i].x), d.rollers[i].z = r10(d.rollers[i].z);
    for (int i = 0; i < d.wallCount; ++i) d.walls[i].z = r10(d.walls[i].z);
    for (int i = 0; i < d.stripCount; ++i) {
        StripDef& st = d.strips[i];
        st.z0 = r10(st.z0);
        st.z1 = r10(st.z1);
        st.push = r10(st.push);
    }
    for (int i = 0; i < d.waveCount; ++i) {
        WaveDef& w = d.waves[i];
        w.z = r10(w.z);
        w.x0 = r10(w.x0);
        w.x1 = r10(w.x1);
    }
}

void gate(LevelDef& d, float x0, float x1, float z, i32 v, u8 kind, u8 grows, i32 cap) {
    if (d.gateCount < kMaxGates) d.gates[d.gateCount++] = {x0, x1, z, v, kind, grows, cap};
}

// The bot's run of the road as the generator lays it (TECH_SPEC 4). It is kept 64 units behind the road's end, so
// nothing laid later can change what it did there (waves wake 62 units ahead, the bot looks at most 42 ahead); a
// measurement copies it and plays only the new stretch, exactly as a run of the road so far would.
Battle& runner() {
    thread_local Battle* b = nullptr;
    if (!b) b = new Battle();
    return *b;
}
Battle& scratch() {
    thread_local Battle* b = nullptr;
    if (!b) b = new Battle();
    return *b;
}

// The squad the bot has at `zEnd` on the road built so far: what the next section is sized from. 0 if it does not get
// there.
float squadAt(float zEnd) {
    Battle& b = scratch();
    b = runner();
    return playTo(b, zEnd) ? float(b.squad) : 0.0f;
}

// What the waves from `firstWave` on cost a weaker player (soldiers their enemies took on reaching it), played to zEnd;
// -1 if it does not get there. The bot plays to `zMeet` (just before the horde wakes), then keeps `weak` of its
// soldiers. Only their own toll: how the bot steers around them changes what else it gains or loses, which is not theirs.
float hordeCost(float zEnd, int firstWave, float zMeet, float weak) {
    Battle& b = scratch();
    b = runner();
    if (b.z < zMeet && !playTo(b, zMeet)) return -1.0f;
    b.squad = maxv<i32>(1, i32(float(b.squad) * weak + 0.5f));
    if (!playTo(b, zEnd)) return -1.0f;
    i32 sum = 0;
    for (int w = maxv(0, firstWave); w < b.def->waveCount && w < kMaxWaves; ++w) sum += b.waveLost[w];
    return float(sum);
}

// The weapons in the order they come in, and the levels that bring them (docs/CONTENT_PASS.md 4.2).
const u8 kWeaponOrder[9] = {kTwin, kScatter, kLance, kRockets, kFrost, kMortar, kFlame, kCannon, kCoil};
const int kWeaponIntro[9] = {9, 14, 22, 28, 32, 36, 41, 48, 53};
int weaponsKnown(int n) {
    int k = 0;
    while (k < 9 && n >= kWeaponIntro[k]) ++k;
    return k;
}

// The power-ups in the order they come in, and the levels that bring them (docs/CONTENT_PASS.md 4.2); which can be a
// medallion (the others come in supply crates only).
const u8 kPowerOrder[12] = {kRapid, kShield, kAirstrike, kPowerShot, kFreeze, kQuick, kMagnet, kReinforce, kGiants, kHourglass, kDouble, kFieldGun};
const int kPowerIntro[12] = {11, 18, 23, 29, 32, 38, 44, 46, 49, 54, 58, 64};
bool medalPower(u8 p) {
    return p == kRapid || p == kPowerShot || p == kShield || p == kMagnet || p == kGiants || p == kQuick || p == kHourglass || p == kDouble;
}
bool cratePower(u8 p) { return !medalPower(p) || p == kRapid || p == kPowerShot || p == kHourglass; }

}  // namespace

// Each block of ten (docs/CONTENT_PASS.md 4.1): 1 march, 2 arsenal, 3 rescue, 4 gauntlet, 5 gold rush, 6 march, 7 siege,
// 8 rush, 9 giants' road, 10 the boss; each mode from the level that brings it in (a march before); a hard level marches.
u8 levelMode(int n) {
    const int slot = (n - 1) % 10 + 1;
    if (slot == 10) return kModeBoss;
    if (isSpike(n)) return kModeMarch;
    if (slot == 5 && n >= 15) return kModeGold;
    if (n == 33 || (slot == 7 && n >= 37)) return kModeSiege;
    if (slot == 2 && n >= 22) return kModeArsenal;
    if (slot == 3 && n >= 34) return kModeRescue;  // rescue moves here once the gauntlet takes slot 4
    if (slot == 4) return n >= 34 ? kModeGauntlet : n >= 24 ? kModeRescue : kModeMarch;
    if (slot == 8 && n >= 38) return kModeRush;
    if (slot == 9 && n >= 49) return kModeGiants;
    return kModeMarch;
}

int weaponOffer(int n) {
    int prev = 0;
    for (int k = 9; k <= n; ++k) {
        int w = 0;
        for (int i = 0; i < 9; ++i)
            if (k == kWeaponIntro[i]) w = kWeaponOrder[i];  // its introduction
        if (w == 0 && hash64(u64(k) * 31337ULL + 5ULL) % 10ULL < 7ULL) {
            const int m = weaponsKnown(k);
            int i = int(hash64(u64(k) * 7777ULL + 3ULL) % u64(m));
            if (kWeaponOrder[i] == prev) i = m > 1 ? (i + 1) % m : -1;
            w = i >= 0 ? kWeaponOrder[i] : 0;
        }
        if (k == n) return w;
        prev = w;
    }
    return 0;
}

float levelSeconds(int n) { return 30.0f * std::pow(30.0f, float(clampv(n, 1, kLevels) - 1) / float(kLevels - 1)); }

bool isSpike(int n) {
    auto hard = [](int k) { return k >= 15 && hash64(u64(k) * 7919ULL + 17ULL) % 9ULL == 0; };
    return hard(n) && !hard(n - 1);
}

float marginTarget(int n) {
    float t = 0.02f + 0.6f * std::exp(-float(n) / 22.0f);  // about 60% early, under 3% by the last levels
    if (n % 10 == 0) t *= 0.8f;          // a boss's level, a little harder
    if (isSpike(n)) t = maxv(0.012f, t * 0.3f);
    return t;
}

// The share of its squad each horde on the way costs the bot: a nibble early on, a third late, more on the hard levels.
float bite(int n) {
    const float b = 0.05f + 0.3f * minv(1.0f, float(n - 1) / 66.0f);
    return isSpike(n) ? b * 1.4f : b;
}

void build(int n, u64 seed, Shape& out) {
    Rng r(seed, u64(n) * 2 + 1);
    out = Shape{};
    out.hordeMax = n <= 3 ? 20 : 900;
    LevelDef& d = out.d;
    d.seed = u32(seed);
    d.world = u8((n - 1) / 10 % 10 + 1);
    d.boss = n % 10 == 0;
    // Faster as the game goes on: the march from 4.6 to 7.0 units a second, the horde's pace up to 1.35 times; a hard
    // level faster still. Each level is longer than the one before, from half a minute to about fifteen at level 100
    // (the owner, 2026-11-27): the road is the time at the march's speed, less the third spent standing for hordes.
    const float late = float(n - 1) / float(kLevels - 1);
    d.speed = (4.6f + 2.4f * late) * (isSpike(n) ? 1.08f : 1.0f) * (levelMode(n) == kModeRush ? 1.5f : 1.0f);
    d.pace = (1.0f + 0.35f * late) * (isSpike(n) ? 1.1f : 1.0f);
    d.length = levelSeconds(n) * d.speed * (levelMode(n) == kModeRush ? 0.4f : 0.68f) + (d.boss ? 24.0f : 0.0f);  // a rush is short
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
        case 9: d.hint = 12; break;   // the first weapon gate
        case 14: d.hint = 13; break;  // the first weapon crate
        case 22: d.hint = 14; break;
        case 28: d.hint = 15; break;
        case 11: d.hint = 16; break;  // the first medallion (rapid fire)
        case 18: d.hint = 17; break;
        case 23: d.hint = 18; break;  // the first supply crate (the airstrike)
        case 29: d.hint = 19; break;
        case 32: d.hint = 20; break;
        case 13: d.hint = 21; break;  // mines
        case 17: d.hint = 23; break;  // archers
        case 19: d.hint = 24; break;  // moving gates
        case 26: d.hint = 33; break;  // shield bearers
        case 36: d.hint = 34; break;  // the mortar
        case 37: d.hint = 35; break;  // bombers
        case 38: d.hint = 36; break;  // quick march
        case 41: d.hint = 37; break;  // the flamethrower
        case 42: d.hint = 38; break;  // drummers
        case 44: d.hint = 39; break;  // the magnet
        case 46: d.hint = 40; break;  // reinforcements
        case 48: d.hint = 41; break;  // the cannon
        case 49: d.hint = 42; break;  // giants (the power-up)
        case 53: d.hint = 43; break;  // the storm coil
        case 54: d.hint = 44; break;  // the hourglass
        case 58: d.hint = 45; break;  // double gates
        case 64: d.hint = 46; break;  // the field gun
        default: break;
    }
    d.mode = levelMode(n);
    const bool gold = d.mode == kModeGold, rescue = d.mode == kModeRescue, arsenal = d.mode == kModeArsenal;
    if (gold) d.hint = 22;
    if (rescue) {
        d.hint = 25;
        d.start = 3;
    }
    if (arsenal) d.hint = 26;
    const bool siege = d.mode == kModeSiege, gauntlet = d.mode == kModeGauntlet, rush = d.mode == kModeRush, giantsRoad = d.mode == kModeGiants;
    if (siege) d.hint = 48;
    if (gauntlet) d.hint = 49;
    if (rush) d.hint = 50;
    if (giantsRoad) d.hint = 51;
    const bool rollers = n >= 27;  // boulders down marked lanes, from 27
    if (n == 27) d.hint = 47;
    // Step 7 (docs/CONTENT_PASS.md 4.2): ice from 31, flip gates from 39, mystery gates from 56, wind from 62; fog from 67
    // and night from 81 now and then (the view only). An introduction's hint beats the mode's.
    const bool ice = n >= 31, flips = n >= 39, mysteries = n >= 56, wind = n >= 62;
    if (n == 31) d.hint = 52;
    if (n == 39) d.hint = 53;
    if (n == 56) d.hint = 54;
    if (n == 62) d.hint = 55;
    {
        const u64 h = hash64(u64(n) * 2654435761ULL + 99ULL) % 5ULL;
        if (n == 67 || (n > 67 && h == 0)) d.sky = kSkyFog;
        if (n == 81 || (n > 81 && h == 1)) d.sky = kSkyNight;
        if (n == 67) d.hint = 56;
        if (n == 81) d.hint = 57;
    }
    // What this level knows (docs/CONTENT_PASS.md 4.2): mines from 13, archers from 17, moving gates from 19, catapults
    // from 21, cages from 24, shield bearers from 26, giants from 29, bombers from 37, drummers from 42; each is brought
    // in on its first level.
    const bool mines = n >= 13, archers = n >= 17, moving = n >= 19, catapults = n >= 21, cages = n >= 24, giants = n >= 29;
    const bool shieldmen = n >= 26, bombers = n >= 37, drummers = n >= 42;
    if (n == 21) d.hint = 27;
    if (n == 29) d.hint = 28;
    // The boss's attacks: the world's own (docs/CONTENT_PASS.md 4.3), each once it has come in (the club on 30, boulders on
    // 50, the war cry on 60, the stomp on 70); worlds 9 and 10 pick two of all of them.
    {
        static const u8 kWorldAttacks[10] = {kAttackSlam, kAttackToss, kAttackSlam | kAttackStomp, kAttackCry | kAttackSlam,
                                             kAttackToss | kAttackStomp, kAttackCry, kAttackStomp | kAttackCry, kAttackToss | kAttackSlam,
                                             15, 15};
        u8 known = 0;
        if (n >= 30) known |= kAttackSlam;
        if (n >= 50) known |= kAttackToss;
        if (n >= 60) known |= kAttackCry;
        if (n >= 70) known |= kAttackStomp;
        u8 a = u8(kWorldAttacks[d.world - 1] & known);
        if (d.world >= 9) {  // two of those known, at random
            u8 pick = 0;
            for (int tries = 0; tries < 8 && __builtin_popcount(pick) < 2 && a; ++tries) pick = u8(pick | (1 << (r.next() % 4u)));
            a = u8(pick & known);
        }
        d.bossAttacks = a;
        if (n == 30) d.hint = 29;
        if (n == 50) d.hint = 30;
        if (n == 60) d.hint = 31;
        if (n == 70) d.hint = 32;
    }
    // Power-ups: the one this level brings in (on its first horde), else now and then one of those known, before a horde.
    int powersKnown = 0, introPower = 0;
    for (int k = 0; k < 12; ++k) {
        if (n >= kPowerIntro[k]) powersKnown = k + 1;
        if (n == kPowerIntro[k]) introPower = kPowerOrder[k];
    }
    int pickups = 0, minefields = 0, archerRanks = 0, shieldLines = 0, bomberRuns = 0, drumBands = 0;
    // A weapon on offer (the same for every seed of this level): a crate now and then once crates are known, else a gate
    // beside a sum or, with two weapons known, beside another weapon; the weapons' introductions stand alone.
    bool introWeapon = false;
    for (int i = 0; i < 9; ++i) introWeapon = introWeapon || n == kWeaponIntro[i];
    int offer = weaponOffer(n);
    if (arsenal) {  // the weapon is in hand from the start; nothing more on offer
        d.gun = u8(offer > 0 ? offer : kWeaponOrder[r.next() % u32(weaponsKnown(n))]);
        offer = 0;
    }
    if (gold) offer = 0;
    const bool crate = offer > 0 && (n == 14 || (n > 14 && !introWeapon && r.f01() < 0.4f));
    int rival = 0;
    if (offer > 0 && !crate && weaponsKnown(n) >= 2 && !introWeapon && r.f01() < 0.3f) {
        rival = kWeaponOrder[r.next() % u32(weaponsKnown(n))];
        if (rival == offer) rival = kWeaponOrder[(r.next() % u32(weaponsKnown(n)))];
        if (rival == offer) rival = 0;
    }
    const bool pairs = n >= 2, barrels = n >= 4, mul = n >= 5, blocks = n >= 6, riders = n >= 7, brutes = n >= 12, triple = n >= 20;
    const bool bossHere = !gold && (d.boss || n == 8 || isSpike(n) || (n > 8 && r.f01() < (n > 40 ? 0.6f : 0.45f)));
    if (!bossHere) d.bossAttacks = 0;
    int catapultsLaid = 0, giantsLaid = 0;
    // E: the squad the bot really has where the next section starts, measured by playing the road built so far, so
    // every section is sized against it (gates worth taking, barrels worth shooting, hordes that bite). The bot's run of
    // it (runner) starts here and follows the road as it is laid.
    float E = float(d.start);
    snap(d);
    runner().init(d);
    runner().clearWins = false;  // the road is not all laid: no end yet
    float z = 14.0f;
    const float end = d.length - 24.0f;
    int sinceWave = 0;
    int section = 0, last = -1;
    while (z < end) {
        const int gatesBefore = d.gateCount, barrelsBefore = d.barrelCount, wavesBefore = d.waveCount, medalsBefore = d.medalCount,
                  minesBefore = d.mineCount, cagesBefore = d.cageCount, rollersBefore = d.rollerCount, wallsBefore = d.wallCount,
                  stripsBefore = d.stripCount;
        // Above the level's ceiling: gains shrink by `over`, multipliers are left out.
        const float over = maxv(1.0f, E / ceilingOf(n));
        const bool grow = over <= 1.0f;
        // What comes next: a horde after every two or three other sections (never on a gold rush); else a mix of what
        // this level knows, never the same twice running. The first section is always gates.
        int kind;
        if (section == 0) kind = n < 10 || r.f01() < 0.5f ? 0 : 1;
        else if (section == 1 && n == 27) kind = 9;  // the first rollers (the level's weapon comes next)
        else if (section == 1 && (n == 31 || n == 62)) kind = 13;  // the first ice, the first wind
        else if (section == 1 && n == 39) kind = 11;                 // the first flip gate
        else if (section == 1 && n == 56) kind = 12;                 // the first mystery gates
        else if (section == (n == 27 || n == 31 || n == 39 || n == 56 || n == 62 ? 2 : 1) && offer > 0) kind = 6;
        else if (section == 1 && n == 19) kind = 7;  // the first moving gate
        // A horde after every two or three other sections: never on a gold rush, a rush or a gauntlet (their roads are
        // hazards and gates; the end fight is their only horde).
        else if (!gold && !rush && !gauntlet && sinceWave >= 2 + int(r.next() % 2u)) kind = 4;
        else {
            // Gold rush: gates, barrels, medallions; rescue: few gates; gauntlet: rollers and minefields, gates as bait.
            const float gk = rescue ? 0.4f : gauntlet ? 0.5f : 1.0f;
            float w[14] = {(n <= 3 ? 3.0f : 0.9f) * gk, pairs ? 2.2f * gk : 0.0f, barrels ? 1.7f : 0.0f, blocks ? 1.2f * gk : 0.0f, 0.0f,
                           triple && grow ? 1.1f * gk : 0.0f, 0.0f, moving ? (gold ? 1.6f : 0.8f) : 0.0f, gold && powersKnown > 0 ? 1.2f : 0.0f,
                           rollers && !gold ? (gauntlet ? 3.0f : rush ? 1.5f : 0.6f) : 0.0f, gauntlet || (rush && mines) ? 1.4f : 0.0f,
                           flips ? 0.8f * gk : 0.0f, mysteries ? 0.7f * gk : 0.0f, ice || wind ? 0.7f : 0.0f};
            if (last >= 0 && last < 14) w[last] *= 0.12f;
            float sum = 0;
            for (float v : w) sum += v;
            float x = r.f01() * sum;
            kind = 0;
            for (int k = 0; k < 14; ++k) {
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
                i32 a = friendly(maxv(2.0f, E * uni(r, 0.25f, 0.9f) / over)), b = friendly(maxv(2.0f, E * uni(r, 0.2f, 0.7f) / over));
                u8 ka = kGateAdd, kb = kGateAdd;
                const float y = r.f01();
                if (mul && grow && y < 0.4f) {  // x2 against a fair sum
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
                    const i32 reward = friendly(maxv(4.0f, E * uni(r, 0.25f, 0.6f) / over));
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
            case 6: {  // the level's weapon on offer
                const float zx = z + 4.0f;
                if (crate) {
                    const i32 hp = maxv<i32>(4, i32(dps(E) * uni(r, 0.6f, 1.2f)));
                    if (d.barrelCount < kMaxBarrels) d.barrels[d.barrelCount++] = {uni(r, -2.6f, 2.6f), zx + 2.0f, hp, 0, u8(offer)};
                    z += 14.0f;
                } else if (introWeapon) {  // its introduction: alone in the middle of the road
                    gate(d, -2.2f, 2.2f, zx, offer, kGateWeapon, 0, 0);
                    z += 12.0f;
                } else {
                    const bool left = r.f01() < 0.5f;
                    gate(d, left ? -4.6f : 0.25f, left ? -0.25f : 4.6f, zx, offer, kGateWeapon, 0, 0);
                    if (rival > 0) {
                        gate(d, left ? 0.25f : -4.6f, left ? 4.6f : -0.25f, zx, rival, kGateWeapon, 0, 0);
                    } else {  // a modest sum: the weapon should be worth giving it up
                        const i32 a = friendly(maxv(2.0f, E * uni(r, 0.15f, 0.45f)));
                        gate(d, left ? 0.25f : -4.6f, left ? 4.6f : -0.25f, zx, a, kGateAdd, 1, a + 2 + a / 2);
                    }
                    z += 12.0f;
                }
                ++sinceWave;
                break;
            }
            case 7: {  // a gate on the move (and on a gold rush often a second behind it): worth catching
                const int k = gold ? 2 : 1;
                for (int i = 0; i < k; ++i) {
                    const float gx = uni(r, -2.0f, 2.0f);
                    const i32 a = friendly(maxv(3.0f, E * uni(r, 0.4f, 0.9f) / over));
                    gate(d, gx - 1.3f, gx + 1.3f, z + 4.0f + 7.0f * float(i), a, kGateAdd, 1, a + 2 + a / 2);
                    d.gates[d.gateCount - 1].moves = u8(1 + r.next() % 255u);
                }
                z += 8.0f + 7.0f * float(k);
                ++sinceWave;
                break;
            }
            case 9: {  // rollers: one to three boulders waiting down different lanes
                const int k = n == 27 && section <= 2 ? 1 : 1 + int(r.next() % 3u);
                int lane = int(r.next() % 3u);
                for (int i = 0; i < k && d.rollerCount < kMaxRollers; ++i) {
                    d.rollers[d.rollerCount++] = {-2.6f + 2.6f * float(lane), z + 10.0f + 9.0f * float(i)};
                    lane = (lane + 1 + int(r.next() % 2u)) % 3;
                }
                z += 16.0f + 9.0f * float(k);
                ++sinceWave;
                break;
            }
            case 10: {  // a minefield across the road
                const int k = 5 + int(r.next() % 5u);
                for (int i = 0; i < k && d.mineCount < kMaxMines; ++i) d.mines[d.mineCount++] = {uni(r, -4.2f, 4.2f), z + uni(r, 4.0f, 14.0f)};
                z += 18.0f;
                ++sinceWave;
                break;
            }
            case 11: {  // a flip gate over half the road: a fair sum and a cost by turns, 1.5 s each (pass beside it)
                const bool left = r.f01() < 0.5f;
                const i32 a = friendly(maxv(3.0f, E * uni(r, 0.4f, 0.9f) / over)), c = friendly(maxv(2.0f, E * uni(r, 0.15f, 0.4f)));
                gate(d, left ? -4.6f : 0.25f, left ? -0.25f : 4.6f, z + 6.0f, a, kGateAdd, 0, a);
                d.gates[d.gateCount - 1].trick = 1;
                d.gates[d.gateCount - 1].alt = -c;
                z += 14.0f;
                ++sinceWave;
                break;
            }
            case 12: {  // two mystery gates: each shows the same two outcomes; one holds the sum, the other the cost
                const i32 a = friendly(maxv(3.0f, E * uni(r, 0.4f, 0.8f) / over)), c = friendly(maxv(2.0f, E * uni(r, 0.2f, 0.4f)));
                const bool goodLeft = r.f01() < 0.5f;
                for (int side = 0; side < 2; ++side) {
                    const bool good = (side == 0) == goodLeft;
                    gate(d, side == 0 ? -4.6f : 0.25f, side == 0 ? -0.25f : 4.6f, z + 6.0f, good ? a : -c, kGateAdd, 1, good ? a + a / 2 : c / 3);
                    d.gates[d.gateCount - 1].trick = 2;
                    d.gates[d.gateCount - 1].alt = good ? -c : a;
                }
                z += 14.0f;
                ++sinceWave;
                break;
            }
            case 13: {  // weather over the road (ice, or wind pushing one way) with a pair of gates in it
                const bool windy = (n == 62 && d.stripCount == 0) || (wind && (!ice || r.f01() < 0.5f));  // 62 brings the wind in
                if (d.stripCount < kMaxStrips)
                    d.strips[d.stripCount++] = {z, z + 22.0f, u8(windy ? kStripWind : kStripIce), windy ? (r.f01() < 0.5f ? -1.0f : 1.0f) * uni(r, 2.5f, 3.5f) : 0.0f};
                const bool left = r.f01() < 0.5f;
                const i32 a = friendly(maxv(3.0f, E * uni(r, 0.3f, 0.8f) / over)), c = friendly(maxv(2.0f, E * uni(r, 0.15f, 0.4f)));
                gate(d, left ? -4.6f : 0.25f, left ? -0.25f : 4.6f, z + 12.0f, a, kGateAdd, 1, a + 2 + a / 2);
                gate(d, left ? 0.25f : -4.6f, left ? 4.6f : -0.25f, z + 12.0f, -c, kGateAdd, 1, maxv<i32>(1, c / 3));
                z += 26.0f;
                ++sinceWave;
                break;
            }
            case 8: {  // a gold rush's medallions, one or two across the road
                const int k = 1 + int(r.next() % 2u);
                for (int i = 0; i < k && d.medalCount < kMaxMedals; ++i) {
                    u8 p = kPowerOrder[r.next() % u32(powersKnown)];
                    if (!medalPower(p)) p = kRapid;
                    d.medals[d.medalCount++] = {k == 1 ? uni(r, -2.5f, 2.5f) : (i == 0 ? -2.6f : 2.6f), z + 4.0f, p};
                }
                z += 10.0f;
                ++sinceWave;
                break;
            }
            default: {  // a horde, as big as it must be to cost the bot about `bite` of its squad (measured)
                // Mines in front of it (the horde walks over them: shoot them then); on 13, the first horde's alone.
                if (mines && (n == 13 ? minefields == 0 : r.f01() < 0.3f)) {
                    const int k = 3 + int(r.next() % 4u);
                    for (int i = 0; i < k && d.mineCount < kMaxMines; ++i)
                        d.mines[d.mineCount++] = {uni(r, -4.2f, 4.2f), z + uni(r, 3.0f, 9.0f)};
                    ++minefields;
                }
                // Prisoners in a cage among it: every horde on a rescue, now and then elsewhere once cages are known.
                if ((rescue || (cages && r.f01() < 0.2f)) && d.cageCount < kMaxCages)
                    d.cages[d.cageCount++] = {uni(r, -3.0f, 3.0f), z + 12.0f,
                                              friendly(maxv(rescue ? 10.0f : 5.0f, E * (rescue ? uni(r, 0.8f, 1.6f) : uni(r, 0.3f, 0.6f)) / over))};
                // A power-up on the way in: the level's new one on its first horde, else now and then one already known.
                if (powersKnown > 0 && pickups < 2 && (introPower > 0 ? pickups == 0 : r.f01() < 0.45f)) {
                    const u8 p = introPower > 0 ? u8(introPower) : kPowerOrder[r.next() % u32(powersKnown)];
                    const float px = uni(r, -3.0f, 3.0f), pz = z + 3.0f;
                    if (medalPower(p) && (!cratePower(p) || p == introPower || r.f01() < 0.65f)) {
                        if (d.medalCount < kMaxMedals) d.medals[d.medalCount++] = {px, pz, p};
                    } else if (d.barrelCount < kMaxBarrels) {
                        d.barrels[d.barrelCount++] = {px, pz, maxv<i32>(4, i32(dps(E) * uni(r, 0.3f, 0.7f))), 0, 0, p};
                    }
                    ++pickups;
                }
                const bool full = r.f01() < 0.6f;
                const float cx = full ? 0.0f : (r.f01() < 0.5f ? -2.2f : 2.2f);
                const float half = full ? 4.3f : 2.4f;
                const bool withRiders = riders && r.f01() < 0.4f, withBrutes = brutes && r.f01() < 0.35f;
                const bool withArchers = archers && (siege || (n == 17 ? archerRanks == 0 : r.f01() < 0.3f));
                if (withArchers) ++archerRanks;
                // A catapult at the road's side beyond it; a giant at its head (with three seconds of the squad's fire in
                // it), each brought in on its own level.
                const bool withCatapult = catapults && (n == 21 ? catapultsLaid == 0 : r.f01() < 0.25f);
                const bool withGiant = !gold && giants && (giantsRoad || (n == 29 ? giantsLaid == 0 : r.f01() < 0.18f));
                if (withCatapult) ++catapultsLaid;
                if (withGiant) ++giantsLaid;
                const float catX = r.f01() < 0.5f ? -3.6f : 3.6f;
                const i32 giantHp = maxv<i32>(30, i32(dps(E) * 3.0f));
                // Shield bearers in its front rank, bombers among it, a drummer or two in it, each brought in on its own
                // level.
                const bool withShields = shieldmen && (n == 26 ? shieldLines == 0 : r.f01() < 0.25f);
                const bool withBombers = bombers && (n == 37 ? bomberRuns == 0 : r.f01() < 0.25f);
                const bool withDrums = drummers && (n == 42 ? drumBands == 0 : r.f01() < 0.2f);
                if (withShields) ++shieldLines;
                if (withBombers) ++bomberRuns;
                if (withDrums) ++drumBands;
                const float zw = z;
                // A siege: a fort wall before the horde (it waits behind it), archers on the ramparts; the wall holds about
                // four to seven seconds of the squad's fire.
                if (siege && d.wallCount < kMaxWalls) d.walls[d.wallCount++] = {zw + 4.0f, maxv<i32>(20, i32(dps(E) * uni(r, 4.0f, 7.0f)))};
                const float archerZ = siege ? zw + 12.0f : zw + 21.0f;
                // This section's waves for a horde of `count` foot (and those that come with them).
                auto lay = [&](float count) {
                    d.waveCount = wavesBefore;
                    const i32 foot = clampv<i32>(i32(count), 3, out.hordeMax);
                    const float k = 1.0f;
                    d.waves[d.waveCount++] = {zw + 10.0f, cx - half, cx + half, foot, kFoot, tough(kFoot, k)};
                    if (withRiders) d.waves[d.waveCount++] = {zw + 14.0f, -4.0f, 4.0f, minv(maxv<i32>(3, foot / 4), 300), kRider, tough(kRider, k)};
                    if (withBrutes) d.waves[d.waveCount++] = {zw + 18.0f, -3.0f, 3.0f, minv(maxv<i32>(1, foot / 12), 320), kBrute, tough(kBrute, k)};
                    // Archers behind it all, standing in their ranks.
                    if (withArchers)
                        d.waves[d.waveCount++] = {archerZ, -4.2f, 4.2f, minv(maxv<i32>(3, foot / 5), 80), kArcher, tough(kArcher)};
                    if (withCatapult) d.waves[d.waveCount++] = {zw + 26.0f, catX, catX, 1, kCatapult};
                    if (withGiant) d.waves[d.waveCount++] = {zw + 9.0f, -0.6f, 0.6f, 1, kGiant, giantHp};
                    if (withGiant && giantsRoad) d.waves[d.waveCount++] = {zw + 16.0f, -2.6f, 2.6f, 2, kGiant, giantHp};  // two more
                    if (withShields)
                        d.waves[d.waveCount++] = {zw + 8.5f, cx - half, cx + half, minv(maxv<i32>(3, foot / 8), 60), kShieldman, tough(kShieldman, k)};
                    if (withBombers)
                        d.waves[d.waveCount++] = {zw + 12.0f, cx - half, cx + half, minv(maxv<i32>(2, foot / 15), 40), kBomber, tough(kBomber, k)};
                    if (withDrums)
                        d.waves[d.waveCount++] = {zw + 13.0f, cx - 1.0f, cx + 1.0f, minv(maxv<i32>(1, foot / 60), 3), kDrummer, tough(kDrummer, k)};
                };
                // Its cost: the soldiers its own enemies take (played to past its end).
                // Above the ceiling it bites harder (up to twice), never more than 70% of the squad.
                // Sized against a player with fewer soldiers than the bot (half on level 4, 85% on the last), who must
                // still get through: the end fight, not the hordes on the way, sets how close the level runs. On the first
                // three, against the squad the level starts with and two more: whoever steers anywhere gets through.
                const float weak = 0.5f + 0.35f * late;
                const float goal = weak * E * minv(0.7f, bite(n) * uni(r, 0.6f, 1.4f) * minv(over, 2.0f));
                float lo = 2.0f, hi = float(out.hordeMax), pick = lo;
                static const bool trace = std::getenv("PM_GEN_TRACE") != nullptr;
                bool met = false;  // a size whose cost is near the goal; else the goal is on a cliff (none, then all)
                for (int it = 0; it < 8 && d.waveCount + 9 <= kMaxWaves; ++it) {
                    const float mid = std::sqrt(lo * hi);
                    lay(mid);
                    snap(d);
                    const float cost = hordeCost(zw + 47.0f, wavesBefore, zw - 54.0f, weak);
                    if (trace)
                        std::fprintf(stderr, "  horde at %.0f: squad %.0f wants -%.0f; %.0f foot cost %.0f\n", double(zw), double(E), double(goal),
                                     double(mid), double(cost));
                    if (cost < 0.0f || cost > goal * 1.2f) {
                        hi = mid;
                    } else {
                        lo = pick = mid;
                        met = cost >= goal * 0.8f;
                        if (met) break;
                    }
                }
                // On a cliff the size that cost nothing sits just short of one that wipes the squad out: a little smaller,
                // so a run that came another way (the planner's, a player's) is not wiped out by it.
                if (d.waveCount + 9 <= kMaxWaves) lay(met ? pick : maxv(2.0f, pick * 0.85f));
                z += withArchers ? 26.0f : 22.0f;
                sinceWave = 0;
                break;
            }
        }
        // What the bot has after this section (a horde fought to its end); a section it cannot get through is dropped.
        snap(d);
        const float after = squadAt(z + (d.waveCount > wavesBefore ? 25.0f : 2.0f));
        if (after >= 1.0f) {
            E = after;
            playTo(runner(), z - 64.0f);  // the run follows, 64 units behind the road's new end
        } else {
            d.gateCount = gatesBefore;
            d.barrelCount = barrelsBefore;
            d.waveCount = wavesBefore;
            d.medalCount = medalsBefore;
            d.mineCount = minesBefore;
            d.cageCount = cagesBefore;
            d.rollerCount = rollersBefore;
            d.wallCount = wallsBefore;
            d.stripCount = stripsBefore;
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
    if (d.mode == kModeGold) {
        // A bonus road ends at its finish line: nothing to size.
    } else if (s.boss) {  // as many hit points as the squad at the end deals in m seconds (it crushes a ninth of whatever
                          // squad meets it every second: Battle::moveEnemies)
        d.bossHp = maxv<i32>(20, i32(dps(s.endSquad) * m));
    } else if (d.waveCount + 3 <= kMaxWaves) {  // m times the squad's number, foot and riders and brutes (tougher past 900)
        // 50 beyond the road's last section: it wakes (62 ahead) only once the squad is past everything else, runs at it,
        // and the level is won when it is beaten (Battle::tick); the finish line stays beyond it.
        const float E = s.endSquad, count = minv(maxv(5.0f, E * m), float(s.hordeMax) * 4.0f), k = maxv(1.0f, count / 900.0f);
        const float z = s.lastZ + 50.0f;
        const i32 foot = minv(i32(count), s.hordeMax);
        d.waves[d.waveCount++] = {z, -4.3f, 4.3f, foot, kFoot, tough(kFoot, k)};
        if (s.lastRiders) d.waves[d.waveCount++] = {z + 5.0f, -4.0f, 4.0f, minv(maxv<i32>(3, foot / 4), 300), kRider, tough(kRider, k)};
        if (s.lastBrutes) d.waves[d.waveCount++] = {z + 9.0f, -3.5f, 3.5f, minv(maxv<i32>(1, foot / 10), 320), kBrute, tough(kBrute, k)};
        d.length = maxv(d.length, z + 26.0f);
    }
    // Through the text form, so the bot plays exactly what the table will hold.
    static thread_local char buf[262144];  // a 15-minute level's road runs to tens of thousands of characters
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
    // falls from about 60% to under 3% by level 100; the hard levels thrown in ask for a third of their neighbours'.
    const float target = marginTarget(n);
    const bool gold = levelMode(n) == kModeGold;  // a bonus road has no end fight: any win will do
    const float lo = gold ? 0.0f : (n <= 10 ? 0.45f : target * 0.6f);
    // The first ten keep half their squad or more, but their end fight always costs something.
    const float hi = gold ? 1.0f : n <= 10 ? 0.9f : minv(1.0f, target * 1.5f);
    best.target = target;
    float bestErr = 1e9f;
    thread_local Shape* sp = nullptr;  // large: one per thread
    if (!sp) sp = new Shape();
    thread_local LevelDef* dp = nullptr;
    if (!dp) dp = new LevelDef();
    thread_local LevelDef* pp = nullptr;
    if (!pp) pp = new LevelDef();
    thread_local Battle* snapP = nullptr;
    if (!snapP) snapP = new Battle();
    thread_local Battle* endP = nullptr;
    if (!endP) endP = new Battle();
    for (int s = 0; s < maxSeeds; ++s) {
        const u64 seed = hash64(u64(n) * 1000003ULL + u64(s));
        // The road once, every horde sized against the bot's own run of it: that run is the level's proof (a planned run
        // that takes other gates or another weapon meets hordes sized for a different squad, and on a long road one of
        // them is the end of it).
        build(n, seed, *sp);
        if (sp->endSquad < 1.0f) continue;
        LevelDef& p = *pp;
        finish(*sp, 1.0f, p);
        p.waveCount = sp->d.waveCount;  // the end fight's waves come last
        p.bossHp = 0;
        p.spike = isSpike(n) ? 1 : 0;
        // That run to 64 units before the end fight (its waves wake 62 ahead, the boss 36 past that): every end fight's
        // run is that same run up to there, so each strength tried replays only the end.
        Battle& snap = *snapP;
        snap.init(p);
        snap.clearWins = false;  // p holds no end fight yet
        const bool reached = playTo(snap, sp->lastZ - 64.0f) && !snap.won;
        static const bool trace = std::getenv("PM_BAKE_TRACE") != nullptr;
        if (trace)
            std::fprintf(stderr, "bake %d seed %d: road %.0f, the road's run ends with %.0f; the level's run %s at %.0f with %d (peak %d, %.0f s)\n", n, s,
                         double(sp->lastZ), double(sp->endSquad), reached ? "gets there" : "falls", double(snap.z), int(snap.squad), int(snap.peak),
                         double(snap.time));
        if (!reached) continue;
        float mlo = 0.05f, mhi = 90.0f;
        for (int it = 0; it < 12; ++it) {
            const float m = std::sqrt(mlo * mhi);
            LevelDef& d = *dp;
            finish(*sp, m, d);
            d.spike = p.spike;
            Battle& e = *endP;
            e = snap;
            e.def = &d;
            e.clearWins = true;
            playTo(e, d.length + 1.0f);  // to the win (or the fall): the road ends at d.length
            bool won = e.won;
            i32 left = e.squad, peak = e.peak;
            float secs = e.time;
            ++best.plays;
            if (won && float(left) / float(maxv<i32>(peak, 1)) >= lo) {
                // The whole run, to be sure the table's numbers are what a run from the start gives.
                const PlayResult pr = playLevel(d);
                if (pr.won != won || pr.left != left || pr.peak != peak)
                    std::fprintf(stderr, "bake: level %d seed %d: the replay (%d of %d) and the full run (%d of %d) differ\n", n, s,
                                 int(left), int(peak), int(pr.left), int(pr.peak));
                won = pr.won;
                left = pr.left;
                peak = pr.peak;
                secs = pr.seconds;
            }
            const float ratio = won ? float(left) / float(maxv<i32>(peak, 1)) : -1.0f;
            // In the band, but the road alone took the margin and the end fight is next to nothing (a boss of a few hit
            // points): kept only if no other seed does better.
            if (won && ratio >= lo && ratio <= hi && m < 0.5f && !gold) {
                if (bestErr > 0.001f) {
                    bestErr = 0.001f;
                    best.d = d;
                    best.botEnd = left;
                    best.botPeak = peak;
                    best.m = m;
                }
                break;
            }
            if (won && ratio >= lo && ratio <= hi) {
                best.d = d;
                best.ok = true;
                best.botEnd = left;
                best.botPeak = peak;
                best.m = m;
                best.seconds = secs;
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
        const PlayResult pr = playLevel(best.d);  // the replay's numbers, from the start
        best.botEnd = pr.won ? pr.left : 1;
        best.botPeak = pr.peak;
        best.seconds = pr.seconds;
        best.d.botEnd = best.botEnd;
        best.d.star3 = maxv<i32>(1, i32(std::ceil(float(best.botEnd) * 0.9f)));
        best.d.star2 = maxv<i32>(1, i32(std::ceil(float(best.botEnd) * 0.45f)));
        best.ok = true;
        if (bestErr <= 0.001f) std::snprintf(best.why, sizeof best.why, "a weak end fight");
        else std::snprintf(best.why, sizeof best.why, "band missed by %.2f", double(bestErr));
    } else {
        std::snprintf(best.why, sizeof best.why, "the bot never won");
    }
    return best;
}

}  // namespace pm::gen
