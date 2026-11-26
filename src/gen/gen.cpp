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

bool isCamp(u8 k) { return k == sim::kCamp || k == sim::kDepot || k == sim::kKey || k == sim::kKeep; }
}  // namespace

// ---- Plays -------------------------------------------------------------------------------------------------------------

int greedyMove(const sim::LevelDef& d, const sim::State& st) {
    const u32 moves = sim::takeable(d, st);
    int best = -1;
    i64 bestV = 0;
    for (int m = 0; m < d.siteCount; ++m) {
        if (!(moves & bit(m))) continue;
        if (!isCamp(d.sites[m].kind)) continue;
        const i64 v = sim::number(d, st, m);
        if (best < 0 || v < bestV) {
            best = m;
            bestV = v;
        }
    }
    if (best >= 0) return best;
    for (int m = 0; m < d.siteCount; ++m)
        if (moves & bit(m)) return m;
    return (moves & bit(sim::kRivalMove)) ? sim::kRivalMove : -1;
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

// ---- The calendar (GAME_DESIGN 8.2) ------------------------------------------------------------------------------------

const char* recipeName(int r) {
    static const char* const names[kRecipeCount] = {"TREE", "LADDER", "RING", "HOURGLASS", "ISLANDS", "CROSSROADS", "TWIN VALLEYS",
                                                    "SPIRAL", "PAIRS", "TRIPLES", "BRAID", "RINGED KEEP", "SUPPLY LINE",
                                                    "PARALLEL ROADS", "PINCER", "THREE ROADS", "HOMESTEAD"};
    return r >= 0 && r < kRecipeCount ? names[r] : "HAND";
}

Slot calendar(int n) {
    Slot s;
    s.world = u8((n - 1) / 10 % 10 + 1);
    s.boss = n % 10 == 0;
    s.kind = sim::kConquest;
    if (s.boss) {
        // Bosses mix two kinds once both are known (GAME_DESIGN 6.6).
        if (n >= 100 && n % 30 == 0) s.kind = sim::kRace;
        else if (n >= 80 && n % 20 == 0) s.kind = sim::kSiege;
        return s;
    }
    const int p = n % 10;  // 1..9 within the block
    if (n == 22) { s.focus = 1; s.hint = 9; return s; }
    if (n == 28) { s.focus = 2; s.hint = 10; return s; }
    if (n == 34) { s.focus = 3; s.hint = 11; return s; }
    if (n == 16) { s.kind = sim::kGauntlet; s.hint = 8; return s; }
    if (n <= 40) {
        if (n == 16 || (n > 16 && (p == 3 || p == 7) && n != 23 && n != 27)) s.kind = sim::kGauntlet;
        if (n == 19 || n == 33 || n == 37) s.kind = sim::kGauntlet;
        return s;
    }
    if (n == 42) { s.kind = sim::kSiege; s.hint = 12; return s; }
    if (n == 52) { s.focus = 4; s.hint = 13; return s; }
    if (n == 62) { s.kind = sim::kRace; s.hint = 14; return s; }
    if (n == 75) { s.kind = sim::kLastStand; s.hint = 15; return s; }
    if (n <= 80) {
        static const u8 pat[10] = {0, sim::kConquest, sim::kGauntlet, sim::kConquest, sim::kSiege, sim::kConquest, sim::kRace,
                                   sim::kConquest, sim::kGauntlet, sim::kLastStand};
        s.kind = pat[p];
        if (s.kind == sim::kRace && n < 62) s.kind = sim::kConquest;
        if (s.kind == sim::kLastStand && n < 75) s.kind = sim::kConquest;
        if (s.kind == sim::kSiege && n < 42) s.kind = sim::kConquest;
        return s;
    }
    static const u8 pat2[10] = {0, sim::kConquest, sim::kGauntlet, sim::kSiege, sim::kConquest, sim::kRace, sim::kConquest,
                                sim::kLastStand, sim::kGauntlet, sim::kConquest};
    s.kind = pat2[p];
    return s;
}

Allowed allowedAt(int n) {
    Allowed a;
    a.village = n >= 3;
    a.shrine = n >= 4;
    a.post = n >= 5;
    a.fork = n >= 6;
    a.grow = n >= 12;
    a.key = n >= 22;
    a.mire = n >= 28;
    a.desert = n >= 34;
    a.depot = n >= 42;
    a.fade = n >= 52;
    return a;
}

// ---- Layouts -------------------------------------------------------------------------------------------------------

namespace {

struct Graph {
    float x[sim::kMaxSites], y[sim::kMaxSites];
    int row[sim::kMaxSites];
    u8 role[sim::kMaxSites];   // a preferred kind (0: any)
    u8 fork[sim::kMaxSites];
    int n = 0;
    int ra[sim::kMaxRoads], rb[sim::kMaxRoads];
    int rn = 0;
    int keep = -1;
    float pageH = 160;
    int add(float px, float py, int r = 0, u8 roleKind = 0) {
        x[n] = px;
        y[n] = py;
        row[n] = r;
        role[n] = roleKind;
        fork[n] = 0;
        return n++;
    }
    bool has(int a, int b) const {
        for (int i = 0; i < rn; ++i)
            if ((ra[i] == a && rb[i] == b) || (ra[i] == b && rb[i] == a)) return true;
        return false;
    }
};

bool segCross(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
    auto orient = [](float px, float py, float qx, float qy, float rx, float ry) {
        const float v = (qx - px) * (ry - py) - (qy - py) * (rx - px);
        return v > 1e-3f ? 1 : (v < -1e-3f ? -1 : 0);
    };
    const int o1 = orient(ax, ay, bx, by, cx, cy), o2 = orient(ax, ay, bx, by, dx, dy);
    const int o3 = orient(cx, cy, dx, dy, ax, ay), o4 = orient(cx, cy, dx, dy, bx, by);
    return o1 * o2 < 0 && o3 * o4 < 0;
}

float segDist(float px, float py, float ax, float ay, float bx, float by) {
    const float vx = bx - ax, vy = by - ay;
    const float t = clampv(((px - ax) * vx + (py - ay) * vy) / maxv(1e-4f, vx * vx + vy * vy), 0.0f, 1.0f);
    const float dx = ax + vx * t - px, dy = ay + vy * t - py;
    return std::sqrt(dx * dx + dy * dy);
}

// A road only if it crosses no other and passes no site but its ends.
bool road(Graph& g, int a, int b) {
    if (a == b || g.has(a, b) || g.rn >= sim::kMaxRoads) return false;
    for (int i = 0; i < g.rn; ++i) {
        if (g.ra[i] == a || g.rb[i] == a || g.ra[i] == b || g.rb[i] == b) continue;
        if (segCross(g.x[a], g.y[a], g.x[b], g.y[b], g.x[g.ra[i]], g.y[g.ra[i]], g.x[g.rb[i]], g.y[g.rb[i]])) return false;
    }
    for (int s = 0; s < g.n; ++s) {
        if (s == a || s == b) continue;
        if (segDist(g.x[s], g.y[s], g.x[a], g.y[a], g.x[b], g.y[b]) < 9.0f) return false;
    }
    g.ra[g.rn] = a;
    g.rb[g.rn] = b;
    ++g.rn;
    return true;
}

float jit(Rng& r, float a) { return (r.f01() * 2.0f - 1.0f) * a; }

// Rows from just above Home to just under the Keep.
float rowY(const Graph& g, int i, int rows) {
    const float top = 56.0f, bottom = g.pageH - 38.0f;
    return rows <= 1 ? (top + bottom) * 0.5f : bottom - (bottom - top) * float(i) / float(rows - 1);
}

void layoutTree(Graph& g, Rng& r, int sites) {
    const int b = sites >= 10 && r.f01() < 0.6f ? 3 : 2;
    const float xs3[3] = {18, 50, 82}, xs2[2] = {26, 74};
    const int perBranch = maxv(1, (sites - 2) / b);
    const int rows = minv(perBranch, 5);
    int nodes[3][6];
    const int home = g.add(50 + jit(r, 4), g.pageH - 12);
    for (int k = 0; k < b; ++k)
        for (int i = 0; i < rows; ++i) nodes[k][i] = g.add((b == 3 ? xs3[k] : xs2[k]) + jit(r, 4), rowY(g, i, rows) + jit(r, 3), i);
    g.keep = g.add(50 + jit(r, 3), 30);
    for (int k = 0; k < b; ++k) {
        road(g, home, nodes[k][0]);
        for (int i = 0; i + 1 < rows; ++i) road(g, nodes[k][i], nodes[k][i + 1]);
        road(g, nodes[k][rows - 1], g.keep);
    }
    for (int k = 0; k + 1 < b; ++k) {
        const int i = r.rangei(0, rows - 1);
        road(g, nodes[k][i], nodes[k + 1][i]);
        if (r.f01() < 0.4f) road(g, nodes[k][(i + 1) % rows], nodes[k + 1][(i + 1) % rows]);
    }
}

void layoutLadder(Graph& g, Rng& r, int sites) {
    const int rows = clampv((sites - 2) / 2, 2, 5);
    int L[6], R[6], M[6];
    const int home = g.add(50 + jit(r, 3), g.pageH - 12);
    for (int i = 0; i < rows; ++i) {
        L[i] = g.add(22 + jit(r, 4), rowY(g, i, rows) + jit(r, 3), i);
        R[i] = g.add(78 + jit(r, 4), rowY(g, i, rows) + jit(r, 3), i);
        M[i] = -1;
    }
    g.keep = g.add(50 + jit(r, 3), 30);
    road(g, home, L[0]);
    road(g, home, R[0]);
    for (int i = 0; i + 1 < rows; ++i) {
        road(g, L[i], L[i + 1]);
        road(g, R[i], R[i + 1]);
    }
    road(g, L[rows - 1], g.keep);
    road(g, R[rows - 1], g.keep);
    int extra = sites - g.n;
    for (int i = 0; i < rows; ++i) {
        if (extra > 0 && r.f01() < 0.55f) {  // a rung with a site in the middle
            M[i] = g.add(50 + jit(r, 3), rowY(g, i, rows) + jit(r, 2), i);
            road(g, L[i], M[i]);
            road(g, M[i], R[i]);
            --extra;
        } else if (r.f01() < 0.5f) {
            road(g, L[i], R[i]);
        }
    }
}

void layoutRing(Graph& g, Rng& r, int sites) {
    const int side = clampv((sites - 2) / 2, 2, 5);
    const float cy = (g.pageH - 12 + 30) * 0.5f, ry = (g.pageH - 12 - 30) * 0.5f;
    const int home = g.add(50, g.pageH - 12);
    int L[6], R[6];
    for (int i = 0; i < side; ++i) {  // a left and a right arc of an ellipse, from Home (bottom) to the Keep (top)
        const float ang = 3.14159f * float(i + 1) / float(side + 1);
        L[i] = g.add(50 - 38 * std::sin(ang) + jit(r, 2), cy + ry * std::cos(ang) + jit(r, 2), i);
        R[i] = g.add(50 + 38 * std::sin(ang) + jit(r, 2), cy + ry * std::cos(ang) + jit(r, 2), i);
    }
    g.keep = g.add(50, 30);
    road(g, home, L[0]);
    road(g, home, R[0]);
    for (int i = 0; i + 1 < side; ++i) {
        road(g, L[i], L[i + 1]);
        road(g, R[i], R[i + 1]);
    }
    road(g, L[side - 1], g.keep);
    road(g, R[side - 1], g.keep);
    if (sites > g.n) {  // a causeway across the lake
        const int i = side / 2;
        const int c = g.add(50 + jit(r, 2), (g.y[L[i]] + g.y[R[i]]) * 0.5f, i, sim::kPost);
        road(g, L[i], c);
        road(g, c, R[i]);
    }
}

void layoutHourglass(Graph& g, Rng& r, int sites) {
    const int lobe = clampv((sites - 3) / 2, 2, 5);
    const int home = g.add(50 + jit(r, 3), g.pageH - 12);
    const float waistY = g.pageH * 0.5f + 4;
    const float bottomTop = waistY + 22, bottomBot = g.pageH - 34;
    int B[6], T[6];
    for (int i = 0; i < lobe; ++i) {
        const float t = lobe == 1 ? 0.5f : float(i) / float(lobe - 1);
        const float x = i % 2 == 0 ? 24.0f + jit(r, 4) : 76.0f + jit(r, 4);
        B[i] = g.add(x, bottomBot - (bottomBot - bottomTop) * t + jit(r, 2), i);
    }
    const int waist = g.add(50 + jit(r, 2), waistY, lobe, sim::kPost);
    const float topBot = waistY - 22, topTop = 58;
    for (int i = 0; i < lobe; ++i) {
        const float t = lobe == 1 ? 0.5f : float(i) / float(lobe - 1);
        const float x = i % 2 == 0 ? 24.0f + jit(r, 4) : 76.0f + jit(r, 4);
        T[i] = g.add(x, topBot - (topBot - topTop) * t + jit(r, 2), lobe + 1 + i);
    }
    g.keep = g.add(50 + jit(r, 3), 30);
    for (int i = 0; i < lobe; ++i) {
        road(g, home, B[i]);
        if (i > 0) road(g, B[i - 1], B[i]);
        road(g, B[i], waist);
        road(g, waist, T[i]);
        if (i > 0) road(g, T[i - 1], T[i]);
        road(g, T[i], g.keep);
    }
}

void layoutIslands(Graph& g, Rng& r, int sites) {
    const int home = g.add(50 + jit(r, 3), g.pageH - 12);
    const int a = g.add(26 + jit(r, 4), g.pageH - 30 + jit(r, 2), 0);
    const int b = g.add(74 + jit(r, 4), g.pageH - 32 + jit(r, 2), 0);
    const int p1 = g.add(50 + jit(r, 4), g.pageH - 56, 1, sim::kPost);
    const int c = g.add(24 + jit(r, 4), g.pageH * 0.5f + jit(r, 3), 2);
    const int d = g.add(76 + jit(r, 4), g.pageH * 0.5f + jit(r, 3), 2);
    const int p2 = g.add(50 + jit(r, 4), 70, 3, sim::kPost);
    int e = -1;
    if (sites > 9) e = g.add(r.f01() < 0.5f ? 22.0f : 78.0f, 50 + jit(r, 3), 4);
    g.keep = g.add(50 + jit(r, 3), 30);
    road(g, home, a), road(g, home, b), road(g, a, p1), road(g, b, p1), road(g, p1, c), road(g, p1, d), road(g, c, p2),
        road(g, d, p2), road(g, p2, g.keep);
    if (e >= 0) {
        road(g, e, p2);
        road(g, e, g.keep);
        if (!g.has(e, p2) && !g.has(e, g.keep)) road(g, e, g.x[e] < 50 ? c : d);
    }
}

void layoutCrossroads(Graph& g, Rng& r, int sites) {
    const int home = g.add(50 + jit(r, 3), g.pageH - 12);
    const int low = g.add(50 + jit(r, 3), g.pageH - 40, 0);
    const int hub = g.add(50 + jit(r, 2), g.pageH * 0.5f, 1);
    const int up = g.add(50 + jit(r, 3), 58, 2);
    const int l1 = g.add(20 + jit(r, 3), g.pageH * 0.5f + 12, 1), r1 = g.add(80 + jit(r, 3), g.pageH * 0.5f + 12, 1);
    g.keep = g.add(50, 30);
    road(g, home, low), road(g, low, hub), road(g, hub, up), road(g, up, g.keep), road(g, hub, l1), road(g, hub, r1);
    if (sites > 8) {
        const int l2 = g.add(20 + jit(r, 3), 66, 2), r2 = g.add(80 + jit(r, 3), 66, 2);
        road(g, l1, l2), road(g, r1, r2), road(g, l2, g.keep), road(g, r2, g.keep);
    } else {
        road(g, l1, up);
        road(g, r1, up);
    }
    if (sites > 10) {
        const int l0 = g.add(18 + jit(r, 3), g.pageH - 28, 0), r0 = g.add(82 + jit(r, 3), g.pageH - 28, 0);
        road(g, home, l0), road(g, home, r0), road(g, l0, l1), road(g, r0, r1);
    }
}

void layoutTwinValleys(Graph& g, Rng& r, int sites) {
    const int rows = clampv((sites - 2) / 2, 2, 5);
    const int home = g.add(50 + jit(r, 3), g.pageH - 12);
    int L[6], R[6];
    for (int i = 0; i < rows; ++i) {
        L[i] = g.add(24 + jit(r, 5), rowY(g, i, rows) + jit(r, 3), i);
        R[i] = g.add(76 + jit(r, 5), rowY(g, i, rows) + jit(r, 3), i);
    }
    g.keep = g.add(50, 30);
    g.fork[L[0]] = g.fork[R[0]] = 1;  // choose a valley
    road(g, home, L[0]), road(g, home, R[0]);
    for (int i = 0; i + 1 < rows; ++i) road(g, L[i], L[i + 1]), road(g, R[i], R[i + 1]);
    road(g, L[rows - 1], g.keep), road(g, R[rows - 1], g.keep);
}

void layoutSpiral(Graph& g, Rng& r, int sites) {
    const int home = g.add(20 + jit(r, 3), g.pageH - 14);
    int prev = home;
    const int perRow = 3;
    const int rows = clampv((sites - 2 + perRow - 1) / perRow, 2, 5);
    int count = 0;
    int firstOfRow[6], lastOfRow[6];
    for (int rw = 0; rw < rows && g.n < sites - 1; ++rw) {
        const float y = rowY(g, rw, rows) + 4;
        for (int k = 0; k < perRow && g.n < sites - 1; ++k) {
            const int kk = rw % 2 == 0 ? k : perRow - 1 - k;
            const int s = g.add(18.0f + 32.0f * float(kk) + jit(r, 3), y + jit(r, 2), rw);
            if (k == 0) firstOfRow[rw] = s;
            lastOfRow[rw] = s;
            road(g, prev, s);
            prev = s;
            ++count;
        }
    }
    g.keep = g.add(50, 30);
    road(g, prev, g.keep);
    // A shortcut or two across the turns.
    for (int rw = 0; rw + 1 < rows; ++rw)
        if (r.f01() < 0.5f) road(g, firstOfRow[rw], lastOfRow[rw + 1]);
    (void)count;
}

void layoutGauntlet(Graph& g, Rng& r, int rows, int width, bool braid) {
    g.pageH = float(70 + rows * 30);
    const int home = g.add(50, g.pageH - 12);
    int prev[3] = {home, home, home};
    int prevN = 1;
    for (int i = 0; i < rows; ++i) {
        const int w = braid && i % 2 == 1 ? 2 : width;
        const float y = g.pageH - 44 - 30.0f * float(i) + jit(r, 2);
        int cur[3];
        for (int k = 0; k < w; ++k) {
            const float x = w == 2 ? (k == 0 ? 28.0f : 72.0f) : 16.0f + 34.0f * float(k);
            cur[k] = g.add(x + jit(r, 3), y, i);
            if (!(braid && i % 2 == 1)) g.fork[cur[k]] = u8(1 + i);
        }
        // Straight up, then a diagonal or two that cross nothing.
        for (int k = 0; k < w; ++k) {
            if (prevN == 1) road(g, prev[0], cur[k]);
            else road(g, prev[minv(k * prevN / w, prevN - 1)], cur[k]);
        }
        if (prevN > 1)
            for (int k = 0; k < w; ++k)
                for (int j = 0; j < prevN; ++j)
                    if (r.f01() < 0.45f) road(g, prev[j], cur[k]);
        for (int k = 0; k < w; ++k) prev[k] = cur[k];
        prevN = w;
    }
    g.keep = g.add(50, 26);
    for (int k = 0; k < prevN; ++k) road(g, prev[k], g.keep);
}

void layoutRace(Graph& g, Rng& r, int sites, bool pincer, int& rivalCamp, int* route, int& routeLen) {
    // The host climbs one side; the Warlord comes down the other, taking camps on its way to Home.
    const float hx = pincer ? 74.0f : 26.0f, wx = pincer ? 26.0f : 74.0f;
    const int rows = clampv((sites - 3) / 2, 2, 5);
    const int home = g.add(50 + jit(r, 3), g.pageH - 12);
    int H[6], W[6];
    for (int i = 0; i < rows; ++i) {
        H[i] = g.add(hx + jit(r, 4), rowY(g, i, rows) + jit(r, 3), i);
        W[i] = g.add(wx + jit(r, 4), rowY(g, i, rows) + jit(r, 3), i);
    }
    rivalCamp = g.add(wx + jit(r, 3), 44, rows, sim::kRivalCamp);
    g.keep = g.add(50 + (pincer ? -6.0f : 6.0f), 30);
    road(g, home, H[0]);
    for (int i = 0; i + 1 < rows; ++i) road(g, H[i], H[i + 1]), road(g, W[i], W[i + 1]);
    road(g, H[rows - 1], g.keep);
    road(g, W[rows - 1], rivalCamp);
    road(g, rivalCamp, g.keep);
    road(g, home, W[0]);
    const int cross = r.rangei(0, rows - 1);
    road(g, H[cross], W[cross]);
    routeLen = 0;
    route[routeLen++] = u8(rivalCamp);
    for (int i = rows - 1; i >= 0 && routeLen < sim::kMaxRoute; --i) route[routeLen++] = W[i];
}

void layoutLastStand(Graph& g, Rng& r, int sites, bool ring) {
    const int home = g.add(50, ring ? g.pageH * 0.56f : g.pageH - 24);
    if (ring) {
        const int n = clampv(sites - 1, 5, 8);
        int s[8];
        for (int k = 0; k < n; ++k) {
            const float a = 6.283f * float(k) / float(n) + 0.3f;
            s[k] = g.add(50 + 36 * std::cos(a) + jit(r, 2), g.pageH * 0.56f + 44 * std::sin(a) + jit(r, 2), k);
        }
        for (int k = 0; k < n; ++k) {
            if (k % 2 == 0 || r.f01() < 0.5f) road(g, home, s[k]);
            road(g, s[k], s[(k + 1) % n]);
        }
    } else {
        const float dirs[3][2] = {{-1, -0.3f}, {0, -1}, {1, -0.3f}};
        const int per = clampv((sites - 1) / 3, 1, 4);
        for (int b = 0; b < 3; ++b) {
            int prev = home;
            for (int i = 0; i < per; ++i) {
                const float d = 26.0f * float(i + 1);
                const int s = g.add(clampv(50 + dirs[b][0] * d * 1.15f + jit(r, 3), 12.0f, 88.0f),
                                    clampv(g.pageH - 24 + dirs[b][1] * d + jit(r, 3), 30.0f, g.pageH - 20), i);
                road(g, prev, s);
                prev = s;
            }
        }
    }
    g.keep = -1;
}

// ---- Numbers ---------------------------------------------------------------------------------------------------------

i64 friendly(i64 v, bool down) {
    if (v < 30) return maxv<i64>(1, v);
    const i64 step = v < 300 ? 5 : (v < 3000 ? 10 : (v < 30000 ? 50 : 100));
    return down ? v / step * step : (v + step - 1) / step * step;
}

struct Band {
    float lo, hi;     // a camp's share of the host that meets it on the intended line
    int sitesLo, sitesHi;
    int randomLo, randomHi;   // random play's win share, %
    int threeHi;              // random play reaching three stars, % at most
    int threeLo;
};
Band bandFor(int n) {
    if (n <= 30) return {0.30f, 0.75f, 6, 10, 70, 100, 40, 3};
    if (n <= 80) return {0.45f, 0.88f, 8, 13, 45, 85, 20, 1};
    if (n <= 160) return {0.55f, 0.95f, 9, 16, 25, 65, 10, 0};
    return {0.6f, 0.97f, 10, 18, 12, 50, 5, 0};
}

}  // namespace

bool generate(int n, const Slot& slot, int recipe, u64 seed, sim::LevelDef& out) {
    Rng r(seed, u64(n) * 2 + 1);
    const Band band = bandFor(n);
    const Allowed allow = allowedAt(n);
    int sites = r.rangei(band.sitesLo, band.sitesHi);
    if (slot.boss) sites = minv(sites + 4, 20);
    Graph g;
    g.pageH = 160;
    int rivalCamp = -1, route[sim::kMaxRoute], routeLen = 0;
    switch (recipe) {
        case kTree: layoutTree(g, r, sites); break;
        case kLadder: layoutLadder(g, r, sites); break;
        case kRing: layoutRing(g, r, sites); break;
        case kHourglass: layoutHourglass(g, r, sites); break;
        case kIslands: layoutIslands(g, r, sites); break;
        case kCrossroads: layoutCrossroads(g, r, sites); break;
        case kTwinValleys: layoutTwinValleys(g, r, sites); break;
        case kSpiral: layoutSpiral(g, r, sites); break;
        case kPairs: layoutGauntlet(g, r, clampv(sites / 2 + 1, 5, 9), 2, false); break;
        case kTriples: layoutGauntlet(g, r, clampv(sites / 3 + 2, 5, 7), 3, false); break;
        case kBraid: layoutGauntlet(g, r, clampv(sites / 2 + 1, 5, 9), 2, true); break;
        case kRingedKeep: layoutRing(g, r, sites); break;
        case kSupplyLine: layoutLadder(g, r, sites); break;
        case kParallel: layoutRace(g, r, sites, false, rivalCamp, route, routeLen); break;
        case kPincer: layoutRace(g, r, sites, true, rivalCamp, route, routeLen); break;
        case kThreeRoads: layoutLastStand(g, r, sites, false); break;
        case kHomestead: layoutLastStand(g, r, sites, true); break;
        default: return false;
    }
    if (g.n < 4 || g.n > sim::kMaxSites) return false;
    // Every site reachable from Home.
    {
        bool seen[sim::kMaxSites] = {};
        int q[sim::kMaxSites], qh = 0, qt = 0;
        q[qt++] = 0;
        seen[0] = true;
        while (qh < qt) {
            const int u = q[qh++];
            for (int i = 0; i < g.rn; ++i) {
                const int v = g.ra[i] == u ? g.rb[i] : (g.rb[i] == u ? g.ra[i] : -1);
                if (v >= 0 && !seen[v]) {
                    seen[v] = true;
                    q[qt++] = v;
                }
            }
        }
        if (qt != g.n) return false;
    }
    // Kinds. A post where a recipe asks for a bridge; then villages, a shrine or two, mires, keys, depots by what is allowed.
    sim::LevelBuilder b;
    b.d.kind = slot.kind;
    b.d.boss = slot.boss;
    b.d.world = slot.world;
    b.d.recipe = u8(recipe);
    b.d.pageH = i16(g.pageH + 0.5f);
    b.d.seed = u32(seed);
    for (int i = 0; i < g.n; ++i) {
        u8 kind = sim::kCamp;
        if (i == 0) kind = sim::kHome;
        else if (i == g.keep) kind = sim::kKeep;
        else if (g.role[i] == sim::kRivalCamp) kind = sim::kRivalCamp;
        else if (g.role[i] == sim::kPost) kind = allow.post ? sim::kPost : sim::kCamp;
        b.site(kind, int(g.x[i] + 0.5f), int(g.y[i] + 0.5f), 0);
        b.d.sites[i].fork = allow.fork || slot.kind == sim::kGauntlet ? g.fork[i] : 0;
    }
    for (int i = 0; i < g.rn; ++i) b.road(g.ra[i], g.rb[i]);
    // Roles for the plain camps.
    int shrines = 0, mires = 0, keys = 0, depots = 0;
    const int maxShrines = n < 60 ? 1 : 2;
    for (int i = 1; i < g.n; ++i) {
        sim::Site& s = b.d.sites[i];
        if (s.kind != sim::kCamp) continue;
        const float x = r.f01();
        if (allow.village && x < 0.22f) s.kind = sim::kVillage;
        else if (allow.shrine && shrines < maxShrines && x < 0.32f) s.kind = sim::kShrine, ++shrines;
        else if (allow.post && x < 0.38f) s.kind = sim::kPost;
        else if (allow.mire && mires < 1 && x < 0.43f) s.kind = sim::kMire, ++mires;
        else if (slot.kind == sim::kSiege && depots < 3 && x < 0.62f) s.kind = sim::kDepot, ++depots;
    }
    if (slot.kind == sim::kSiege && depots == 0) {
        for (int i = 1; i < g.n && depots < 2; ++i)
            if (b.d.sites[i].kind == sim::kCamp) b.d.sites[i].kind = sim::kDepot, ++depots;
    }
    if (slot.focus == 2 && mires == 0)
        for (int i = 1; i < g.n && mires == 0; ++i)
            if (b.d.sites[i].kind == sim::kCamp && b.d.sites[i].fork == 0) b.d.sites[i].kind = sim::kMire, ++mires;
    if (slot.focus == 4 && shrines == 0)
        for (int i = 1; i < g.n && shrines == 0; ++i)
            if (b.d.sites[i].kind == sim::kCamp && b.d.sites[i].fork == 0) b.d.sites[i].kind = sim::kShrine, ++shrines;
    // A key tower locks the road into the Keep (or a road near the top).
    if (allow.key && g.keep >= 0 && (slot.focus == 1 || r.f01() < 0.35f)) {
        int lockRoad = -1;
        for (int i = 0; i < b.d.roadCount; ++i)
            if (b.d.roads[i].a == g.keep || b.d.roads[i].b == g.keep) lockRoad = i;
        int tower = -1;
        for (int i = 1; i < g.n && tower < 0; ++i)
            if (b.d.sites[i].kind == sim::kCamp && b.d.sites[i].y > g.pageH * 0.45f && b.d.sites[i].fork == 0) tower = i;
        if (lockRoad >= 0 && tower >= 0) {
            b.d.sites[tower].kind = sim::kKey;
            // Lock every road into the Keep so the tower matters.
            for (int i = 0; i < b.d.roadCount; ++i)
                if (b.d.roads[i].a == g.keep || b.d.roads[i].b == g.keep) b.d.roads[i].lock = u8(tower);
            ++keys;
        }
    }
    (void)keys;
    // Shrines are never forked away from a dead end: a fork's sites stay camps or villages.
    for (int i = 1; i < g.n; ++i) {
        sim::Site& s = b.d.sites[i];
        if (s.fork && (s.kind == sim::kMire || s.kind == sim::kKey || s.kind == sim::kDepot)) s.kind = sim::kCamp;
    }
    if (rivalCamp >= 0) {
        b.d.routeLen = u8(routeLen);
        for (int k = 0; k < routeLen; ++k) {
            b.d.route[k] = u8(route[k]);
            if (k > 0) b.d.sites[route[k]].kind = sim::kCamp;  // the Warlord takes camps
            b.d.sites[route[k]].fork = 0;
        }
    }

    // The forward walk (GAME_DESIGN 8.3): an intended order, numbers set from the host that meets them.
    sim::LevelDef& d = b.d;
    const i64 start = n <= 100 ? r.rangei(5, 20) : r.rangei(10, 30);
    d.start = i32(start);
    sim::State st = sim::start(d);
    st.flags &= u8(~sim::kRivalAlive);  // the walk ignores the rival
    i64 S = start;
    int order[sim::kMaxSites], on = 0;
    bool taken[sim::kMaxSites] = {};
    taken[0] = true;
    int lastShrineAllowed = maxv(2, (g.n * 6) / 10);
    bool focusDone = false;
    for (int step = 0; step < g.n * 2 && on < g.n - 1; ++step) {
        // Reachable, untaken, unburnt, not the Keep (last), not the rival's camp.
        int cand[sim::kMaxSites], cn = 0;
        for (int rr = 0; rr < d.roadCount; ++rr) {
            const int a = d.roads[rr].a, c = d.roads[rr].b;
            const int lock = d.roads[rr].lock;
            if (lock != sim::kNoLock && !taken[lock]) continue;
            for (int side = 0; side < 2; ++side) {
                const int from = side ? c : a, to = side ? a : c;
                if (!taken[from] || taken[to] || (st.burnt & bit(to)) || to == g.keep || d.sites[to].kind == sim::kRivalCamp) continue;
                bool dup = false;
                for (int k = 0; k < cn; ++k) dup = dup || cand[k] == to;
                if (!dup) cand[cn++] = to;
            }
        }
        if (cn == 0) break;
        // Shrines late in the order (the trap: they look best early), mires early.
        int pick = cand[r.next() % u32(cn)];
        for (int tries = 0; tries < 6; ++tries) {
            const u8 k = d.sites[pick].kind;
            if (k == sim::kShrine && on < lastShrineAllowed && cn > 1) pick = cand[r.next() % u32(cn)];
            else break;
        }
        sim::Site& s = d.sites[pick];
        const int turn = on;
        switch (s.kind) {
            case sim::kCamp: case sim::kDepot: case sim::kKey: {
                const float f = band.lo + (band.hi - band.lo) * r.f01();
                i64 v = friendly(maxv<i64>(1, i64(float(S) * f)), true);
                if (v > S) v = S;
                if (allow.grow && s.kind == sim::kCamp && !s.fork && r.f01() < 0.18f && turn >= 2) {
                    // A growing camp: it is worth more later, while it can still be beaten.
                    const int per = maxv(1, int(v / 8));
                    const i64 base = maxv<i64>(1, v - i64(per) * turn);
                    s.perTurn = i16(minv(per, 30000));
                    s.lo = 1;
                    s.hi = i32(friendly(v + per * 3, false));
                    s.value = i32(base);
                    v = minv<i64>(base + i64(per) * turn, s.hi);
                } else if (allow.desert && s.kind == sim::kCamp && !s.fork && (r.f01() < 0.12f || (slot.focus == 3 && !focusDone))) {
                    focusDone = true;
                    const int per = maxv(1, int(v / 10));
                    s.perTurn = i16(-minv(per, 30000));
                    s.lo = i32(maxv<i64>(1, v / 2));
                    s.hi = 0;
                    s.value = i32(v + i64(per) * turn);
                    if (s.value > 99999) s.value = 99999;
                    v = maxv<i64>(s.lo, s.value - i64(per) * turn);
                    if (v > S) v = S;
                } else {
                    s.value = i32(v);
                }
                if (s.kind == sim::kDepot) s.aux = 0;  // set with the Keep
                S += v;
                break;
            }
            case sim::kVillage: {
                const i64 a = friendly(maxv<i64>(2, i64(float(S) * (0.25f + 0.5f * r.f01()))), true);
                s.value = i32(a);
                S += a;
                break;
            }
            case sim::kShrine: {
                s.value = n >= 90 && r.f01() < 0.3f ? 3 : 2;
                if (allow.fade && (r.f01() < 0.4f || slot.focus == 4)) {
                    s.until = i16(turn + r.rangei(1, 3));
                    s.aux = s.value == 3 ? 2 : 1;
                }
                S *= s.value;
                break;
            }
            case sim::kPost: {
                const i64 t = friendly(maxv<i64>(1, i64(float(S) * (0.12f + 0.2f * r.f01()))), true);
                s.value = i32(minv<i64>(t, S - 1));
                if (s.value < 1) s.value = 1;
                S -= s.value;
                break;
            }
            case sim::kMire: {
                s.value = 2;
                S = (S + 1) / 2;
                break;
            }
            default: break;
        }
        if (S > 99999) S = 99999;
        taken[pick] = true;
        st.mine |= bit(pick);
        if (s.fork)
            for (int j = 0; j < g.n; ++j)
                if (j != pick && d.sites[j].fork == s.fork && !taken[j]) st.burnt |= bit(j);
        order[on++] = pick;
        st.turn = u8(on);
    }
    // Sites the walk never reached (burnt forks) still need numbers: like their neighbours.
    for (int i = 1; i < g.n; ++i) {
        sim::Site& s = d.sites[i];
        if (taken[i] || i == g.keep || s.kind == sim::kRivalCamp) continue;
        switch (s.kind) {
            case sim::kVillage: s.value = i32(friendly(maxv<i64>(2, S / 4), true)); break;
            case sim::kShrine: s.value = 2; break;
            case sim::kPost: s.value = i32(friendly(maxv<i64>(1, S / 6), true)); break;
            case sim::kMire: s.value = 2; break;
            default: s.value = i32(friendly(maxv<i64>(1, i64(float(S) * (0.3f + 0.5f * r.f01()))), true)); break;
        }
    }
    // The Keep: a share of what arrives (a Siege's grows each turn; its depots cut it).
    i64 strongest = 1;
    for (int i = 1; i < g.n; ++i)
        if (isCamp(d.sites[i].kind) && i != g.keep) strongest = maxv<i64>(strongest, sim::number(d, st, i));
    if (g.keep >= 0) {
        sim::Site& k = d.sites[g.keep];
        const float f = 0.72f + 0.24f * r.f01();
        i64 v = friendly(maxv<i64>(i64(float(S) * f), strongest * 3 / 2 + 1), true);
        if (v > S) v = S;
        if (slot.kind == sim::kSiege) {
            // It grows each turn; the depots each take a share off it.
            const int turns = on;
            const int per = maxv(1, int(v / 12));
            i64 cuts = 0;
            for (int i = 1; i < g.n; ++i)
                if (d.sites[i].kind == sim::kDepot) {
                    d.sites[i].aux = i32(friendly(maxv<i64>(1, v / 6), true));
                    cuts += d.sites[i].aux;
                }
            k.perTurn = i16(minv(per, 30000));
            k.lo = 1;
            k.hi = 0;
            k.value = i32(maxv<i64>(1, v + cuts - i64(per) * turns));
        } else {
            k.value = i32(v);
        }
        if (k.value > 99999) k.value = 99999;
    }
    // Race: the Warlord starts a little behind the host's middle.
    if (rivalCamp >= 0) {
        d.rivalStart = i32(friendly(maxv<i64>(2, start + (S - start) / 3), true));
        d.sites[rivalCamp].value = d.rivalStart;
    }
    // Last Stand: raids along the intended line's growth.
    if (slot.kind == sim::kLastStand) {
        const int raids = clampv(on / 2, 2, 5);
        i64 hostAt[sim::kMaxSites + 2];
        {
            sim::LevelDef tmp = d;
            sim::State t2 = sim::start(tmp);
            hostAt[0] = t2.s;
            for (int k = 0; k < on; ++k) {
                if (!(sim::takeable(tmp, t2) & bit(order[k]))) break;
                sim::take(tmp, t2, order[k], nullptr);
                hostAt[k + 1] = t2.s;
            }
        }
        int turn = 0;
        for (int k = 0; k < raids; ++k) {
            turn += maxv(1, on / raids);
            const int t = minv(turn, on);
            const i64 w = friendly(maxv<i64>(1, i64(float(hostAt[t]) * (0.45f + 0.4f * r.f01()))), true);
            int entry = 1 + int(r.next() % u32(g.n - 1));
            d.raids[d.raidCount].turn = u8(t);
            d.raids[d.raidCount].entry = u8(entry);
            d.raids[d.raidCount].strength = i32(w);
            ++d.raidCount;
        }
    }
    d.world = slot.world;
    d.hint = slot.hint;
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
    gr.greedyWin = greedyWins(d);
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
    const int p10 = n % 10;
    const bool mustLose = (n > 80 && n <= 160 && (p10 == 2 || p10 == 5 || p10 == 7 || p10 == 9)) ||
                          (n > 160 && (p10 == 1 || p10 == 3 || p10 == 4 || p10 == 6 || p10 == 8 || p10 == 9));
    if (mustLose && gr.greedyWin) {
        std::snprintf(gr.why, sizeof gr.why, "greedy should lose here");
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
