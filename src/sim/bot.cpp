#include "sim/bot.h"

#include <cmath>

namespace pm::sim {

namespace {
// What passing a gate leaves the squad, as the bot sees it: a weapon is worth a share of the squad over the musket
// (a rough guess; the planner tries every gate of a row and keeps what really works).
i64 gateWorth(const Battle& b, const GateDef& gd, const GateState& gs) {
    if (gd.kind == kGateMul) return i64(b.squad) * gs.value;
    if (gd.kind == kGateWeapon) {
        static const float kWorth[kWeaponCount] = {0.0f, 0.15f, 0.1f, 0.2f, 0.25f};
        const float gain = kWorth[clampv<i32>(gs.value, 0, kWeaponCount - 1)] - kWorth[b.weapon < kWeaponCount ? b.weapon : 0];
        return i64(float(b.squad) * (1.0f + gain));
    }
    return i64(b.squad) + gs.value;
}
}  // namespace

float botSteer(const Battle& b) {
    const LevelDef& d = *b.def;
    const float r = b.radius();
    const float look = d.planLook > 0 ? d.planLook : 15.0f;
    // 1. The next row of gates (within `look` units): the plan's gate, else the one that leaves the biggest squad, or a
    // gap if every gate hurts.
    float rowZ = 1e9f;
    for (int g = 0; g < d.gateCount; ++g) {
        const GateState& gs = b.gates[g];
        if (gs.used || gs.passed || d.gates[g].z <= b.z || d.gates[g].z - b.z > look) continue;
        rowZ = minv(rowZ, d.gates[g].z);
    }
    if (rowZ < 1e8f) {
        for (int ri = 0; ri < d.rowCount && ri < d.planCount; ++ri) {
            if (std::fabs(d.rowZ[ri] - rowZ) > 0.05f || d.plan[ri] < 0) continue;
            int idx[8], k = 0;
            for (int g = 0; g < d.gateCount && k < 8; ++g)
                if (std::fabs(d.gates[g].z - rowZ) < 0.05f) {
                    int at = k++;  // in order from the left
                    while (at > 0 && d.gates[idx[at - 1]].x0 > d.gates[g].x0) {
                        idx[at] = idx[at - 1];
                        --at;
                    }
                    idx[at] = g;
                }
            if (d.plan[ri] < k) {
                const GateDef& gd = d.gates[idx[int(d.plan[ri])]];
                return (gd.x0 + gd.x1) * 0.5f;
            }
        }
        i64 best = b.squad;  // passing no gate at all
        float bestX = b.x;
        bool gateBest = false;
        float covered0[8], covered1[8];
        int nc = 0;
        for (int g = 0; g < d.gateCount; ++g) {
            const GateDef& gd = d.gates[g];
            const GateState& gs = b.gates[g];
            if (gs.used || gs.passed || std::fabs(gd.z - rowZ) > 0.5f) continue;
            const i64 after = gateWorth(b, gd, gs);
            if (nc < 8) {
                covered0[nc] = gd.x0;
                covered1[nc] = gd.x1;
                ++nc;
            }
            if (after > best || (!gateBest && after >= best)) {
                best = after;
                bestX = (gd.x0 + gd.x1) * 0.5f;
                gateBest = true;
            }
        }
        if (!gateBest || best < b.squad) {
            // Every gate here hurts: find a gap in the row, if there is one.
            for (float gx = -4.5f; gx <= 4.5f; gx += 0.5f) {
                bool inside = false;
                for (int k = 0; k < nc; ++k) inside = inside || (gx >= covered0[k] - 0.2f && gx <= covered1[k] + 0.2f);
                if (!inside) return gx;
            }
        }
        if (gateBest) return bestX;
    }
    // 2. A barrel to shoot open while no enemy is close; never run into one.
    float nearestEnemy = 1e9f, sumX = 0, sumW = 0;
    for (int i = 0; i < b.enemyCount; ++i) {
        const Enemy& e = b.enemies[i];
        if (!e.alive) continue;
        const float dz = e.z - b.z;
        if (dz < -1.0f || dz > 32.0f) continue;
        nearestEnemy = minv(nearestEnemy, dz);
        const float w = 1.0f / (1.0f + dz * 0.2f) * (e.kind == kBoss ? 20.0f : 1.0f);
        sumX += e.x * w;
        sumW += w;
    }
    for (int k = 0; k < d.barrelCount; ++k) {
        const BarrelDef& bd = d.barrels[k];
        if (b.barrels[k].broken) continue;
        const float dz = bd.z - b.z;
        if (dz < -1.0f) continue;
        if (dz < 4.0f && std::fabs(bd.x - b.x) < 0.9f + r * 0.6f + 0.4f) {  // about to hit it: step aside
            const float side = bd.x >= 0 ? bd.x - (1.6f + r * 0.6f) : bd.x + (1.6f + r * 0.6f);
            return side;
        }
        if (dz < 26.0f && nearestEnemy > 12.0f) return bd.x;
    }
    // 3. The horde: aim at it, the nearest weighing most.
    if (sumW > 0) return sumX / sumW;
    return b.x * 0.98f;
}

PlayResult playLevel(const LevelDef& d, int maxSeconds) {
    thread_local Battle* bp = nullptr;  // large: one per thread, made once
    if (!bp) bp = new Battle();
    bp->init(d);
    BattleEvents ev;
    const int steps = maxSeconds * 60;
    for (int t = 0; t < steps && !bp->won && !bp->lost; ++t) bp->tick(botSteer(*bp), true, ev);
    PlayResult r;
    r.won = bp->won;
    r.left = bp->squad;
    r.peak = bp->peak;
    r.progress = bp->progress();
    return r;
}

bool botPlay(const LevelDef& d, i32* left, i32* peak, int maxSeconds) {
    const PlayResult r = playLevel(d, maxSeconds);
    if (left) *left = r.left;
    if (peak) *peak = r.peak;
    return r.won;
}

namespace {

bool better(const PlayResult& a, const PlayResult& b) {
    if (a.won != b.won) return a.won;
    if (a.won) return a.left > b.left;
    if (std::fabs(a.progress - b.progress) > 1e-4f) return a.progress > b.progress;
    return a.left > b.left;
}

}  // namespace

PlayResult planLevel(LevelDef& d) {
    // How far ahead to line up with a row of gates: the everyday 15 units, or earlier, to shoot the chosen gate up.
    static const float kLooks[] = {0.0f, 26.0f, 42.0f};
    thread_local LevelDef* wp = nullptr;  // large: one per thread
    if (!wp) wp = new LevelDef();
    LevelDef& w = *wp;
    w = d;
    w.planCount = w.rowCount;
    for (int i = 0; i < kMaxRows; ++i) w.plan[i] = -1;
    PlayResult best;
    float bestLook = 0;
    bool have = false;
    for (float look : kLooks) {
        w.planLook = look;
        const PlayResult r = playLevel(w);
        if (!have || better(r, best)) {
            best = r;
            bestLook = look;
            have = true;
        }
    }
    w.planLook = bestLook;
    // Each row of choices in turn, the rows before it as chosen, the ones after left to the bot.
    for (int ri = 0; ri < w.rowCount; ++ri) {
        int k = 0;
        for (int g = 0; g < w.gateCount; ++g) k += std::fabs(w.gates[g].z - w.rowZ[ri]) < 0.05f;
        int keep = -1;
        for (int c = 0; c < minv(k, 8); ++c) {
            w.plan[ri] = i8(c);
            const PlayResult r = playLevel(w);
            if (better(r, best)) {
                best = r;
                keep = c;
            }
        }
        w.plan[ri] = i8(keep);
    }
    // The other looks once more with the rows chosen.
    for (float look : kLooks) {
        if (look == bestLook) continue;
        w.planLook = look;
        const PlayResult r = playLevel(w);
        if (better(r, best)) {
            best = r;
            bestLook = look;
        }
    }
    d.planLook = bestLook;
    d.planCount = 0;
    for (int i = 0; i < w.rowCount; ++i)
        if (w.plan[i] >= 0) d.planCount = u8(i + 1);  // the choices up to the last one made
    for (int i = 0; i < kMaxRows; ++i) d.plan[i] = w.plan[i];
    return best;
}

}  // namespace pm::sim
