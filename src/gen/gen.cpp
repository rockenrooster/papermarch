// The generator (GAME_DESIGN 8.3) and grading (8.4): recipes place sites and roads, a forward walk along an intended
// order sets the numbers, traps make the obvious order worse, and the solver and the plays grade the result.
#include "gen/gen.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/rng.h"
#include "sim/levels_hand.h"

namespace pm::gen {

namespace {
inline u32 bit(int i) { return 1u << u32(i); }

}  // namespace

// ---- Plays -------------------------------------------------------------------------------------------------------------

int greedyMove(const sim::LevelDef& d, const sim::State& st) {
    // The naive player (GAME_DESIGN 8.4): whatever gives the most soldiers now.
    const u32 moves = sim::takeable(d, st);
    int best = -1;
    i64 bestS = -1;
    for (int m = 0; m <= sim::kRivalMove; ++m) {
        if (!(moves & bit(m))) continue;
        const i64 after = sim::effectOf(d, st, m);
        if (after > bestS) {
            bestS = after;
            best = m;
        }
    }
    return best;
}

bool greedyWins(const sim::LevelDef& d, i64* final) {
    sim::State st = sim::start(d);
    for (int guard = 0; guard < 200 && !st.over(); ++guard) {
        const int m = greedyMove(d, st);
        if (m < 0) break;
        sim::take(d, st, m, nullptr);
    }
    if (final) *final = st.s;
    return st.won();
}

PlayStats randomPlays(const sim::LevelDef& d, int plays, u64 seed) {
    PlayStats out;
    Rng rng(seed, 77);
    for (int p = 0; p < plays; ++p) {
        sim::State st = sim::start(d);
        for (int guard = 0; guard < 200 && !st.over(); ++guard) {
            const u32 moves = sim::takeable(d, st);
            if (!moves) break;
            int list[32], n = 0;
            for (int m = 0; m <= sim::kRivalMove; ++m)
                if (moves & bit(m)) list[n++] = m;
            sim::take(d, st, list[rng.next() % u32(n)], nullptr);
        }
        ++out.plays;
        if (st.won()) {
            ++out.wins;
            if (st.s >= d.best) ++out.threeStars;
        }
    }
    return out;
}

bool hasDecision(const sim::LevelDef& d, sim::Solver& solver) {
    sim::State st = sim::start(d);
    for (int guard = 0; guard < 60 && !st.over(); ++guard) {
        const sim::Solve here = solver.run(d, st);
        if (!here.winnable || here.first < 0) return false;
        const u32 moves = sim::takeable(d, st);
        int n = 0, keep = 0;
        for (int m = 0; m <= sim::kRivalMove; ++m) {
            if (!(moves & bit(m))) continue;
            ++n;
            sim::State c = st;
            sim::take(d, c, m, nullptr);
            const i64 b = c.won() ? c.s : (c.lost() ? 0 : solver.run(d, c).best);
            if (b >= here.best) ++keep;
        }
        if (n >= 2 && keep < n) return true;
        sim::take(d, st, here.first, nullptr);
    }
    return false;
}

u64 fingerprint(const sim::LevelDef& d) {
    // Site count, the degree of each site and its kind, in breadth-first order from Home.
    int order[sim::kMaxSites], dist[sim::kMaxSites];
    for (int i = 0; i < sim::kMaxSites; ++i) dist[i] = -1;
    int qh = 0, qt = 0;
    order[qt++] = 0;
    dist[0] = 0;
    while (qh < qt) {
        const int u = order[qh++];
        for (int r = 0; r < d.roadCount; ++r) {
            const int v = d.roads[r].a == u ? d.roads[r].b : (d.roads[r].b == u ? d.roads[r].a : -1);
            if (v >= 0 && dist[v] < 0) {
                dist[v] = dist[u] + 1;
                order[qt++] = v;
            }
        }
    }
    u64 h = hash64(u64(d.siteCount) * 31 + d.kind);
    for (int k = 0; k < qt; ++k) {
        const int s = order[k];
        int deg = 0;
        for (int r = 0; r < d.roadCount; ++r) deg += (d.roads[r].a == s) + (d.roads[r].b == s);
        h = hash64(h ^ (u64(d.sites[s].kind) << 8) ^ (u64(deg) << 16) ^ (u64(dist[s]) << 24) ^ (u64(d.sites[s].fork != 0) << 32));
    }
    return h;
}

// ---- The calendar (GAME_DESIGN 8.2): every level is a wave battle (the owner's choice, 2026-11-26) -----------------------

const char* recipeName(int r) {
    static const char* const names[kRecipeCount] = {"TWO LANES", "THREE LANES", "CHOKEPOINTS"};
    return r >= 0 && r < kRecipeCount ? names[r] : "HAND";
}

Slot calendar(int n) {
    Slot s;
    s.world = u8((n - 1) / 10 % 10 + 1);
    s.boss = n % 10 == 0;
    s.kind = sim::kWaves;
    // Introductions (GAME_DESIGN 5.2): the level that first shows a thing, with its hint line.
    switch (n) {
        case 11: s.focus = kFocusThreeLanes; s.hint = 9; break;
        case 18: s.focus = kFocusSwamp; s.hint = 10; break;
        case 22: s.focus = kFocusChoke; s.hint = 11; break;
        case 26: s.focus = kFocusTriple; s.hint = 12; break;
        case 34: s.focus = kFocusDouble; s.hint = 13; break;
        case 46: s.focus = kFocusKey; s.hint = 14; break;
        case 56: s.focus = kFocusDepot; s.hint = 15; break;
        default: break;
    }
    return s;
}

Allowed allowedAt(int n) {
    Allowed a;
    a.village = true;
    a.shrine = n >= 3;
    a.post = n >= 6;
    a.fork = true;
    a.three = n >= 11;
    a.mire = n >= 18;
    a.choke = n >= 22;
    a.triple = n >= 26;
    a.twin = n >= 34;
    a.key = n >= 46;
    a.depot = n >= 56;
    return a;
}

// ---- Numbers ---------------------------------------------------------------------------------------------------------

namespace {

i64 friendly(i64 v, bool down) {
    if (v < 30) return maxv<i64>(1, v);
    const i64 step = v < 300 ? 5 : (v < 3000 ? 10 : (v < 30000 ? 50 : 100));
    return down ? v / step * step : (v + step - 1) / step * step;
}

struct Band {
    int randomLo, randomHi;   // random play's win share, %
    int threeLo, threeHi;     // random play reaching three stars, %
};
Band bandFor(int n) {
    if (n <= 30) return {60, 100, 2, 45};
    if (n <= 80) return {40, 85, 1, 25};
    if (n <= 160) return {22, 70, 0, 12};
    return {10, 55, 0, 6};
}

float uni(Rng& r, float lo, float hi) { return lo + (hi - lo) * r.f01(); }

}  // namespace

// A wave battle (GAME_DESIGN 6): the army at the bottom, rows of choices up the field in two or three lanes (a lane leads
// on only where the tracks go), an enemy wave after most rows, the castle last. A forward walk along an intended route sets
// every number from the army that meets it; the solver then finds the true best.
bool generate(int n, const Slot& slot, int recipe, u64 seed, sim::LevelDef& out) {
    Rng r(seed, u64(n) * 2 + 1);
    const Allowed al = allowedAt(n);
    if (slot.focus == kFocusThreeLanes) recipe = kThreeLanes;
    if (slot.focus == kFocusChoke) recipe = kChokepoints;
    if (recipe == kThreeLanes && !al.three) recipe = kTwoLanes;
    if (recipe == kChokepoints && !al.choke) recipe = kTwoLanes;
    const int lanes = recipe == kThreeLanes ? 3 : 2;
    int rows = n <= 15 ? r.rangei(3, 4) : n <= 40 ? r.rangei(4, 5) : n <= 100 ? r.rangei(4, 6) : n <= 200 ? r.rangei(5, 7) : r.rangei(6, 8);
    if (slot.boss) rows = minv(rows + 1, 8);
    if (lanes == 3) rows = minv(rows, 7);  // 3 x 7 + Home + castle stays under the site limit with room
    const float kRow = 40.0f;
    const float H = 96.0f + float(rows) * kRow;
    sim::LevelBuilder b;
    b.d.kind = sim::kWaves;
    b.d.boss = slot.boss;
    b.d.world = slot.world;
    b.d.recipe = u8(recipe);
    b.d.pageH = i16(H + 0.5f);
    b.d.seed = u32(seed);
    b.d.hint = slot.hint;
    const int home = b.site(sim::kHome, 50, int(H - 16), 0);
    int ids[8][3], count[8];
    for (int i = 0; i < rows; ++i) {
        const int y = int(H - 50.0f - float(i) * kRow);
        const bool single = recipe == kChokepoints && i % 2 == 1 && i != rows - 1;
        count[i] = single ? 1 : lanes;
        for (int k = 0; k < count[i]; ++k) {
            const int x = single ? 50 : (lanes == 2 ? (k == 0 ? 30 : 70) : (k == 0 ? 18 : (k == 1 ? 50 : 82)));
            ids[i][k] = b.site(sim::kCamp, x + int(uni(r, -2.0f, 2.0f)), y, 0);
            if (count[i] > 1) b.fork(ids[i][k], i + 1);
        }
    }
    const int keep = b.site(sim::kKeep, 50, 30, 0);
    // Tracks: Home to the first row; lane to lane, straight on, with crossings where they go; the last row to the castle.
    const float cross = recipe == kThreeLanes ? 0.4f : 0.35f;
    for (int k = 0; k < count[0]; ++k) b.road(home, ids[0][k]);
    for (int i = 0; i + 1 < rows; ++i) {
        if (count[i] == 1 || count[i + 1] == 1) {
            for (int a = 0; a < count[i]; ++a)
                for (int c = 0; c < count[i + 1]; ++c) b.road(ids[i][a], ids[i + 1][c]);
            continue;
        }
        for (int k = 0; k < lanes; ++k) b.road(ids[i][k], ids[i + 1][k]);
        for (int k = 0; k + 1 < lanes; ++k) {
            if (r.f01() < cross) b.road(ids[i][k], ids[i + 1][k + 1]);
            if (r.f01() < cross) b.road(ids[i][k + 1], ids[i + 1][k]);
        }
    }
    for (int k = 0; k < count[rows - 1]; ++k) b.road(ids[rows - 1][k], keep);
    sim::LevelDef& d = b.d;

    // The intended route: a walk up the tracks.
    int route[8];
    {
        int at = r.rangei(0, count[0] - 1);
        route[0] = at;
        for (int i = 1; i < rows; ++i) {
            int next[3], nn = 0;
            for (int c = 0; c < count[i]; ++c)
                for (int rr = 0; rr < d.roadCount; ++rr)
                    if ((d.roads[rr].a == ids[i - 1][at] && d.roads[rr].b == ids[i][c])) next[nn++] = c;
            at = nn ? next[r.next() % u32(nn)] : 0;
            route[i] = at;
        }
    }
    // Kinds: what lies on the route is worth taking; what lies off it tempts.
    int shrines = 0;
    for (int i = 0; i < rows; ++i)
        for (int k = 0; k < count[i]; ++k) {
            sim::Site& s = d.sites[ids[i][k]];
            const bool onRoute = route[i] == k;
            const float x = r.f01();
            u8 kind = sim::kVillage;
            if (count[i] == 1) kind = (al.post && x < 0.35f) ? sim::kPost : sim::kCamp;  // a chokepoint: a toll or a fight
            else if (al.shrine && shrines < (n < 60 ? 1 : 2) + int(slot.boss) && x < 0.2f) kind = sim::kShrine;
            else if (x < 0.58f) kind = sim::kCamp;
            else if (!onRoute && al.post && x < 0.66f) kind = sim::kPost;
            else if (!onRoute && al.mire && x < 0.72f) kind = sim::kMire;
            if (kind == sim::kShrine) ++shrines;
            s.kind = kind;
        }
    if (slot.focus == kFocusSwamp) {  // a swamp beside the route, early
        for (int i = 0; i < rows; ++i)
            if (count[i] > 1) {
                d.sites[ids[i][(route[i] + 1) % count[i]]].kind = sim::kMire;
                break;
            }
    }
    if ((slot.focus == kFocusTriple || (al.triple && r.f01() < 0.35f)) && shrines == 0) {
        const int i = r.rangei(rows / 2, rows - 1);
        d.sites[ids[i][route[i]]].kind = sim::kShrine;
        shrines = 1;
    }
    // A key: an off-route site early opens a crossing later (both are optional; the solver weighs them).
    if ((slot.focus == kFocusKey || (al.key && r.f01() < 0.3f)) && rows >= 4 && lanes >= 2) {
        const int ki = r.rangei(0, rows / 2 - 1);
        int kk = count[ki] > 1 ? (route[ki] + 1) % count[ki] : 0;
        sim::Site& key = d.sites[ids[ki][kk]];
        key.kind = sim::kKey;
        // Lock a crossing two rows or more later (add one if none).
        for (int i = ki + 2; i + 1 < rows; ++i) {
            if (count[i] < 2 || count[i + 1] < 2) continue;
            const int a = ids[i][0], c = ids[i + 1][1];
            if (d.roadCount >= sim::kMaxRoads) break;
            int found = -1;
            for (int rr = 0; rr < d.roadCount; ++rr)
                if (d.roads[rr].a == a && d.roads[rr].b == c) found = rr;
            if (found < 0) {
                b.road(a, c, ids[ki][kk]);
            } else {
                d.roads[found].lock = u8(ids[ki][kk]);
            }
            break;
        }
    }

    // The forward walk.
    const i64 start = n <= 50 ? r.rangei(8, 20) : (n <= 150 ? r.rangei(10, 30) : r.rangei(15, 40));
    d.start = i32(start);
    i64 S = start;
    const float wlo = n <= 30 ? 0.45f : (n <= 100 ? 0.55f : 0.62f), whi = n <= 30 ? 0.8f : (n <= 100 ? 0.92f : 0.98f);
    const float waveChance = 0.55f + 0.3f * minv(1.0f, float(n) / 150.0f);
    int waves = 0;
    for (int i = 0; i < rows; ++i) {
        const i64 here = S;
        for (int k = 0; k < count[i]; ++k) {
            sim::Site& s = d.sites[ids[i][k]];
            const bool on = route[i] == k;
            switch (s.kind) {
                case sim::kVillage: s.value = i32(friendly(maxv<i64>(2, i64(float(here) * uni(r, on ? 0.35f : 0.25f, on ? 1.0f : 1.25f))), true)); break;
                case sim::kShrine:
                    s.value = al.triple && (slot.focus == kFocusTriple || r.f01() < 0.3f) ? 3 : 2;
                    break;
                case sim::kCamp:
                case sim::kKey:
                case sim::kDepot: {
                    const float f = on ? uni(r, 0.45f, 0.95f) : uni(r, 0.35f, 1.35f);
                    i64 v = friendly(maxv<i64>(1, i64(float(here) * f)), true);
                    if (on && v > here) v = here;
                    s.value = i32(v);
                    break;
                }
                case sim::kPost: s.value = i32(maxv<i64>(1, minv<i64>(friendly(i64(float(here) * uni(r, 0.08f, 0.3f)), true), here - 1))); break;
                case sim::kMire: s.value = 2; break;
                default: break;
            }
        }
        // The route's pick.
        const sim::Site& p = d.sites[ids[i][route[i]]];
        switch (p.kind) {
            case sim::kVillage: case sim::kCamp: case sim::kKey: case sim::kDepot: S += p.value; break;
            case sim::kShrine: S *= p.value; break;
            case sim::kPost: S -= p.value; break;
            case sim::kMire: S = (S + 1) / 2; break;
            default: break;
        }
        // A wave after most rows, always one before the castle.
        const bool last = i == rows - 1;
        if (last || r.f01() < waveChance) {
            const bool twin = (slot.focus == kFocusDouble && waves == 0) || (al.twin && r.f01() < 0.18f);
            for (int w = 0; w < (twin ? 2 : 1) && d.raidCount < sim::kMaxRaids; ++w) {
                const float f = twin ? uni(r, 0.3f, 0.55f) : uni(r, wlo, whi);
                const i64 strength = friendly(maxv<i64>(1, i64(float(S) * f)), true);
                b.raid(i + 1, ids[i][route[i]], i32(minv<i64>(strength, 999999)));
                S += strength;
                ++waves;
            }
        }
        if (S > 9999999) S = 9999999;
    }
    // The castle, a share of what arrives; supply camps (depots) on the field cut it.
    i64 castle = friendly(maxv<i64>(1, i64(float(S) * uni(r, slot.boss ? 0.75f : 0.6f, slot.boss ? 0.95f : 0.9f))), true);
    if (slot.focus == kFocusDepot || (al.depot && r.f01() < 0.3f)) {
        int made = 0;
        for (int i = 0; i < rows && made < 2; ++i)
            for (int k = 0; k < count[i] && made < 2; ++k) {
                sim::Site& s = d.sites[ids[i][k]];
                if (s.kind != sim::kCamp || route[i] == k) continue;
                s.kind = sim::kDepot;
                s.aux = i32(friendly(maxv<i64>(1, castle / 5), true));
                ++made;
            }
        if (made == 0) {
            for (int i = 0; i < rows && made < 1; ++i) {
                sim::Site& s = d.sites[ids[i][route[i]]];
                if (s.kind == sim::kCamp) {
                    s.kind = sim::kDepot;
                    s.aux = i32(friendly(maxv<i64>(1, castle / 5), true));
                    ++made;
                }
            }
        }
        castle += castle / 10;  // a little stronger, for there are cuts to take
    }
    d.sites[keep].value = i32(minv<i64>(castle, 99999999));
    for (int i = 0; i < d.siteCount; ++i)
        if (d.sites[i].value > 99999999) d.sites[i].value = 99999999;
    out = d;
    return true;
}

Grade grade(int n, sim::LevelDef& d, sim::Solver& solver, bool hand) {
    Grade gr;
    const sim::Solve s = solver.run(d, sim::start(d));
    gr.expansions = s.expansions;
    if (!s.proven) {
        std::snprintf(gr.why, sizeof gr.why, "the solver gave up (%d expansions)", s.expansions);
        return gr;
    }
    if (!s.winnable) {
        std::snprintf(gr.why, sizeof gr.why, "not winnable");
        return gr;
    }
    d.best = s.best;
    gr.best = s.best;
    if (!hand) {
        if (s.best < 4 * i64(d.start)) {
            std::snprintf(gr.why, sizeof gr.why, "no snowball (best %lld from %d)", (long long)s.best, d.start);
            return gr;
        }
        if (s.expansions > 60000) {
            std::snprintf(gr.why, sizeof gr.why, "too costly to solve (%d)", s.expansions);
            return gr;
        }
        // Sites far enough apart for a finger and their tags (GAME_DESIGN 8.4 line 4).
        for (int i = 0; i < d.siteCount; ++i)
            for (int j = i + 1; j < d.siteCount; ++j) {
                const float dx = float(d.sites[i].x - d.sites[j].x), dy = float(d.sites[i].y - d.sites[j].y);
                const bool sameRow = std::fabs(dy) < 14.0f;
                if ((sameRow && std::fabs(dx) < 22.0f) || dx * dx + dy * dy < 17.0f * 17.0f) {
                    std::snprintf(gr.why, sizeof gr.why, "sites %d and %d too close", i, j);
                    return gr;
                }
            }
    }
    const PlayStats p = randomPlays(d, 400, u64(n) * 9176 + 7);
    gr.randomWinPct = p.wins * 100 / maxv(1, p.plays);
    gr.randomThreePct = p.threeStars * 100 / maxv(1, p.plays);
    i64 greedyFinal = 0;
    gr.greedyWin = greedyWins(d, &greedyFinal);
    gr.decision = hasDecision(d, solver);
    if (hand) {
        gr.ok = true;
        return gr;
    }
    const Band b = bandFor(n);
    if (gr.randomWinPct < b.randomLo || gr.randomWinPct > b.randomHi) {
        std::snprintf(gr.why, sizeof gr.why, "random play wins %d%% (band %d-%d)", gr.randomWinPct, b.randomLo, b.randomHi);
        return gr;
    }
    if (gr.randomThreePct > b.threeHi || gr.randomThreePct < b.threeLo) {
        std::snprintf(gr.why, sizeof gr.why, "random three stars %d%% (band %d-%d)", gr.randomThreePct, b.threeLo, b.threeHi);
        return gr;
    }
    if (n <= 30 && !gr.greedyWin) {
        std::snprintf(gr.why, sizeof gr.why, "greedy loses early");
        return gr;
    }
    // From level 41 the naive player (the most soldiers now) must not find the best line on most levels: the lanes and the
    // waves have to be thought through for three stars.
    const int p10 = n % 10;
    const bool mustMiss = (n > 40 && n <= 120 && p10 % 2 == 1) || (n > 120 && p10 != 0 && p10 != 5);
    if (mustMiss && gr.greedyWin && greedyFinal >= d.best) {
        std::snprintf(gr.why, sizeof gr.why, "the naive line is already the best");
        return gr;
    }
    if (!gr.decision) {
        std::snprintf(gr.why, sizeof gr.why, "no real decision");
        return gr;
    }
    gr.ok = true;
    return gr;
}

}  // namespace pm::gen
