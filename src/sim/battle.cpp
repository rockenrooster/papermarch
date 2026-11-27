// The battle's rules (see battle.h): the squad's march and fire, bullets against gates, barrels and enemies, the hordes'
// charge, the boss.
#include "sim/battle.h"

#include <cmath>
#include <cstring>

namespace pm::sim {

namespace {
const EnemyStats kStats[kEnemyKindCount] = {
    {1, 3.2f, 0.42f, 1},   // foot
    {2, 5.6f, 0.55f, 2},   // rider
    {8, 2.2f, 0.85f, 6},   // brute
    {1, 1.5f, 1.9f, 0},    // boss (its hit points come from the level; it drains the squad while it touches it)
    {1, 0.0f, 0.42f, 1},   // archer: stands where it is and shoots (volleys)
    {40, 0.0f, 1.2f, 0},   // catapult: a timber engine at the road's side (attacks); the squad smashes it in passing
    {1, 1.8f, 1.3f, 25},   // giant: its hit points come from its wave (35% of a boss's); it drops a medallion
    {3, 2.4f, 0.5f, 2},    // shield bearer: behind a shield of 14 (Enemy::shield)
    {2, 6.5f, 0.45f, 8},   // bomber: bursts when it dies (10 musket balls' damage within 2.2; bombers chain)
    {4, 3.2f, 0.5f, 2},    // drummer: whoever is within 5 of it goes 40% faster
};
// docs/CONTENT_PASS.md 1 (rate and damage times the musket's; the twin fires pairs, the scattergun a fan of five).
const WeaponStats kWeapons[kWeaponCount] = {
    {1.0f, 1.0f, 48.0f, 42.0f, 1, 0.0f, 0, 0.0f, 0.0f},      // musket
    {1.0f, 0.6f, 48.0f, 48.0f, 2, 0.5f, 0, 0.0f, 0.0f},      // twin rifles: two streams 0.5 apart
    {0.8f, 0.45f, 14.0f, 36.0f, 5, 0.244f, 0, 0.0f, 0.0f},   // scattergun: a 28 degree fan
    {0.5f, 1.2f, 60.0f, 75.0f, 1, 0.0f, 6, 0.0f, 0.0f},      // long lance: through six
    {0.25f, 3.0f, 45.0f, 22.0f, 1, 0.0f, 0, 1.8f, 1.5f},     // rockets: a burst where they strike
    {1.0f, 0.6f, 9.0f, 0.0f, 0, 0.325f, 0, 0.0f, 0.0f},      // flamethrower: 10 ticks a second on a cone 9 long, 18 degrees
    {0.12f, 8.0f, 50.0f, 26.0f, 1, 0.6f, 255, 0.0f, 0.0f},   // cannon: a ball 1.2 wide through everything on its line
    {0.6f, 0.8f, 22.0f, 0.0f, 0, 1.5f, 4, 3.0f, 0.7f},       // storm coil: its lane 3 wide, 4 jumps of 3 at 0.7 each
    {0.2f, 2.5f, 18.0f, 0.0f, 0, 0.0f, 0, 2.6f, 1.0f},       // mortar: a shell 18 ahead after 1.1 s, a blast of 2.6
    {1.0f, 0.5f, 36.0f, 40.0f, 1, 0.0f, 0, 0.0f, 0.0f},      // frost blaster: what it hits goes at half speed for 2 s
};
// Bullets from the front a shield takes (the lance, flame, coil, mortar, cannon and every blast get past it).
bool fromFront(u8 w) { return w == kMusket || w == kTwin || w == kScatter || w == kRockets || w == kFrost; }
constexpr float kWake = 62.0f;  // a wave wakes when the squad is this far from its front
}  // namespace

const EnemyStats& enemyStats(u8 kind) { return kStats[kind < kEnemyKindCount ? kind : 0]; }
const WeaponStats& weaponStats(u8 kind) { return kWeapons[kind < kWeaponCount ? kind : 0]; }

u32 Battle::rand() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

float Battle::radius() const { return minv(4.4f, 0.55f + 0.30f * std::sqrt(float(maxv<i32>(squad, 1)))); }
void Battle::footprint(float& lo, float& hi) const {
    const float r = radius();
    lo = maxv(-kRoadHalf + 0.3f, x - r);
    hi = minv(kRoadHalf - 0.3f, x + r);
    if (hi < lo + 0.2f) lo = hi = x;
}
float Battle::fireRate() const { return 5.0f + 2.4f * float(minv<i32>(squad, 50)); }
i32 Battle::damage() const { return maxv<i32>(kHp, squad * kHp / 50); }  // one musket ball, more past 50 soldiers
float Battle::progress() const { return def ? clampv(z / maxv(1.0f, def->length - 8.0f), 0.0f, 1.0f) : 0.0f; }

void Battle::init(const LevelDef& d) {
    def = &d;
    squad = peak = d.start;
    weapon = d.gun < kWeaponCount ? d.gun : kMusket;
    x = z = vx = time = 0;
    started = won = lost = halted = false;
    enemyCount = bulletCount = 0;
    gatesInit_ = barrelsInit_ = 0;
    sync();
    nextWave = 0;
    bossDrain = bossRate = 0;
    for (i32& l : waveLost) l = 0;
    for (int p = 0; p < kPowerCount; ++p) powerLeft[p] = powerFull[p] = 0;
    shieldPool = 0;
    for (u8& m : medalTaken) m = 0;
    bombCount = 0;
    for (int m = 0; m < kMaxMines; ++m) {
        mineHp[m] = 3 * kHp;
        mineGone[m] = 0;
    }
    for (u8& c : cageOpen) c = 0;
    arrowCount = 0;
    strikeCount = 0;
    slamT = 2.0f;
    tossT = 1.5f;
    stompT = 3.0f;
    criesDone = 0;
    dropCount = 0;
    for (u8& t : dropTaken) t = 0;
    shellCount = 0;
    flameAcc = fieldGunAcc = magnetT = reinforceLeft = reinforceRate = reinforceAcc = 0;
    drumN_ = 0;
    fireAcc_ = 0;
    rng_ = d.seed * 2654435761u + 12345u;
    if (!rng_) rng_ = 1;
    alive_ = 0;
    boss_ = -1;
}

void Battle::spawnWave(int w, BattleEvents& ev) {
    const WaveDef& wd = def->waves[w];
    const EnemyStats& st = enemyStats(wd.kind);
    const float gap = st.radius * 2.05f;
    const int cols = maxv(1, int((wd.x1 - wd.x0) / gap) + 1);
    for (int k = 0; k < wd.count && enemyCount < kMaxEnemies; ++k) {
        Enemy& e = enemies[enemyCount++];
        const int row = k / cols, col = k % cols;
        // A crowd, not a block: every one a little off its place in the ranks.
        const float jx = (float(rand() & 0xFFFF) / 65535.0f - 0.5f) * gap * 0.5f;
        const float jz = (float(rand() & 0xFFFF) / 65535.0f - 0.5f) * gap * 0.8f;
        e.x = wd.x0 + (float(col) + (row % 2 ? 0.5f : 0.0f)) * gap;
        if (e.x > wd.x1) e.x -= gap;
        e.x = clampv(e.x + jx, -kRoadHalf + 0.3f, kRoadHalf - 0.3f);
        e.z = wd.z + float(row) * gap * 0.9f + jz;
        e.hp = (wd.hp > 0 ? wd.hp : st.hp) * kHp;
        e.incoming = 0;
        e.kind = wd.kind;
        e.alive = 1;
        e.seed = u16(rand());
        e.hit = 99;
        e.cool = float(rand() & 0xFFFF) / 65535.0f * 1.6f;  // archers loose out of step
        e.shield = wd.kind == kShieldman ? e.hp * 14 / st.hp : 0;  // 14 balls of what it takes itself
        e.slow = 0;
        e.wave = i16(w);
        ++alive_;
    }
    ev.wave = w;
}

void Battle::sync() {
    for (; gatesInit_ < def->gateCount; ++gatesInit_) gates[gatesInit_] = {def->gates[gatesInit_].value, 0, 0, 99.0f};
    for (; barrelsInit_ < def->barrelCount; ++barrelsInit_) barrels[barrelsInit_] = {def->barrels[barrelsInit_].hp * kHp, 0, 99.0f};
}

void Battle::gateSpan(int g, float t, float& lo, float& hi) const {
    const GateDef& gd = def->gates[g];
    lo = gd.x0;
    hi = gd.x1;
    if (!gd.moves) return;
    const float half = 0.5f * (hi - lo), c0 = 0.5f * (lo + hi);
    const float c = clampv(c0 + 3.0f * std::sin(6.2831853f * (t / 3.0f + float(gd.moves) / 256.0f)), -kRoadHalf + half, kRoadHalf - half);
    lo = c - half;
    hi = c + half;
}

void Battle::explode(int m, BattleEvents& ev) {
    if (mineGone[m]) return;
    mineGone[m] = 1;
    ev.mine = m;
    bucket();
    blast(def->mines[m].x, def->mines[m].z, 2.5f, damage() * 10, -1, 3, ev);
}

void Battle::volleys(BattleEvents& ev) {
    const bool frozen = powered(kFreeze);
    const float tick = kDt * (powered(kHourglass) ? 0.4f : 1.0f);  // the hourglass slows the archers and their arrows
    for (int i = 0; i < enemyCount; ++i) {
        Enemy& e = enemies[i];
        const float d = e.z - z;
        if (!e.alive || e.kind != kArcher || d <= 0.0f || d > 18.0f || (frozen && d < 45.0f)) continue;
        e.cool -= tick;
        if (e.cool > 0.0f) continue;
        e.cool += 1.6f;
        if (arrowCount < kMaxArrows) {
            arrows[arrowCount++] = {e.x, e.z, 0.8f, e.wave};
            ++ev.arrows;
        }
    }
    int o = 0;
    for (int k = 0; k < arrowCount; ++k) {
        Arrow a = arrows[k];
        a.t -= tick;
        if (a.t <= 0.0f) lose(toll(1), ev, a.wave);
        else arrows[o++] = a;
    }
    arrowCount = o;
}

float Battle::drive(const Enemy& e) const {
    float k = powered(kHourglass) ? 0.4f : 1.0f;
    if (e.slow > 0.0f) k *= e.kind == kBoss ? 0.75f : 0.5f;
    if (e.kind != kBoss)
        for (int d = 0; d < drumN_; ++d) {
            const float dx = e.x - drumX_[d], dz = e.z - drumZ_[d];
            if (dx * dx + dz * dz < 25.0f) {
                k *= 1.4f;
                break;
            }
        }
    return k;
}

void Battle::lightning(float x0, float z0, i32 dmg, BattleEvents& ev) {
    // The nearest enemy in its lane (3 wide, 22 ahead), then four jumps to the nearest within 3 not yet struck.
    const WeaponStats& w = weaponStats(kCoil);
    int struck[5] = {-1, -1, -1, -1, -1};
    float fx = x0, fz = z0;
    for (int jump = 0; jump <= int(w.pierce); ++jump) {
        int best = -1;
        float bestD = 1e9f;
        const float cz = jump == 0 ? z0 + 0.5f * w.range : fz;
        const float reach = jump == 0 ? 0.5f * w.range + 1.0f : w.splash;
        const int c0 = maxv(0, int(cz - reach - z + 2.0f)), c1 = minv(kCells - 1, int(cz + reach - z + 2.0f) + 1);
        for (int c = c0; c <= c1; ++c)
            for (int i = cellHead_[c]; i >= 0; i = next_[i]) {
                const Enemy& e = enemies[i];
                if (!e.alive || i == struck[0] || i == struck[1] || i == struck[2] || i == struck[3] || i == struck[4]) continue;
                float d;
                if (jump == 0) {
                    if (std::fabs(e.x - x0) > w.spread || e.z < z0 || e.z > z0 + w.range) continue;
                    d = e.z - z0;
                } else {
                    const float dx = e.x - fx, dz = e.z - fz;
                    d = dx * dx + dz * dz;
                    if (d > w.splash * w.splash) continue;
                }
                if (d < bestD) {
                    bestD = d;
                    best = i;
                }
            }
        if (best < 0) return;
        struck[jump] = best;
        const float ex = enemies[best].x, ez = enemies[best].z;
        ev.zap(fx, fz, ex, ez);
        hurt(best, maxv<i32>(1, dmg), ev);
        dmg = i32(float(dmg) * w.splashDamage);
        fx = ex;
        fz = ez;
    }
}

void Battle::scorch(i32 dmg, BattleEvents& ev) {
    // Every enemy in the cone from the squad's front (9 long, widening 18 degrees each side) burns; mines in it go off.
    const WeaponStats& w = weaponStats(kFlame);
    float lo, hi;
    footprint(lo, hi);
    const float z0 = z + radius() * 0.45f, base = 0.6f + (hi - lo) * 0.3f;
    for (int c = 0; c <= minv(kCells - 1, int(z0 - z + w.range + 3.0f)); ++c)
        for (int i = cellHead_[c]; i >= 0; i = next_[i]) {
            const Enemy& e = enemies[i];
            const float dz = e.z - z0;
            if (!e.alive || dz < -1.0f || dz > w.range) continue;
            if (std::fabs(e.x - x) <= base + maxv(0.0f, dz) * w.spread + enemyStats(e.kind).radius) hurt(i, dmg, ev);
        }
    for (int m = 0; m < def->mineCount; ++m) {
        const MineDef& md = def->mines[m];
        const float dz = md.z - z0;
        if (!mineGone[m] && dz > -1.0f && dz < w.range && std::fabs(md.x - x) <= base + maxv(0.0f, dz) * w.spread) explode(m, ev);
    }
}

void Battle::bucket() {
    for (int c = 0; c < kCells; ++c) cellHead_[c] = -1;
    for (int i = 0; i < enemyCount; ++i) {
        const Enemy& e = enemies[i];
        if (!e.alive) continue;
        const int c = int(e.z - z + 2.0f);
        if (c < 0 || c >= kCells) continue;
        next_[i] = cellHead_[c];
        cellHead_[c] = i16(i);
    }
}

// The nearest enemy ahead (by distance up the road, then across) that the bullets in flight will not already kill.
int Battle::pickTarget(float bz, float range, bool front) {
    const int c1 = minv(kCells - 1, int(bz - z + range + 2.0f));
    auto needy = [&](const Enemy& e) { return e.alive && e.hp + (front ? e.shield : 0) - e.incoming > 0; };
    for (int c = 2; c <= c1; ++c) {
        int best = -1;
        float bestD = 1e9f;
        for (int i = cellHead_[c]; i >= 0; i = next_[i]) {
            const Enemy& e = enemies[i];
            if (!needy(e)) continue;
            const float d = std::fabs(e.x - x);
            if (d < bestD) {
                bestD = d;
                best = i;
            }
        }
        if (best < 0) continue;
        // A bomber (its burst clears a hole) or a drummer (its beat hurries the rest) up to 12 further on goes first.
        for (int c2 = c; c2 <= minv(c1, c + 12); ++c2)
            for (int i = cellHead_[c2]; i >= 0; i = next_[i]) {
                const Enemy& e = enemies[i];
                if ((e.kind == kBomber || e.kind == kDrummer) && needy(e)) return i;
            }
        return best;
    }
    return -1;
}

float Battle::enemySpeed(const Enemy& e) const {
    const EnemyStats& st = enemyStats(e.kind);
    if (e.kind == kBoss) return st.speed * drive(e);
    if (e.kind == kArcher || e.kind == kCatapult || (powered(kFreeze) && e.z - z < 45.0f)) return 0.0f;
    // It runs at the squad at the march's speed on top of its own: it closes on the standing squad as fast as the two
    // closed when both moved.
    return (st.speed * def->pace + def->speed) * drive(e);
}

i32 Battle::toll(i32 n) const { return maxv<i32>(n, i32(i64(n) * squad / 50)); }

i32 Battle::against(const Enemy& e, i32 dmg) const {
    if (e.kind == kBoss || e.kind == kGiant) return dmg;
    return maxv<i32>(1, i32(i64(dmg) * kHp / damage()));
}

void Battle::release(Bullet& b) {
    if (b.target >= 0 && b.target < enemyCount) {
        Enemy& e = enemies[b.target];
        if (e.alive) e.incoming = maxv<i32>(0, e.incoming - against(e, b.dmg));
    }
    b.target = -1;
}

void Battle::hurt(int i, i32 dmg, BattleEvents& ev, bool front) {
    Enemy& e = enemies[i];
    if (!e.alive) return;
    dmg = against(e, dmg);
    if (front && e.shield > 0) {  // the shield takes it first; what is left of the blow goes through
        e.shield -= dmg;
        dmg = e.shield < 0 ? -e.shield : 0;
        e.shield = maxv<i32>(0, e.shield);
    }
    e.hp -= dmg;
    e.hit = 0;
    ++ev.hits;
    if (e.kind == kBoss) ev.bossHit = true;
    if (e.hp <= 0) kill(i, ev);
}

void Battle::blast(float bx, float bz, float reach, i32 dmg, int spare, u8 what, BattleEvents& ev) {
    ev.spot(bx, bz, what);
    const int c0 = maxv(0, int(bz - z - reach)), c1 = minv(kCells - 1, int(bz - z + reach + 4.0f));
    for (int c = c0; c <= c1; ++c)
        for (int i = cellHead_[c]; i >= 0; i = next_[i]) {
            const Enemy& e = enemies[i];
            if (i == spare || !e.alive) continue;
            const float dx = e.x - bx, dz = e.z - bz, r = reach + 0.5f * enemyStats(e.kind).radius;
            if (dx * dx + dz * dz <= r * r) hurt(i, dmg, ev);
        }
}

void Battle::breakBarrel(int k, BattleEvents& ev) {
    const BarrelDef& bd = def->barrels[k];
    barrels[k].broken = 1;
    ev.barrel = k;
    if (bd.weapon > 0 && bd.weapon < kWeaponCount) {
        weapon = bd.weapon;
        ev.weapon = weapon;
    } else if (bd.power > 0 && bd.power < kPowerCount) {
        take(bd.power, ev);
    } else {
        squad = minv(squad + bd.reward, kSquadCap);
        ev.barrelReward = bd.reward;
    }
}

void Battle::take(u8 p, BattleEvents& ev) {
    if (p == kPowerNone || p >= kPowerCount) return;
    ev.power = p;
    // rapid, power shot, shield, freeze, airstrike, magnet, giants, reinforcements, quick march, hourglass, double gates,
    // field gun (docs/CONTENT_PASS.md 2)
    static const float kLasts[kPowerCount] = {0.0f, 6.0f, 6.0f, 8.0f, 3.0f, 0.0f, 5.0f, 5.0f, 0.0f, 5.0f, 5.0f, 12.0f, 8.0f};
    powerLeft[p] = powerFull[p] = kLasts[p];
    if (p == kShield) shieldPool = maxv<i32>(10, squad * 3 / 10);
    if (p == kReinforce) {  // max(8, 15% of the peak) parachute in over 2 s
        reinforceLeft += float(maxv<i32>(8, peak * 15 / 100));
        reinforceRate = reinforceLeft / 2.0f;
    }
    if (p == kMagnet) magnetT = 0;
    if (p == kFieldGun) fieldGunAcc = 1.0f;  // its first ball at once
    if (p == kAirstrike) {  // three bombs up the road, each on the thickest of the horde where it falls (else the squad's x)
        for (int k = 0; k < 3 && bombCount < kMaxBombs; ++k) {
            const float bz = z + 12.0f + 8.0f * float(k);
            float sx = 0, n = 0;
            for (int i = 0; i < enemyCount; ++i) {
                const Enemy& e = enemies[i];
                if (e.alive && e.kind != kBoss && std::fabs(e.z - bz) < 5.0f) {
                    sx += e.x;
                    n += 1.0f;
                }
            }
            bombs[bombCount++] = {n > 0 ? clampv(sx / n, -3.0f, 3.0f) : x, bz, 0.5f + 0.6f * float(k)};
        }
    }
}

void Battle::lose(i32 n, BattleEvents& ev, int wave) {
    if (n <= 0) return;
    if (powered(kShield) && shieldPool > 0) {
        const i32 a = minv(n, shieldPool);
        shieldPool -= a;
        n -= a;
        ev.absorbed += a;
        if (shieldPool <= 0) {
            powerLeft[kShield] = 0;
            ev.shieldGone = true;
        }
    }
    squad -= n;
    ev.lost += n;
    if (wave >= 0 && wave < kMaxWaves) waveLost[wave] += n;
}

void Battle::powers(BattleEvents& ev) {
    for (int p = 0; p < kPowerCount; ++p)
        if (powerLeft[p] > 0.0f) {
            powerLeft[p] = maxv(0.0f, powerLeft[p] - kDt);
            if (p == kShield && powerLeft[p] <= 0.0f) {
                shieldPool = 0;
                ev.shieldGone = true;
            }
        }
    int o = 0;
    for (int k = 0; k < bombCount; ++k) {
        Bomb bm = bombs[k];
        bm.t -= kDt;
        if (bm.t <= 0.0f) {
            bucket();
            blast(bm.x, bm.z, 3.0f, damage() * 12, -1, 2, ev);
        } else {
            bombs[o++] = bm;
        }
    }
    bombCount = o;
    // The mortar's shells land: a blast of 2.6 over everything (shields, walls and gates do not stop them).
    o = 0;
    for (int k = 0; k < shellCount; ++k) {
        Shell s = shells[k];
        s.t -= kDt;
        if (s.t <= 0.0f) {
            bucket();
            blast(s.x, s.z, weaponStats(kMortar).splash, s.dmg, -1, 5, ev);
        } else {
            shells[o++] = s;
        }
    }
    shellCount = o;
    // REINFORCEMENTS drift down over 2 s.
    if (reinforceLeft > 0.0f) {
        const float add = minv(reinforceLeft, reinforceRate * kDt);
        reinforceLeft -= add;
        reinforceAcc += add;
        const i32 whole = i32(reinforceAcc);
        if (whole > 0) {
            reinforceAcc -= float(whole);
            squad = minv(squad + whole, kSquadCap);
            ev.reinforced += whole;
        }
    }
    // MAGNET: every 0.3 s the nearest barrel, crate or cage within 25 ahead opens by itself.
    if (powered(kMagnet) && (magnetT -= kDt) <= 0.0f) {
        magnetT = 0.3f;
        int best = -1;
        float bestZ = 1e9f;
        for (int k = 0; k < def->barrelCount; ++k) {
            const float d = def->barrels[k].z - z;
            if (!barrels[k].broken && d > 0.0f && d < 25.0f && d < bestZ) {
                bestZ = d;
                best = k;
            }
        }
        for (int c = 0; c < def->cageCount; ++c) {
            const float d = def->cages[c].z - z;
            if (!cageOpen[c] && d > 0.0f && d < 25.0f && d < bestZ) {
                bestZ = d;
                best = 100 + c;
            }
        }
        if (best >= 100) {
            cageOpen[best - 100] = 1;
            ev.cage = best - 100;
            squad = minv(squad + def->cages[best - 100].reward, kSquadCap);
            ev.magnet = best;
        } else if (best >= 0) {
            barrels[best].hp = 0;
            breakBarrel(best, ev);
            ev.magnet = best;
        }
    }
}

void Battle::kill(int i, BattleEvents& ev) {
    Enemy& e = enemies[i];
    e.alive = 0;
    --alive_;
    ++ev.kills;
    if (ev.killN < BattleEvents::kMaxKills) {
        ev.killX[ev.killN] = e.x;
        ev.killZ[ev.killN] = e.z;
        ev.killKind[ev.killN] = e.kind;
        ++ev.killN;
    }
    if (e.kind == kBoss) {
        ev.bossDown = true;
        won = true;
        ev.won = true;
    }
    if (e.kind == kGiant && dropCount < kMaxDrops) {  // a giant drops a medallion where it falls
        drops[dropCount] = {e.x, e.z, u8(e.seed & 1 ? kPowerShot : kRapid)};
        dropTaken[dropCount++] = 0;
    }
    if (e.kind == kBomber) blast(e.x, e.z, 2.2f, damage() * 10, i, 4, ev);  // its keg goes up (other bombers chain)
}

float Battle::share(float a, float b) const {
    float lo, hi;
    footprint(lo, hi);
    return clampv((minv(hi, b) - maxv(lo, a)) / maxv(0.2f, hi - lo), 0.0f, 1.0f);
}

void Battle::strike(const Strike& s, BattleEvents& ev) {
    if (strikeCount >= kMaxStrikes) return;
    strikes[strikeCount++] = s;
    ev.strikeShown = s.kind;
}

void Battle::attacks(BattleEvents& ev) {
    const bool frozen = powered(kFreeze);
    const float hg = powered(kHourglass) ? 0.4f : 1.0f;  // the hourglass slows the engines, the boss and its blows
    // Catapults within 45 units hurl a boulder at where the squad will be when it lands, 1.2 s later.
    for (int i = 0; i < enemyCount; ++i) {
        Enemy& e = enemies[i];
        const float d = e.z - z;
        if (!e.alive || e.kind != kCatapult || d < 6.0f || d > 45.0f || frozen) continue;
        e.cool -= kDt * hg;
        if (e.cool > 0.0f) continue;
        e.cool += 3.2f;
        Strike s;
        s.kind = 0;
        s.x = x;
        s.z = z + def->speed * 1.2f;
        s.r = 1.7f;
        s.t = s.full = 1.2f;
        s.fromX = e.x;
        s.fromZ = e.z;
        s.cap = 14;
        strike(s, ev);
        ++ev.thrown;
    }
    // The boss's attacks while it is in sight and not frozen: the club when it is close, boulders while it is not, the
    // stomp's ridge, a war cry at two thirds and one third of its strength.
    if (boss_ >= 0 && def->bossAttacks && !(powerLeft[kFreeze] > powerFull[kFreeze] * 0.5f)) {
        const Enemy& bo = enemies[boss_];
        const float d = bo.z - z;
        const u8 a = def->bossAttacks;
        if (d < 60.0f) {
            if ((a & kAttackSlam) && d < 14.0f && (slamT -= kDt * hg) <= 0.0f) {
                slamT = 4.5f;
                Strike s;
                s.kind = 1;
                s.x = x < 0.0f ? -1.0f : 1.0f;
                s.z = z;
                s.t = s.full = 1.0f;
                s.cap = maxv<i32>(1, squad * 12 / 100);
                strike(s, ev);
            }
            if ((a & kAttackToss) && d >= 14.0f && (tossT -= kDt * hg) <= 0.0f) {
                tossT = 3.0f;
                Strike s;
                s.kind = 0;
                s.x = x;
                s.z = z + def->speed * 1.2f;
                s.r = 2.0f;
                s.t = s.full = 1.2f;
                s.fromX = bo.x;
                s.fromZ = bo.z;
                s.cap = maxv<i32>(1, squad / 10);
                s.byBoss = 1;
                strike(s, ev);
                ++ev.thrown;
                ev.bossThrew = true;
            }
            if ((a & kAttackStomp) && (stompT -= kDt * hg) <= 0.0f) {
                stompT = 5.0f;
                Strike s;
                s.kind = 2;
                s.x = -3.0f + 6.0f * float(rand() & 0xFFFF) / 65535.0f;
                s.z = bo.z;
                s.t = s.full = maxv(0.8f, d / 12.0f);  // the ridge rolls 12 units a second
                s.cap = maxv<i32>(1, squad / 10);
                strike(s, ev);
            }
            if (a & kAttackCry) {
                const i32 full = def->bossHp * kHp;
                for (int k = 0; k < 2; ++k) {
                    if ((criesDone & (1 << k)) || bo.hp > full * (k == 0 ? 66 : 33) / 100) continue;
                    criesDone = u8(criesDone | (1 << k));
                    ev.roar = true;
                    const int n = def->spike ? 24 : 16;  // its escort runs in from behind it
                    for (int j = 0; j < n && enemyCount < kMaxEnemies; ++j) {
                        Enemy& e = enemies[enemyCount++];
                        e = Enemy{};
                        e.x = -3.6f + 7.2f * float(j % 8) / 7.0f;
                        e.z = bo.z + 2.0f + 0.9f * float(j / 8);
                        e.hp = enemyStats(kFoot).hp * kHp;
                        e.kind = kFoot;
                        e.alive = 1;
                        e.seed = u16(rand());
                        e.hit = 99;
                        ++alive_;
                    }
                }
            }
        }
    }
    // Blows land on whatever of the squad stands in them.
    int o = 0;
    for (int k = 0; k < strikeCount; ++k) {
        Strike s = strikes[k];
        s.t -= kDt * hg;
        if (s.t > 0.0f) {
            strikes[o++] = s;
            continue;
        }
        float f;
        if (s.kind == 0) f = std::fabs(s.z - z) <= s.r + radius() * 0.6f ? share(s.x - s.r, s.x + s.r) : 0.0f;
        else if (s.kind == 1) f = s.x < 0.0f ? share(-kRoadHalf, 0.0f) : share(0.0f, kRoadHalf);
        else f = 1.0f - share(s.x - 1.5f, s.x + 1.5f);
        const i32 n = minv(s.cap, i32(std::ceil(float(squad) * f - 0.001f)));
        lose(n, ev);
        ev.strikeLanded = s.kind;
        ev.strikeX = s.x;
        ev.strikeZ = s.kind == 0 ? s.z : z;
    }
    strikeCount = o;
}

void Battle::fire(BattleEvents& ev) {
    const WeaponStats& w = weaponStats(weapon);
    const float boost = powered(kRapid) ? 2.0f : 1.0f, power = powered(kPowerShot) ? 2.0f : 1.0f;
    const float r = radius();
    float lo, hi;
    footprint(lo, hi);
    const float cx = (lo + hi) * 0.5f, hw = (hi - lo) * 0.5f;
    bucket();
    // A cannonball: from x at the squad's front, straight up the road, through everything on its line.
    auto ball = [&](float bx, i32 dmg) {
        if (bulletCount >= kMaxBullets) return;
        Bullet& b = bullets[bulletCount++];
        b = Bullet{};
        b.x = bx;
        b.z = z + r * 0.45f;
        b.zEnd = b.z + weaponStats(kCannon).range;
        b.dmg = dmg;
        b.kind = kCannon;
        b.pierce = 255;
        b.alive = 1;
        ++ev.shots;
    };
    // FIELD GUN: a cannon at the squad's side fires every second, whatever the weapon.
    if (powered(kFieldGun) && (fieldGunAcc += kDt) >= 1.0f) {
        fieldGunAcc -= 1.0f;
        ball(clampv(hi + 0.6f, -kRoadHalf + 0.6f, kRoadHalf - 0.6f), maxv<i32>(1, i32(float(damage()) * 8.0f * power)));
    }
    // The flamethrower: ten ticks a second on its cone, no projectiles.
    if (weapon == kFlame) {
        flameAcc += 10.0f * boost * kDt;
        while (flameAcc >= 1.0f) {
            flameAcc -= 1.0f;
            scorch(maxv<i32>(1, i32(float(damage()) * w.damage * power + 0.5f)), ev);
            ++ev.shots;
        }
        return;
    }
    // Every shot goes for the nearest enemy that the shots already flying will not kill, led to where they meet: the
    // fire spreads over the front ranks, a brute in front takes as many as it needs, the boss takes all once nothing
    // stands before it (the owner, 2026-11-27). With no enemy in range a shot flies straight up the road. Twin rifles
    // fire a pair side by side; the scattergun a fan, its middle pellet aimed. The cannon fires at least once a second.
    fireAcc_ += (weapon == kCannon ? maxv(1.0f, fireRate() * w.rate) : fireRate() * w.rate) * boost * kDt;
    const i32 dmg = maxv<i32>(1, i32(float(damage()) * w.damage * power + 0.5f));
    while (fireAcc_ >= 1.0f) {
        fireAcc_ -= 1.0f;
        if (weapon == kCannon) {
            ball(x, dmg);
            continue;
        }
        if (weapon == kCoil) {  // lightning: instant, in its lane
            lightning(x, z + r * 0.45f, dmg, ev);
            ++ev.shots;
            continue;
        }
        if (weapon == kMortar) {  // a shell lobbed to land 18 ahead of where the squad will be, at its x
            if (shellCount < kMaxShells) {
                const float u = float(rand() & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
                shells[shellCount++] = {cx + u * hw * 0.5f, z + r * 0.45f, x + u * 0.6f, z + w.range + def->speed * 1.1f, 1.1f, dmg};
                ++ev.shots;
            }
            continue;
        }
        if (bulletCount + w.balls > kMaxBullets) continue;
        const float u = float(rand() & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
        const float bx = cx + u * hw * 0.85f, bz = z + r * 0.45f;
        const int t = pickTarget(bz, w.range, fromFront(weapon));
        float vx = 0;
        if (t >= 0) {
            const Enemy& e = enemies[t];
            const float closing = w.speed + enemySpeed(e);
            const float zMeet = bz + w.speed * maxv(0.0f, e.z - bz) / closing;
            vx = (e.x - bx) / maxv(0.6f, zMeet - bz);
        }
        for (int k = 0; k < w.balls; ++k) {
            Bullet& b = bullets[bulletCount++];
            b.x = bx;
            b.z = bz;
            b.zEnd = bz + w.range;
            b.vx = vx;
            b.dmg = dmg;
            b.kind = weapon;
            b.pierce = w.pierce;
            b.grows = 1;
            b.target = -1;
            b.alive = 1;
            const bool aimed = w.balls <= 2 || k == w.balls / 2;
            if (w.balls == 2) {
                b.x += (k ? 0.5f : -0.5f) * w.spread;
            } else if (w.balls > 2) {
                b.vx = std::tan(std::atan(vx) + (float(k) / float(w.balls - 1) * 2.0f - 1.0f) * w.spread);
                b.grows = aimed;
            }
            if (t >= 0 && aimed) {
                b.target = i16(t);
                enemies[t].incoming += against(enemies[t], dmg);
            }
        }
        ++ev.shots;
    }
}

void Battle::moveBullets(BattleEvents& ev) {
    // Enemies by distance ahead, one unit a cell: fire() bucketed them this step and nothing has moved since.
    int o = 0;
    for (int bi = 0; bi < bulletCount; ++bi) {
        Bullet b = bullets[bi];
        const WeaponStats& w = weaponStats(b.kind);
        const float zp = b.z, zn = b.z + w.speed * kDt;
        b.x += b.vx * (zn - zp);
        if (b.kind == kCannon) {
            // The ball rolls through everything on its line (1.2 wide): each enemy it passes takes its damage once (a brute
            // or a giant is knocked back 1.5), barrels and crates break, cages open, mines go off, growing gates gain one.
            const int c0 = maxv(0, int(zp - z + 2.0f) - 2), c1 = minv(kCells - 1, int(zn - z + 2.0f) + 2);
            for (int c = c0; c <= c1; ++c)
                for (int i = cellHead_[c]; i >= 0; i = next_[i]) {
                    Enemy& e = enemies[i];
                    const EnemyStats& st = enemyStats(e.kind);
                    const float v = enemySpeed(e) * kDt;
                    if (!e.alive || std::fabs(e.x - b.x) > st.radius + w.spread || e.z <= zp || e.z > zn + v) continue;
                    if (ev.spotN < 6) ev.spot(e.x, e.z, 6);
                    if (e.kind == kBrute || e.kind == kGiant) e.z += 1.5f;
                    hurt(i, b.dmg, ev);
                }
            for (int k = 0; k < def->barrelCount; ++k) {
                const BarrelDef& bd = def->barrels[k];
                if (barrels[k].broken || std::fabs(b.x - bd.x) > 1.45f || bd.z < zp - 0.6f || bd.z > zn + 0.6f) continue;
                barrels[k].hp = 0;
                breakBarrel(k, ev);
            }
            for (int c = 0; c < def->cageCount; ++c) {
                const CageDef& cd = def->cages[c];
                if (cageOpen[c] || std::fabs(b.x - cd.x) > 1.5f || cd.z < zp - 0.8f || cd.z > zn + 0.8f) continue;
                cageOpen[c] = 1;
                ev.cage = c;
                squad = minv(squad + cd.reward, kSquadCap);
            }
            for (int m = 0; m < def->mineCount; ++m) {
                const MineDef& md = def->mines[m];
                if (!mineGone[m] && std::fabs(b.x - md.x) < 1.1f && md.z >= zp - 0.5f && md.z < zn + 0.5f) explode(m, ev);
            }
            for (int g = 0; g < def->gateCount; ++g) {
                const GateDef& gd = def->gates[g];
                GateState& gs = gates[g];
                if (gs.used || gs.passed || !gd.grows || gs.value >= gd.cap || gd.z < zp || gd.z >= zn) continue;
                float g0, g1;
                gateSpan(g, time, g0, g1);
                if (b.x < g0 || b.x > g1) continue;
                gs.value = minv(gs.value + 1, gd.cap);
                gs.hit = 0;
            }
            b.z = zn;
            if (b.z < b.zEnd) bullets[o++] = b;
            continue;
        }
        // The nearest thing it strikes along this step: a gate that grows, a barrel (or crate), a mine.
        float wallZ = 1e9f;
        int kind = 0, which = -1;  // 1 gate, 2 barrel, 3 mine
        for (int g = 0; g < def->gateCount; ++g) {
            const GateDef& gd = def->gates[g];
            const GateState& gs = gates[g];
            if (gs.used || gs.passed || !gd.grows || gs.value >= gd.cap || gd.z < zp || gd.z > zn) continue;
            float g0, g1;
            gateSpan(g, time, g0, g1);
            if (b.x < g0 || b.x > g1) continue;
            if (gd.z < wallZ) {
                wallZ = gd.z;
                kind = 1;
                which = g;
            }
        }
        for (int k = 0; k < def->barrelCount; ++k) {
            const BarrelDef& bd = def->barrels[k];
            if (barrels[k].broken || std::fabs(b.x - bd.x) > 0.85f || bd.z < zp - 0.6f || bd.z > zn + 0.6f) continue;
            if (bd.z - 0.6f < wallZ) {
                wallZ = bd.z - 0.6f;
                kind = 2;
                which = k;
            }
        }
        for (int m = 0; m < def->mineCount; ++m) {
            const MineDef& md = def->mines[m];
            if (mineGone[m] || std::fabs(b.x - md.x) > 0.5f || md.z < zp - 0.5f || md.z > zn + 0.5f) continue;
            if (md.z - 0.5f < wallZ) {
                wallZ = md.z - 0.5f;
                kind = 3;
                which = m;
            }
        }
        // Enemies before that, nearest first: a ball stops at the first; a lance bolt goes through up to `pierce` of
        // them (each once: those whose middle it passes this step) and stops at the boss.
        const int c0 = maxv(0, int(zp - z + 2.0f) - 2), c1 = minv(kCells - 1, int(zn - z + 2.0f) + 2);
        bool stopped = false;
        float lastZ = -1e9f;
        int lastI = -1;
        for (int pass = 0; pass < 8 && !stopped; ++pass) {
            float bestZ = wallZ;
            int hit = -1;
            for (int c = c0; c <= c1; ++c)
                for (int i = cellHead_[c]; i >= 0; i = next_[i]) {
                    const Enemy& e = enemies[i];
                    if (!e.alive) continue;
                    const float rad = enemyStats(e.kind).radius;
                    if (std::fabs(e.x - b.x) > rad) continue;
                    float ez;
                    if (b.pierce > 0 && e.kind != kBoss) {
                        if (e.z <= zp || e.z > zn + enemySpeed(e) * kDt) continue;
                        ez = e.z;
                    } else {
                        if (e.z - rad > zn || e.z + rad < zp) continue;
                        ez = e.z - rad;
                    }
                    if ((ez < bestZ || (ez == bestZ && hit >= 0 && i < hit)) && (ez > lastZ || (ez == lastZ && i > lastI))) {
                        bestZ = ez;
                        hit = i;
                    }
                }
            if (hit < 0) break;
            release(b);
            const bool boss = enemies[hit].kind == kBoss;
            const float ex = enemies[hit].x, ez = enemies[hit].z;
            if (b.kind == kFrost) enemies[hit].slow = 2.0f;  // the frost blaster: half speed for 2 s (the boss three quarters)
            hurt(hit, b.dmg, ev, fromFront(b.kind));
            if (w.splash > 0.0f) {
                blast(b.x, bestZ, w.splash, i32(float(b.dmg) * w.splashDamage / w.damage + 0.5f), hit, 0, ev);
                stopped = true;
            } else if (b.pierce > 0 && !boss) {
                ev.spot(ex, ez, 1);
                stopped = --b.pierce == 0;
                lastZ = bestZ;
                lastI = hit;
            } else {
                stopped = true;
            }
        }
        if (stopped) continue;
        if (kind != 0) {  // it strikes the gate, the barrel or the mine and stops there
            release(b);
            if (kind == 1) {
                GateState& gs = gates[which];
                if (b.grows) gs.value = minv(gs.value + 1, def->gates[which].cap);
                gs.hit = 0;
                ++ev.hits;
            } else if (kind == 2) {
                BarrelState& bs = barrels[which];
                bs.hp -= b.dmg;
                bs.hit = 0;
                ++ev.hits;
                if (bs.hp <= 0) breakBarrel(which, ev);
            } else {
                mineHp[which] -= b.dmg;
                ++ev.hits;
                if (mineHp[which] <= 0) explode(which, ev);
            }
            if (w.splash > 0.0f) blast(b.x, wallZ, w.splash, i32(float(b.dmg) * w.splashDamage / w.damage + 0.5f), -1, 0, ev);
            continue;
        }
        b.z = zn;
        if (b.z < b.zEnd) bullets[o++] = b;
        else release(b);
    }
    bulletCount = o;
}

void Battle::moveEnemies(BattleEvents& ev) {
    const float r = radius();
    float lo, hi;
    footprint(lo, hi);
    // The drummers' beat this step (whoever is within 5 of one goes 40% faster: drive).
    drumN_ = 0;
    for (int i = 0; i < enemyCount && drumN_ < 16; ++i)
        if (enemies[i].alive && enemies[i].kind == kDrummer) {
            drumX_[drumN_] = enemies[i].x;
            drumZ_[drumN_++] = enemies[i].z;
        }
    const bool giants = powered(kGiants);  // GIANTS: what touches the squad is trampled and takes nothing
    for (int i = 0; i < enemyCount; ++i) {
        Enemy& e = enemies[i];
        if (!e.alive) continue;
        const EnemyStats& st = enemyStats(e.kind);
        e.hit += kDt;
        const float d = e.z - z, k = drive(e);
        e.slow = maxv(0.0f, e.slow - kDt);
        if (e.kind == kBoss) {
            if (powerLeft[kFreeze] > powerFull[kFreeze] * 0.5f) continue;  // frozen (half as long as the horde)
            if (d - st.radius <= r * 0.6f) {  // it reaches the squad: it stops and crushes soldiers until it falls
                // It crushes a ninth of the squad it meets every second (at least the level's `drain`), whatever its size:
                // the fight lasts seconds (half as fast against GIANTS, slowed by the hourglass).
                if (bossRate <= 0.0f) bossRate = maxv(def->drain, float(squad) / 9.0f);
                bossDrain += bossRate * (giants ? 0.5f : 1.0f) * (powered(kHourglass) ? 0.4f : 1.0f) * kDt;
                while (bossDrain >= 1.0f && squad > 0) {
                    bossDrain -= 1.0f;
                    lose(1, ev);
                }
            } else {
                e.z -= st.speed * k * kDt;
                e.x += clampv(x - e.x, -1.0f, 1.0f) * 0.8f * k * kDt;
            }
            continue;
        }
        if (!(powered(kFreeze) && d < 45.0f) && e.kind != kArcher && e.kind != kCatapult) {  // frozen (or one that stands)
            e.z -= enemySpeed(e) * kDt;
            if (d < 14.0f) e.x += clampv(x - e.x, -1.0f, 1.0f) * (e.kind == kRider ? 2.4f : 1.6f) * def->pace * k * kDt;
        }
        if (d < r * 0.6f + st.radius && e.x > lo - st.radius && e.x < hi + st.radius) {
            if (!giants) lose(toll(st.strength), ev, e.wave);
            if (giants && ev.killN < BattleEvents::kMaxKills) {  // trampled: shown as a kill
                ev.killX[ev.killN] = e.x;
                ev.killZ[ev.killN] = e.z;
                ev.killKind[ev.killN++] = e.kind;
            }
            e.alive = 0;
            --alive_;
        } else if (d < -8.0f) {
            e.alive = 0;
            --alive_;
        }
    }
    // Compact now and then; the boss's index is found again.
    if (enemyCount > 64 && alive_ < enemyCount / 2) {
        int o = 0;
        for (int i = 0; i < enemyCount; ++i) {
            remap_[i] = enemies[i].alive ? i16(o) : i16(-1);
            if (enemies[i].alive) enemies[o++] = enemies[i];
        }
        enemyCount = o;
        for (int bi = 0; bi < bulletCount; ++bi)
            if (bullets[bi].target >= 0) bullets[bi].target = remap_[bullets[bi].target];
    }
    boss_ = -1;
    for (int i = 0; i < enemyCount; ++i)
        if (enemies[i].alive && enemies[i].kind == kBoss) boss_ = i;
}

void Battle::tick(float steerX, bool go, BattleEvents& ev) {
    ev.clear();
    if (won || lost || !def) return;
    if (gatesInit_ < def->gateCount || barrelsInit_ < def->barrelCount) sync();
    if (!started) {
        if (!go) return;
        started = true;
    }
    time += kDt;
    // Steer: the squad follows the finger, fast but not at once (half again as quick on QUICK MARCH).
    const float r = radius();
    const float lim = kRoadHalf - minv(r * 0.5f, 1.6f);
    const float tx = clampv(steerX, -lim, lim);
    const float quick = powered(kQuick) ? 1.5f : 1.0f;
    vx = clampv((tx - x) * 12.0f * quick, -18.0f * quick, 18.0f * quick);
    x += vx * kDt;
    // March. The squad stands its ground while a horde that comes to it is within 25 units ahead, and marches on once
    // that horde is beaten (the owner, 2026-11-27: "the enemies come to the squad"); archers and catapults stand, so they
    // do not stop it. It halts before the boss.
    const float zPrev = z;
    const bool bossNear = boss_ >= 0 && enemies[boss_].z - z < 8.0f;
    halted = false;
    for (int i = 0; i < enemyCount && !halted; ++i) {
        const Enemy& e = enemies[i];
        const float d = e.z - z;
        halted = e.alive && d > 0.5f && d < 25.0f && e.kind != kBoss && e.kind != kArcher && e.kind != kCatapult;
    }
    if (!bossNear && !halted && !(def->bossHp == 0 && z >= def->length)) z += def->speed * (powered(kQuick) ? 1.6f : 1.0f) * kDt;
    while (nextWave < def->waveCount && def->waves[nextWave].z - z < kWake) spawnWave(nextWave++, ev);
    if (def->bossHp > 0 && boss_ < 0 && !won && def->length - z < kWake && enemyCount < kMaxEnemies) {
        bool any = false;
        for (int i = 0; i < enemyCount; ++i) any = any || (enemies[i].kind == kBoss);
        if (!any) {
            Enemy& e = enemies[enemyCount++];
            e.x = 0;
            e.z = def->length;
            e.hp = def->bossHp * kHp;
            e.incoming = 0;
            e.kind = kBoss;
            e.alive = 1;
            e.seed = u16(rand());
            e.hit = 99;
            e.wave = -1;
            boss_ = enemyCount - 1;
            ++alive_;
        }
    }
    // Gates the squad's centre crosses: the one it is inside works; the others are left behind.
    for (int g = 0; g < def->gateCount; ++g) {
        const GateDef& gd = def->gates[g];
        GateState& gs = gates[g];
        if (gs.used || gs.passed || gd.z <= zPrev || gd.z > z) continue;
        float g0, g1;
        gateSpan(g, time, g0, g1);
        if (x >= g0 && x <= g1) {
            ev.gate = g;
            ev.gateBefore = squad;
            if (gd.kind == kGateWeapon) {  // it hands over its weapon
                weapon = u8(clampv<i32>(gs.value, 0, kWeaponCount - 1));
                ev.weapon = weapon;
            } else {
                // DOUBLE GATES: the next add gate counts twice (a barricade's loss does not).
                const bool twice = gd.kind == kGateAdd && gs.value > 0 && powered(kDouble);
                if (twice) {
                    powerLeft[kDouble] = 0;
                    ev.doubled = true;
                }
                const i64 s = gd.kind == kGateMul ? i64(squad) * gs.value : i64(squad) + gs.value * (twice ? 2 : 1);
                squad = i32(clampv<i64>(s, 0, kSquadCap));
                if (def->mode == kModeGold) squad = maxv(squad, minv<i32>(3, ev.gateBefore));  // a bonus road keeps 3
            }
            ev.gateAfter = squad;
            gs.used = 1;
            if (squad <= 0) ev.lost += ev.gateBefore;
        } else {
            gs.passed = 1;
        }
    }
    // Barrels the squad runs into cost soldiers and break.
    float lo, hi;
    footprint(lo, hi);
    for (int k = 0; k < def->barrelCount; ++k) {
        const BarrelDef& bd = def->barrels[k];
        BarrelState& bs = barrels[k];
        if (bs.broken || bd.z > z + r * 0.5f + 0.6f || bd.z < z - 1.0f) continue;
        if (std::fabs(bd.x - (lo + hi) * 0.5f) < 0.9f + (hi - lo) * 0.3f) {
            bs.broken = 1;
            if (bd.power > 0) {  // a supply crate is a gift: run into, it opens at no cost
                ev.barrel = k;
                take(bd.power, ev);
                continue;
            }
            if (!powered(kGiants)) lose(maxv<i32>(1, (bs.hp + kHp - 1) / kHp / 2), ev);  // giants smash through for free
            ev.crashed = k;
            if (bd.weapon > 0 && bd.weapon < kWeaponCount) {  // a crate run into still hands over its weapon
                weapon = bd.weapon;
                ev.weapon = weapon;
            }
        }
    }
    // Medallions the footprint touches.
    for (int m = 0; m < def->medalCount; ++m) {
        const MedalDef& md = def->medals[m];
        if (medalTaken[m] || md.z > z + r * 0.6f + 0.6f || md.z < z - r * 0.6f - 0.6f) continue;
        if (md.x > lo - 0.6f && md.x < hi + 0.6f) {
            medalTaken[m] = 1;
            ev.medal = m;
            take(md.power, ev);
        }
    }
    // Medallions dropped by giants, the same way.
    for (int m = 0; m < dropCount; ++m) {
        const MedalDef& md = drops[m];
        if (dropTaken[m] || md.z > z + r * 0.6f + 0.6f || md.z < z - r * 0.6f - 0.6f) continue;
        if (md.x > lo - 0.6f && md.x < hi + 0.6f) {
            dropTaken[m] = 1;
            take(md.power, ev);
        }
    }
    // Cages the footprint touches free their prisoners; mines it touches go off under it.
    for (int c = 0; c < def->cageCount; ++c) {
        const CageDef& cd = def->cages[c];
        if (cageOpen[c] || cd.z > z + r * 0.6f + 0.8f || cd.z < z - r * 0.6f - 0.8f) continue;
        if (cd.x > lo - 0.8f && cd.x < hi + 0.8f) {
            cageOpen[c] = 1;
            ev.cage = c;
            squad = minv(squad + cd.reward, kSquadCap);
        }
    }
    for (int m = 0; m < def->mineCount; ++m) {
        const MineDef& md = def->mines[m];
        if (mineGone[m] || md.z > z + r * 0.6f + 0.5f || md.z < z - r * 0.6f - 0.5f) continue;
        if (md.x > lo - 0.5f && md.x < hi + 0.5f) {
            ev.mineStepped = true;
            lose(toll(6), ev);
            explode(m, ev);
        }
    }
    fire(ev);
    moveBullets(ev);
    moveEnemies(ev);
    volleys(ev);
    attacks(ev);
    powers(ev);
    for (int g = 0; g < def->gateCount; ++g) gates[g].hit += kDt;
    for (int k = 0; k < def->barrelCount; ++k) barrels[k].hit += kDt;
    if (squad <= 0) {
        squad = 0;
        lost = true;
        ev.lostAll = true;
        return;
    }
    // Won at the finish line, or once every wave has come and no enemy is left (the owner, 2026-11-27: "level should be
    // finished when finish line is crossed or all enemies are gone"); a boss level when the boss falls (kill).
    const bool cleared = clearWins && def->waveCount > 0 && nextWave >= def->waveCount && alive_ == 0;
    if (!won && def->bossHp == 0 && (z >= def->length || cleared)) {
        won = true;
        ev.won = true;
    }
    peak = maxv(peak, squad);
}

int starsFor(const LevelDef& d, i32 left) {
    if (left >= d.star3 && d.star3 > 0) return 3;
    if (left >= d.star2 && d.star2 > 0) return 2;
    return 1;
}

}  // namespace pm::sim
