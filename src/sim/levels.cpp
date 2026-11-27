// Levels: the compact text form (the bake writes it, the app reads it), the baked table and the hint lines.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sim/battle.h"

namespace pm::sim {

namespace {
#include "sim/levels_baked.h"  // kBaked[], kBakedCount

// Decoded once at start (a LevelDef is large: a road of 15 minutes; as a static array its member defaults would sit in
// the binary, megabytes of it).
LevelDef* g_levels = nullptr;
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
    "GRAB A NEW WEAPON",                 // 12: the first weapon gate (docs/CONTENT_PASS.md 4.2)
    "SHOOT CRATES OPEN",                 // 13: the first weapon crate
    "THE LANCE GOES THROUGH",            // 14
    "ROCKETS BLAST CROWDS",              // 15
    "TOUCH MEDALS FOR POWER",            // 16: the first medallion (rapid fire)
    "THE SHIELD SAVES SOLDIERS",         // 17
    "CALL THE AIRSTRIKE",                // 18: the first supply crate
    "POWER SHOT: DOUBLE DAMAGE",         // 19
    "FREEZE THE HORDE",                  // 20
    "SHOOT MINES NEAR THE HORDE",        // 21: mines (13)
    "BONUS ROAD: GRAB IT ALL",           // 22: every gold rush level
    "ARCHERS SHOOT BACK",                // 23: archers (17)
    "GATES ON THE MOVE",                 // 24: moving gates (19)
    "FREE THE PRISONERS",                // 25: every rescue level
    "ARSENAL: A WEAPON FROM THE START",  // 26: every arsenal level
    "DODGE THE RED CIRCLE",              // 27: catapults (21)
    "A GIANT BLOCKS THE ROAD",           // 28: giants (29)
    "DODGE THE CLUB",                    // 29: the boss's club slam (30)
    "WATCH THE SKY",                     // 30: its boulders (50)
    "THE OGRE CALLS FOR HELP",           // 31: its war cry (60)
    "FIND THE GAP",                      // 32: its stomp (70)
    "SHIELDS STOP BULLETS",              // 33: shield bearers (26)
    "MORTARS FLY OVER WALLS",            // 34: the mortar (36)
    "SHOOT BOMBERS IN THE CROWD",        // 35: bombers (37)
    "RUN FOR IT",                        // 36: quick march (38)
    "GET CLOSE AND BURN",                // 37: the flamethrower (41)
    "SILENCE THE DRUMMER",               // 38: drummers (42)
    "THE MAGNET OPENS BARRELS",          // 39: the magnet (44)
    "HOLD THE LINE",                     // 40: reinforcements (46)
    "LINE UP THE CANNON",                // 41: the cannon (48)
    "GIANTS STOMP THE HORDE",            // 42: giants, the power-up (49)
    "LIGHTNING JUMPS",                   // 43: the storm coil (53)
    "SLOW THEM DOWN",                    // 44: the hourglass (54)
    "NEXT GATE COUNTS TWICE",            // 45: double gates (58)
    "A CANNON JOINS YOU",                // 46: the field gun (64)
    "LEAVE THE MARKED LANE",             // 47: rollers (27)
    "BREAK THE WALL",                    // 48: every siege level (from 33)
    "DODGE THE ROLLERS",                 // 49: every gauntlet (from 34)
    "RUN FOR IT",                        // 50: every rush (from 38)
    "TOPPLE THE GIANTS",                 // 51: every giants' road (from 49)
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
    g_levels = new LevelDef[kBakedCount > 0 ? kBakedCount : 1];
    for (int i = 0; i < kBakedCount; ++i)
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

const char* hintText(int hint) { return hint > 0 && hint < kHintCount && kHints[hint] ? kHints[hint] : ""; }

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
            case 'D': if (!num(p, v[0])) return false; out.drain = float(v[0]) / 10.0f; break;
            case 'P': if (!num(p, v[0])) return false; out.planLook = float(v[0]) / 10.0f; break;
            case 'G': if (!num(p, v[0]) || v[0] < 0 || v[0] >= kWeaponCount) return false; out.gun = u8(v[0]); break;
            case 'M': if (!num(p, v[0]) || v[0] < 0 || v[0] >= kModeCount) return false; out.mode = u8(v[0]); break;
            case 'J': if (!num(p, v[0]) || v[0] < 0 || v[0] > 15) return false; out.bossAttacks = u8(v[0]); break;
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
            case 'g': {  // x0, x1, z, value, kind, grows, cap[, moves]
                if (out.gateCount >= kMaxGates || !nums(p, v, 7)) return false;
                v[7] = 0;
                if (*p == ',') {
                    ++p;
                    if (!num(p, v[7])) return false;
                }
                out.gates[out.gateCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f, float(v[2]) / 10.0f, i32(v[3]), u8(v[4]), u8(v[5]),
                                              i32(v[6]), u8(v[7])};
                break;
            }
            case 'n': {  // a mine: x, z
                if (out.mineCount >= kMaxMines || !nums(p, v, 2)) return false;
                out.mines[out.mineCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f};
                break;
            }
            case 'k': {  // a cage: x, z, prisoners
                if (out.cageCount >= kMaxCages || !nums(p, v, 3)) return false;
                out.cages[out.cageCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f, i32(v[2])};
                break;
            }
            case 'b': {
                if (out.barrelCount >= kMaxBarrels || !nums(p, v, 4)) return false;
                out.barrels[out.barrelCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f, i32(v[2]), i32(v[3])};
                break;
            }
            case 'c': {  // a weapon crate: x, z, hit points, weapon
                if (out.barrelCount >= kMaxBarrels || !nums(p, v, 4) || v[3] < 1 || v[3] >= kWeaponCount) return false;
                out.barrels[out.barrelCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f, i32(v[2]), 0, u8(v[3])};
                break;
            }
            case 's': {  // a supply crate: x, z, hit points, power-up
                if (out.barrelCount >= kMaxBarrels || !nums(p, v, 4) || v[3] < 1 || v[3] >= kPowerCount) return false;
                out.barrels[out.barrelCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f, i32(v[2]), 0, 0, u8(v[3])};
                break;
            }
            case 'm': {  // a medallion: x, z, power-up
                if (out.medalCount >= kMaxMedals || !nums(p, v, 3) || v[2] < 1 || v[2] >= kPowerCount) return false;
                out.medals[out.medalCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f, u8(v[2])};
                break;
            }
            case 'o': {  // a roller: x, z
                if (out.rollerCount >= kMaxRollers || !nums(p, v, 2)) return false;
                out.rollers[out.rollerCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f};
                break;
            }
            case 'l': {  // a fort wall: z, hit points
                if (out.wallCount >= kMaxWalls || !nums(p, v, 2)) return false;
                out.walls[out.wallCount++] = {float(v[0]) / 10.0f, i32(v[1])};
                break;
            }
            case 'w': {  // z, x0, x1, count, kind[, hit points each]
                if (out.waveCount >= kMaxWaves || !nums(p, v, 5) || v[4] < 0 || v[4] >= kEnemyKindCount) return false;
                v[5] = 0;
                if (*p == ',') {
                    ++p;
                    if (!num(p, v[5])) return false;
                }
                out.waves[out.waveCount++] = {float(v[0]) / 10.0f, float(v[1]) / 10.0f, float(v[2]) / 10.0f, i32(v[3]), u8(v[4]), i32(v[5])};
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
    if (int(d.drain * 10.0f + 0.5f) != 220) put(" D%d", int(d.drain * 10.0f + 0.5f));
    if (d.planLook > 0) put(" P%d", t10(d.planLook));
    if (d.gun != kMusket) put(" G%d", d.gun);
    if (d.mode != kModeMarch) put(" M%d", d.mode);
    if (d.bossAttacks) put(" J%d", d.bossAttacks);
    if (d.planCount > 0) {
        put(" Q%d", d.plan[0]);
        for (int i = 1; i < d.planCount; ++i) put(",%d", d.plan[i]);
    }
    for (int i = 0; i < d.gateCount; ++i) {
        const GateDef& g = d.gates[i];
        put(" g%d,%d,%d,%d,%d,%d,%d", t10(g.x0), t10(g.x1), t10(g.z), g.value, g.kind, g.grows, g.cap);
        if (g.moves) put(",%d", g.moves);
    }
    for (int i = 0; i < d.barrelCount; ++i) {
        const BarrelDef& b = d.barrels[i];
        if (b.weapon > 0) put(" c%d,%d,%d,%d", t10(b.x), t10(b.z), b.hp, b.weapon);
        else if (b.power > 0) put(" s%d,%d,%d,%d", t10(b.x), t10(b.z), b.hp, b.power);
        else put(" b%d,%d,%d,%d", t10(b.x), t10(b.z), b.hp, b.reward);
    }
    for (int i = 0; i < d.medalCount; ++i) put(" m%d,%d,%d", t10(d.medals[i].x), t10(d.medals[i].z), d.medals[i].power);
    for (int i = 0; i < d.mineCount; ++i) put(" n%d,%d", t10(d.mines[i].x), t10(d.mines[i].z));
    for (int i = 0; i < d.cageCount; ++i) put(" k%d,%d,%d", t10(d.cages[i].x), t10(d.cages[i].z), d.cages[i].reward);
    for (int i = 0; i < d.rollerCount; ++i) put(" o%d,%d", t10(d.rollers[i].x), t10(d.rollers[i].z));
    for (int i = 0; i < d.wallCount; ++i) put(" l%d,%d", t10(d.walls[i].z), d.walls[i].hp);
    for (int i = 0; i < d.waveCount; ++i) {
        const WaveDef& w = d.waves[i];
        put(" w%d,%d,%d,%d,%d", t10(w.z), t10(w.x0), t10(w.x1), w.count, w.kind);
        if (w.hp > 0) put(",%d", w.hp);
    }
    return o < 0 ? 0 : o;
}

}  // namespace pm::sim
