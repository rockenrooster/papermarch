// Levels: the compact text form (the bake writes it, the app reads it), the baked table and the hint lines.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sim/battle.h"

namespace pm::sim {

namespace {
#include "sim/levels_baked.h"  // kBaked[], kBakedCount

LevelDef g_levels[320];
int g_count = 0;
bool g_loaded = false;

const char* const kHints[kHintCount] = {
    "",
    "DRAG TO STEER YOUR SQUAD",          // 1
    "MARCH THROUGH THE BLUE GATES",      // 2
    "SHOOT A GATE TO GROW IT",           // 3
    "SHOOT BARRELS TO FREE RECRUITS",    // 4
    ("\xC3\x97" "2 DOUBLES YOUR SQUAD"), // 5
    "SHOOT RED BARRICADES UP",           // 6
    "RIDERS CHARGE FAST",                // 7
    "BRING DOWN THE BOSS",               // 8
    "BRUTES TAKE MANY HITS",             // 9
    "DON'T RUN INTO BARRELS",            // 10
    "HARD LEVEL! EVERY SOLDIER COUNTS",  // 11 (kHintHard)
    "", "", "", "",
};

// Until the first bake: one small level, so the app always has something to play.
void fallback(LevelDef& d) {
    d = LevelDef();
    d.start = 3;
    d.length = 90;
    d.speed = 5;
    d.hint = 1;
    for (int i = 0; i < 12; ++i) d.gates[d.gateCount++] = {-4.8f, -2.4f, 14.0f + 1.4f * float(i), 1, kGateAdd, 1, 3};
    d.gates[d.gateCount++] = {-4.6f, -0.2f, 40.0f, 5, kGateAdd, 1, 15};
    d.gates[d.gateCount++] = {0.2f, 4.6f, 40.0f, 2, kGateMul, 0, 2};
    d.barrels[d.barrelCount++] = {2.5f, 52.0f, 12, 12};
    d.waves[d.waveCount++] = {66.0f, -3.5f, 3.5f, 14, kFoot};
    d.star2 = 10;
    d.star3 = 20;
}

void load() {
    if (g_loaded) return;
    g_loaded = true;
    for (int i = 0; i < kBakedCount && g_count < int(sizeof g_levels / sizeof g_levels[0]); ++i)
        if (decodeLevel(kBaked[i], g_levels[g_count])) ++g_count;
    if (g_count == 0) {
        fallback(g_levels[0]);
        g_count = 1;
    }
}

bool num(const char*& p, i64& v) {
    char* end = nullptr;
    v = std::strtoll(p, &end, 10);
    if (end == p) return false;
    p = end;
    return true;
}

bool nums(const char*& p, i64* out, int n) {
    for (int i = 0; i < n; ++i) {
        if (i > 0) {
            if (*p != ',') return false;
            ++p;
        }
        if (!num(p, out[i])) return false;
    }
    return true;
}

}  // namespace

const char* hintText(int hint) { return hint > 0 && hint < kHintCount ? kHints[hint] : ""; }

void indexRows(LevelDef& d) {
    d.rowCount = 0;
    for (int i = 0; i < d.gateCount; ++i) {
        const float z = d.gates[i].z;
        int same = 0;
        for (int j = 0; j < d.gateCount; ++j) same += std::fabs(d.gates[j].z - z) < 0.05f;
        if (same < 2) continue;
        bool known = false;
        for (int r = 0; r < d.rowCount; ++r) known = known || std::fabs(d.rowZ[r] - z) < 0.05f;
        if (known || d.rowCount >= kMaxRows) continue;
        int at = d.rowCount++;  // kept in order up the road
        while (at > 0 && d.rowZ[at - 1] > z) {
            d.rowZ[at] = d.rowZ[at - 1];
            --at;
        }
        d.rowZ[at] = z;
    }
}

int levelCount() {
    load();
    return g_count;
}

const LevelDef& level(int n) {
    load();
    return g_levels[clampv(n, 1, g_count) - 1];
}

bool decodeLevel(const char* text, LevelDef& out) {
    out = LevelDef();
    const char* p = text;
    i64 v[8];
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        const char tag = *p++;
        switch (tag) {
            case 'S': if (!num(p, v[0])) return false; out.start = i32(v[0]); break;
            case 'L': if (!num(p, v[0])) return false; out.length = float(v[0]) / 10.0f; break;
            case 'V': if (!num(p, v[0])) return false; out.speed = float(v[0]) / 100.0f; break;
            case 'B': if (!num(p, v[0])) return false; out.bossHp = i32(v[0]); break;
            case 'W': if (!num(p, v[0])) return false; out.world = u8(v[0]); break;
            case 'X': if (!num(p, v[0])) return false; out.boss = u8(v[0]); break;
            case 'T': if (!num(p, v[0])) return false; out.hint = u8(v[0]); break;
            case 'E': if (!num(p, v[0])) return false; out.seed = u32(v[0]); break;
            case 'A': if (!num(p, v[0])) return false; out.star2 = i32(v[0]); break;
            case 'C': if (!num(p, v[0])) return false; out.star3 = i32(v[0]); break;
            case 'R': if (!num(p, v[0])) return false; out.botEnd = i32(v[0]); break;
            case 'K': if (!num(p, v[0])) return false; out.spike = u8(v[0]); break;
            case 'Y': if (!num(p, v[0])) return false; out.pace = float(v[0]) / 100.0f; break;
            case 'P': if (!num(p, v[0])) return false; out.planLook = float(v[0]) / 10.0f; break;
            case 'Q': {  // the plan's choices, one a row: Q-1,0,1
                out.planCount = 0;
                for (;;) {
                    if (!num(p, v[0])) return false;
                    if (out.planCount < kMaxRows) out.plan[out.planCount++] = i8(v[0]);
                    if (*p != ',') break;
                    ++p;
                }
                break;
            }
            case 'g': {
                if (out.gateCount >= kMaxGates || !nums(p, v, 7)) return false;
                out.gates[out.gateCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f, float(v[2]) / 10.0f, i32(v[3]), u8(v[4]), u8(v[5]),
                                              i32(v[6])};
                break;
            }
            case 'b': {
                if (out.barrelCount >= kMaxBarrels || !nums(p, v, 4)) return false;
                out.barrels[out.barrelCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f, i32(v[2]), i32(v[3])};
                break;
            }
            case 'w': {
                if (out.waveCount >= kMaxWaves || !nums(p, v, 5)) return false;
                out.waves[out.waveCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f, float(v[2]) / 10.0f, i32(v[3]), u8(v[4])};
                break;
            }
            default:
                return false;
        }
        if (*p && *p != ' ') return false;
    }
    indexRows(out);
    return out.start > 0 && out.length > 10;
}

