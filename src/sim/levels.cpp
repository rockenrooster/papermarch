// Levels (GAME_DESIGN 8): the compact text form, the baked table (src/sim/levels_baked.h, written by
// papermarch_headless --bake-levels), and names.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sim/level.h"
#include "sim/levels_hand.h"

namespace pm::sim {

namespace {
#include "sim/levels_baked.h"  // kBaked[], kBakedCount

LevelDef g_levels[400];
int g_count = 0;
bool g_loaded = false;

void load() {
    if (g_loaded) return;
    g_loaded = true;
    g_count = 0;
    for (int i = 0; i < kBakedCount && g_count < int(sizeof g_levels / sizeof g_levels[0]); ++i)
        if (decodeLevel(kBaked[i], g_levels[g_count])) ++g_count;
    if (g_count == 0) {  // before the first bake: the hand-made levels as they are written, solved here
        Solver solver;
        for (int n = 1; n <= 400 && g_count < int(sizeof g_levels / sizeof g_levels[0]); ++n)
            if (handLevel(n, g_levels[g_count])) {
                g_levels[g_count].best = solver.run(g_levels[g_count], start(g_levels[g_count])).best;
                ++g_count;
            }
    }
}

bool num(const char*& p, i64& v) {
    char* end = nullptr;
    v = std::strtoll(p, &end, 10);
    if (end == p) return false;
    p = end;
    return true;
}

bool expect(const char*& p, char c) {
    if (*p != c) return false;
    ++p;
    return true;
}

}  // namespace

int levelCount() {
    load();
    return g_count;
}

const LevelDef& level(int n) {
    load();
    if (g_count == 0) {
        static LevelDef empty;
        return empty;
    }
    if (n < 1) n = 1;
    if (n > g_count) n = g_count;
    return g_levels[n - 1];
}

bool decodeLevel(const char* text, LevelDef& out) {
    out = LevelDef();
    const char* p = text;
    i64 v = 0, a = 0, b = 0, c = 0;
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        const char tag = *p++;
        switch (tag) {
            case 'K': if (!num(p, v)) return false; out.kind = u8(v); break;
            case 'P': if (!num(p, v)) return false; out.recipe = u8(v); break;
            case 'W': if (!num(p, v)) return false; out.world = u8(v); break;
            case 'X': if (!num(p, v)) return false; out.boss = u8(v); break;
            case 'S': if (!num(p, v)) return false; out.start = i32(v); break;
            case 'V': if (!num(p, v)) return false; out.rivalStart = i32(v); break;
            case 'H': if (!num(p, v)) return false; out.pageH = i16(v); break;
            case 'E': if (!num(p, v)) return false; out.seed = u32(v); break;
            case 'B': if (!num(p, v)) return false; out.best = v; break;
            case 'T': if (!num(p, v)) return false; out.hint = u8(v); break;
            case 's': {
                if (out.siteCount >= kMaxSites) return false;
                Site& s = out.sites[out.siteCount++];
                if (!num(p, v) || !expect(p, ',') || !num(p, a) || !expect(p, ',') || !num(p, b) || !expect(p, ',') || !num(p, c))
                    return false;
                s.kind = u8(v);
                s.x = i16(a);
                s.y = i16(b);
                s.value = i32(c);
                while (*p == '/') {
                    ++p;
                    const char k = *p++;
                    if (k == 'f') {
                        if (!num(p, v)) return false;
                        s.fork = u8(v);
                    } else if (k == 'g') {
                        if (!num(p, v) || !expect(p, ',') || !num(p, a) || !expect(p, ',') || !num(p, b)) return false;
                        s.perTurn = i16(v);
                        s.lo = i32(a);
                        s.hi = i32(b);
                    } else if (k == 'u') {
                        if (!num(p, v) || !expect(p, ',') || !num(p, a)) return false;
                        s.until = i16(v);
                        s.aux = i32(a);
                    } else if (k == 'a') {
                        if (!num(p, v)) return false;
                        s.aux = i32(v);
                    } else {
                        return false;
                    }
                }
                break;
            }
            case 'r': {
                if (out.roadCount >= kMaxRoads) return false;
                Road& r = out.roads[out.roadCount++];
                if (!num(p, v) || !expect(p, ',') || !num(p, a)) return false;
                r.a = u8(v);
                r.b = u8(a);
                if (*p == '/') {
                    ++p;
                    if (!expect(p, 'l') || !num(p, b)) return false;
                    r.lock = u8(b);
                }
                break;
            }
            case 'w': {
                if (out.raidCount >= kMaxRaids) return false;
                Raid& r = out.raids[out.raidCount++];
                if (!num(p, v) || !expect(p, ',') || !num(p, a) || !expect(p, ',') || !num(p, b)) return false;
                r.turn = u8(v);
                r.entry = u8(a);
                r.strength = i32(b);
                break;
            }
            case 'o': {
                do {
                    if (out.routeLen >= kMaxRoute || !num(p, v)) return false;
                    out.route[out.routeLen++] = u8(v);
                } while (expect(p, ','));
                break;
            }
            default:
                return false;
        }
        if (*p && *p != ' ') return false;
    }
    if (out.siteCount < 2 || out.sites[0].kind != kHome) return false;
    for (int r = 0; r < out.roadCount; ++r)
        if (out.roads[r].a >= out.siteCount || out.roads[r].b >= out.siteCount) return false;
    return true;
}

