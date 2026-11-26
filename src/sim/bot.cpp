#include "sim/bot.h"

#include <cmath>

namespace pm::sim {

float botSteer(const Battle& b) {
    const LevelDef& d = *b.def;
    const float r = b.radius();
    // 1. The next row of gates (within 15 units): the one that leaves the biggest squad, or a gap if every gate hurts.
    float rowZ = 1e9f;
    for (int g = 0; g < d.gateCount; ++g) {
        const GateState& gs = b.gates[g];
        if (gs.used || gs.passed || d.gates[g].z <= b.z || d.gates[g].z - b.z > 15.0f) continue;
        rowZ = minv(rowZ, d.gates[g].z);
    }
    if (rowZ < 1e8f) {
        i64 best = b.squad;  // passing no gate at all
        float bestX = b.x;
        bool gateBest = false;
        float covered0[8], covered1[8];
        int nc = 0;
        for (int g = 0; g < d.gateCount; ++g) {
            const GateDef& gd = d.gates[g];
            const GateState& gs = b.gates[g];
            if (gs.used || gs.passed || std::fabs(gd.z - rowZ) > 0.5f) continue;
            const i64 after = gd.kind == kGateMul ? i64(b.squad) * gs.value : i64(b.squad) + gs.value;
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

bool botPlay(const LevelDef& d, i32* left, i32* peak, int maxSeconds) {
    thread_local Battle* bp = nullptr;  // large: one per thread, made once
    if (!bp) bp = new Battle();
    bp->init(d);
    BattleEvents ev;
    const int steps = maxSeconds * 60;
    for (int t = 0; t < steps && !bp->won && !bp->lost; ++t) bp->tick(botSteer(*bp), true, ev);
    if (left) *left = bp->squad;
    if (peak) *peak = bp->peak;
    return bp->won;
}

}  // namespace pm::sim