int encodeLevel(const LevelDef& d, char* out, int cap) {
    int o = 0;
    auto put = [&](const char* fmt, auto... args) {
        if (o < 0) return;
        const int n = std::snprintf(out + o, size_t(cap - o), fmt, args...);
        if (n < 0 || n >= cap - o) o = -1;
        else o += n;
    };
    auto t10 = [](float f) { return int(f * 10.0f + (f >= 0 ? 0.5f : -0.5f)); };
    put("S%d L%d V%d B%d W%d X%d T%d E%u A%d C%d R%d", d.start, t10(d.length), int(d.speed * 100.0f + 0.5f), d.bossHp, d.world, d.boss,
        d.hint, d.seed, d.star2, d.star3, d.botEnd);
    if (d.spike) put(" K%d", d.spike);
    if (int(d.pace * 100.0f + 0.5f) != 100) put(" Y%d", int(d.pace * 100.0f + 0.5f));
    if (d.planLook > 0) put(" P%d", t10(d.planLook));
    if (d.planCount > 0) {
        put(" Q%d", d.plan[0]);
        for (int i = 1; i < d.planCount; ++i) put(",%d", d.plan[i]);
    }
    for (int i = 0; i < d.gateCount; ++i) {
        const GateDef& g = d.gates[i];
        put(" g%d,%d,%d,%d,%d,%d,%d", t10(g.x0), t10(g.x1), t10(g.z), g.value, g.kind, g.grows, g.cap);
    }
    for (int i = 0; i < d.barrelCount; ++i) {
        const BarrelDef& b = d.barrels[i];
        put(" b%d,%d,%d,%d", t10(b.x), t10(b.z), b.hp, b.reward);
    }
    for (int i = 0; i < d.waveCount; ++i) {
        const WaveDef& w = d.waves[i];
        put(" w%d,%d,%d,%d,%d", t10(w.z), t10(w.x0), t10(w.x1), w.count, w.kind);
    }
    return o < 0 ? 0 : o;
}

}  // namespace pm::sim