int encodeLevel(const LevelDef& d, char* out, int cap) {
    int o = 0;
    auto put = [&](const char* fmt, auto... args) {
        if (o < 0) return;
        const int n = std::snprintf(out + o, size_t(cap - o), fmt, args...);
        if (n < 0 || n >= cap - o) o = -1;
        else o += n;
    };
    put("K%d P%d W%d X%d S%d V%d H%d E%u B%lld T%d", d.kind, d.recipe, d.world, d.boss, d.start, d.rivalStart, d.pageH, d.seed,
        (long long)d.best, d.hint);
    for (int i = 0; i < d.siteCount; ++i) {
        const Site& s = d.sites[i];
        put(" s%d,%d,%d,%d", s.kind, s.x, s.y, s.value);
        if (s.fork) put("/f%d", s.fork);
        if (s.perTurn) put("/g%d,%d,%d", s.perTurn, s.lo, s.hi);
        if (s.until >= 0) put("/u%d,%d", s.until, s.aux);
        else if (s.aux) put("/a%d", s.aux);
    }
    for (int r = 0; r < d.roadCount; ++r) {
        put(" r%d,%d", d.roads[r].a, d.roads[r].b);
        if (d.roads[r].lock != kNoLock) put("/l%d", d.roads[r].lock);
    }
    for (int r = 0; r < d.raidCount; ++r) put(" w%d,%d,%d", d.raids[r].turn, d.raids[r].entry, d.raids[r].strength);
    if (d.routeLen) {
        put(" o%d", d.route[0]);
        for (int k = 1; k < d.routeLen; ++k) put(",%d", d.route[k]);
    }
    return o < 0 ? 0 : o;
}

const char* kindName(u8 kind) {
    switch (kind) {
        case kHome: return "HOME";
        case kCamp: return "CAMP";
        case kVillage: return "VILLAGE";
        case kShrine: return "SHRINE";
        case kPost: return "POST";
        case kMire: return "MIRE";
        case kDepot: return "DEPOT";
        case kKey: return "KEY TOWER";
        case kKeep: return "KEEP";
        case kRivalCamp: return "WARLORD";
        default: return "?";
    }
}

const char* objective(const LevelDef& d) {
    switch (d.kind) {
        case kGauntlet: return "RUN THE GAUNTLET";
        case kSiege: return "BREAK THE SIEGE";
        case kRace: return "OUTMARCH THE WARLORD";
        case kLastStand: return "HOLD THE LINE";
        case kWaves: return "WAVE BATTLE";
        default: return "TAKE THE KEEP";
    }
}

}  // namespace pm::sim
