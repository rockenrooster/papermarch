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
};
// docs/CONTENT_PASS.md 1 (rate and damage times the musket's; the twin fires pairs, the scattergun a fan of five).
const WeaponStats kWeapons[kWeaponCount] = {
    {1.0f, 1.0f, 48.0f, 42.0f, 1, 0.0f, 0, 0.0f, 0.0f},      // musket
    {1.0f, 0.6f, 48.0f, 48.0f, 2, 0.5f, 0, 0.0f, 0.0f},      // twin rifles: two streams 0.5 apart
    {0.8f, 0.45f, 14.0f, 36.0f, 5, 0.244f, 0, 0.0f, 0.0f},   // scattergun: a 28 degree fan
    {0.5f, 1.2f, 60.0f, 75.0f, 1, 0.0f, 6, 0.0f, 0.0f},      // long lance: through six
    {0.25f, 3.0f, 45.0f, 22.0f, 1, 0.0f, 0, 1.8f, 1.5f},     // rockets: a burst where they strike
};
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
    started = won = lost = false;
    enemyCount = bulletCount = 0;
    for (int i = 0; i < d.gateCount; ++i) gates[i] = {d.gates[i].value, 0, 0, 99.0f};
    for (int i = 0; i < d.barrelCount; ++i) barrels[i] = {d.barrels[i].hp * kHp, 0, 99.0f};
    nextWave = 0;
    bossDrain = bossRate = 0;
    for (i32& l : waveLost) l = 0;
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
        e.hp = st.hp * kHp;
        e.incoming = 0;
        e.kind = wd.kind;
        e.alive = 1;
        e.seed = u16(rand());
        e.hit = 99;
        e.wave = i8(w);
        ++alive_;
    }
    ev.wave = w;
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
int Battle::pickTarget(float bz, float range) {
    const int c1 = minv(kCells - 1, int(bz - z + range + 2.0f));
    for (int c = 2; c <= c1; ++c) {
        int best = -1;
        float bestD = 1e9f;
        for (int i = cellHead_[c]; i >= 0; i = next_[i]) {
            const Enemy& e = enemies[i];
            if (!e.alive || e.hp - e.incoming <= 0) continue;
            const float d = std::fabs(e.x - x);
            if (d < bestD) {
                bestD = d;
                best = i;
            }
        }
        if (best >= 0) return best;
    }
    return -1;
}

void Battle::release(Bullet& b) {
    if (b.target >= 0 && b.target < enemyCount) {
        Enemy& e = enemies[b.target];
        if (e.alive) e.incoming = maxv<i32>(0, e.incoming - b.dmg);
    }
    b.target = -1;
}

void Battle::hurt(int i, i32 dmg, BattleEvents& ev) {
    Enemy& e = enemies[i];
    if (!e.alive) return;
    e.hp -= dmg;
    e.hit = 0;
    ++ev.hits;
    if (e.kind == kBoss) ev.bossHit = true;
    if (e.hp <= 0) kill(i, ev);
}

void Battle::blast(float bx, float bz, i32 dmg, int spare, BattleEvents& ev) {
    const float reach = weaponStats(kRockets).splash;
    ev.spot(bx, bz, 0);
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
    } else {
        squad = minv(squad + bd.reward, kSquadCap);
        ev.barrelReward = bd.reward;
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
}

void Battle::fire(BattleEvents& ev) {
    const WeaponStats& w = weaponStats(weapon);
    fireAcc_ += fireRate() * w.rate * kDt;
    const float r = radius();
    float lo, hi;
    footprint(lo, hi);
    const float cx = (lo + hi) * 0.5f, hw = (hi - lo) * 0.5f;
    // Every shot goes for the nearest enemy that the shots already flying will not kill, led to where they meet: the
    // fire spreads over the front ranks, a brute in front takes as many as it needs, the boss takes all once nothing
    // stands before it (the owner, 2026-11-27). With no enemy in range a shot flies straight up the road. Twin rifles
    // fire a pair side by side; the scattergun a fan, its middle pellet aimed.
    bucket();
    const i32 dmg = maxv<i32>(1, i32(float(damage()) * w.damage + 0.5f));
    while (fireAcc_ >= 1.0f) {
        fireAcc_ -= 1.0f;
        if (bulletCount + w.balls > kMaxBullets) continue;
        const float u = float(rand() & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
        const float bx = cx + u * hw * 0.85f, bz = z + r * 0.45f;
        const int t = pickTarget(bz, w.range);
        float vx = 0;
        if (t >= 0) {
            const Enemy& e = enemies[t];
            const float closing = w.speed + (e.kind == kBoss ? enemyStats(kBoss).speed : enemyStats(e.kind).speed * def->pace);
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
                enemies[t].incoming += dmg;
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
        // The nearest thing it strikes along this step: a gate that grows, a barrel (or crate).
        float wallZ = 1e9f;
        int kind = 0, which = -1;  // 1 gate, 2 barrel
        for (int g = 0; g < def->gateCount; ++g) {
            const GateDef& gd = def->gates[g];
            const GateState& gs = gates[g];
            if (gs.used || gs.passed || !gd.grows || gs.value >= gd.cap) continue;
            if (gd.z < zp || gd.z > zn || b.x < gd.x0 || b.x > gd.x1) continue;
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
                        if (e.z <= zp || e.z > zn + enemyStats(e.kind).speed * def->pace * kDt) continue;
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
            hurt(hit, b.dmg, ev);
            if (w.splash > 0.0f) {
                blast(b.x, bestZ, i32(float(b.dmg) * w.splashDamage / w.damage + 0.5f), hit, ev);
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
        if (kind != 0) {  // it strikes the gate or the barrel and stops there
            release(b);
            if (kind == 1) {
                GateState& gs = gates[which];
                if (b.grows) gs.value = minv(gs.value + 1, def->gates[which].cap);
                gs.hit = 0;
                ++ev.hits;
            } else {
                BarrelState& bs = barrels[which];
                bs.hp -= b.dmg;
                bs.hit = 0;
                ++ev.hits;
                if (bs.hp <= 0) breakBarrel(which, ev);
            }
            if (w.splash > 0.0f) blast(b.x, wallZ, i32(float(b.dmg) * w.splashDamage / w.damage + 0.5f), -1, ev);
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
    for (int i = 0; i < enemyCount; ++i) {
        Enemy& e = enemies[i];
        if (!e.alive) continue;
        const EnemyStats& st = enemyStats(e.kind);
        e.hit += kDt;
        const float d = e.z - z;
        if (e.kind == kBoss) {
            if (d - st.radius <= r * 0.6f) {  // it reaches the squad: it stops and crushes soldiers until it falls
                // It crushes a ninth of the squad it meets every second (at least the level's `drain`), whatever its size:
                // the fight lasts seconds.
                if (bossRate <= 0.0f) bossRate = maxv(def->drain, float(squad) / 9.0f);
                bossDrain += bossRate * kDt;
                while (bossDrain >= 1.0f && squad > 0) {
                    bossDrain -= 1.0f;
                    --squad;
                    ++ev.lost;
                }
            } else {
                e.z -= st.speed * kDt;
                e.x += clampv(x - e.x, -1.0f, 1.0f) * 0.8f * kDt;
            }
            continue;
        }
        e.z -= st.speed * def->pace * kDt;
        if (d < 14.0f) e.x += clampv(x - e.x, -1.0f, 1.0f) * (e.kind == kRider ? 2.4f : 1.6f) * def->pace * kDt;
        if (d < r * 0.6f + st.radius && e.x > lo - st.radius && e.x < hi + st.radius) {
            squad -= st.strength;
            ev.lost += st.strength;
            if (e.wave >= 0 && e.wave < kMaxWaves) waveLost[e.wave] += st.strength;
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
    if (!started) {
        if (!go) return;
        started = true;
    }
    time += kDt;
    // Steer: the squad follows the finger, fast but not at once.
    const float r = radius();
    const float lim = kRoadHalf - minv(r * 0.5f, 1.6f);
    const float tx = clampv(steerX, -lim, lim);
    vx = clampv((tx - x) * 12.0f, -18.0f, 18.0f);
    x += vx * kDt;
    // March; halt before the boss.
    const float zPrev = z;
    const bool bossNear = boss_ >= 0 && enemies[boss_].z - z < 8.0f;
    if (!bossNear && !(def->bossHp == 0 && z >= def->length)) z += def->speed * kDt;
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
        if (x >= gd.x0 && x <= gd.x1) {
            ev.gate = g;
            ev.gateBefore = squad;
            if (gd.kind == kGateWeapon) {  // it hands over its weapon
                weapon = u8(clampv<i32>(gs.value, 0, kWeaponCount - 1));
                ev.weapon = weapon;
            } else {
                const i64 s = gd.kind == kGateMul ? i64(squad) * gs.value : i64(squad) + gs.value;
                squad = i32(clampv<i64>(s, 0, kSquadCap));
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
            const i32 cost = maxv<i32>(1, (bs.hp + kHp - 1) / kHp / 2);
            squad -= cost;
            ev.lost += cost;
            bs.broken = 1;
            ev.crashed = k;
            if (bd.weapon > 0 && bd.weapon < kWeaponCount) {  // a crate run into still hands over its weapon
                weapon = bd.weapon;
                ev.weapon = weapon;
            }
        }
    }
    fire(ev);
    moveBullets(ev);
    moveEnemies(ev);
    for (int g = 0; g < def->gateCount; ++g) gates[g].hit += kDt;
    for (int k = 0; k < def->barrelCount; ++k) barrels[k].hit += kDt;
    if (squad <= 0) {
        squad = 0;
        lost = true;
        ev.lostAll = true;
        return;
    }
    if (!won && def->bossHp == 0 && z >= def->length) {
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
