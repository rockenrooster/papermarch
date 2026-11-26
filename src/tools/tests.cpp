// papermarch_tests (TECH_SPEC 9.4): the rules, the solver against brute force, the hand-made levels' stated results,
// the table, the palette lint, the save and the PNG writer. Exit 0 = all passed. Argument: an output folder.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "core/png.h"
#include "core/rng.h"
#include "game/layout.h"
#include "game/save.h"
#include "gen/gen.h"
#include "gfx/palette.h"
#include "sim/level.h"
#include "sim/levels_hand.h"

using namespace pm;

namespace {

int g_failures = 0, g_checks = 0;
void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("FAIL %s\n", what);
    }
}
#define CHECK(cond) check((cond), #cond)
inline u32 bit(int i) { return 1u << u32(i); }

// ---- Brute force: every order, no memo -------------------------------------------------------------------------------

i64 brute(const sim::LevelDef& d, const sim::State& st, int depth) {
    if (st.won()) return st.s;
    if (st.lost() || depth > 40) return -1;
    const u32 moves = sim::takeable(d, st);
    i64 best = -1;
    for (int m = 0; m <= sim::kRivalMove; ++m) {
        if (!(moves & bit(m))) continue;
        sim::State c = st;
        sim::take(d, c, m, nullptr);
        const i64 b = brute(d, c, depth + 1);
        if (b > best) best = b;
    }
    return best;
}

sim::LevelDef randomLevel(Rng& r, int which) {
    sim::LevelBuilder b;
    const int n = r.rangei(4, 8);
    b.site(sim::kHome, 50, 150, 0);
    const bool stand = which % 7 == 3;  // a few Last Stands: raids, no Keep
    for (int i = 1; i < n; ++i) {
        u8 kind = sim::kCamp;
        const int x = r.rangei(0, 99);
        if (x < 15) kind = sim::kVillage;
        else if (x < 25) kind = sim::kShrine;
        else if (x < 33) kind = sim::kPost;
        else if (x < 39) kind = sim::kMire;
        else if (x < 45) kind = sim::kDepot;
        else if (x < 50) kind = sim::kKey;
        const int v = kind == sim::kShrine ? r.rangei(2, 3) : r.rangei(1, 40);
        b.site(kind, 10 + 10 * i, 150 - 15 * i, v);
        if (kind == sim::kCamp && r.f01() < 0.25f) b.grow(i, r.f01() < 0.7f ? r.rangei(1, 5) : -r.rangei(1, 4), 1, r.f01() < 0.5f ? 60 : 0);
        if (kind == sim::kShrine && r.f01() < 0.3f) {
            b.d.sites[i].until = i16(r.rangei(0, 3));
            b.d.sites[i].aux = r.rangei(1, 2);
        }
        if (kind == sim::kDepot) b.d.sites[i].aux = r.rangei(1, 10);
    }
    if (!stand) b.d.sites[n - 1] = {sim::kKeep, 0, 50, 20, r.rangei(10, 120), 0, 1, 0, -1, 0};
    // A spanning tree, then a few more roads.
    for (int i = 1; i < n; ++i) b.road(r.rangei(0, i - 1), i);
    for (int k = 0; k < 2; ++k) {
        const int a = r.rangei(0, n - 1), c = r.rangei(0, n - 1);
        if (a != c && b.d.roadCount < sim::kMaxRoads) b.road(a, c);
    }
    // Forks and a lock.
    if (r.f01() < 0.4f && n > 3) b.fork(1, 1), b.fork(2, 1);
    for (int i = 1; i < n; ++i)
        if (b.d.sites[i].kind == sim::kKey && r.f01() < 0.7f)
            for (int k = 0; k < b.d.roadCount; ++k)
                if (b.d.roads[k].b == n - 1) b.d.roads[k].lock = u8(i);
    // A rival on some, raids on others.
    if (which % 5 == 1 && n > 4) {
        b.d.sites[n - 2].kind = sim::kRivalCamp;
        b.d.rivalStart = r.rangei(5, 30);
        b.d.routeLen = 1;
        b.d.route[0] = u8(n - 2);
        for (int i = 1; i < n - 2 && b.d.routeLen < 3; ++i)
            if (b.d.sites[i].kind == sim::kCamp && !b.d.sites[i].fork) b.d.route[b.d.routeLen++] = u8(i);
    }
    if (stand || which % 6 == 2) {
        const int raids = r.rangei(1, 3);
        int t = 0;
        for (int k = 0; k < raids; ++k) {
            t += r.rangei(1, 3);
            b.raid(t, 1, r.rangei(5, 60));
        }
    }
    b.d.start = r.rangei(3, 25);
    return b.d;
}

void testRules() {
    // A camp: the host takes it if at least as strong, and grows by it.
    sim::LevelBuilder b;
    const int h = b.site(sim::kHome, 50, 150, 0), c = b.site(sim::kCamp, 50, 120, 10), v = b.site(sim::kVillage, 20, 120, 5),
              s = b.site(sim::kShrine, 80, 120, 3), p = b.site(sim::kPost, 50, 90, 4), m = b.site(sim::kMire, 20, 90, 0),
              k = b.site(sim::kKeep, 50, 40, 50);
    b.road(h, c), b.road(h, v), b.road(h, s), b.road(c, p), b.road(v, m), b.road(p, k);
    b.d.start = 10;
    sim::State st = sim::start(b.d);
    CHECK(sim::takeable(b.d, st) == (bit(c) | bit(v) | bit(s)));
    CHECK(sim::effectOf(b.d, st, c) == 20);
    CHECK(sim::effectOf(b.d, st, v) == 15);
    CHECK(sim::effectOf(b.d, st, s) == 30);
    sim::Events ev;
    sim::take(b.d, st, c, &ev);
    CHECK(st.s == 20 && (st.mine & bit(c)) && st.turn == 1);
    CHECK(ev.n >= 1 && ev.e[0].type == sim::kEvTake && ev.e[0].a == 10 && ev.e[0].b == 20);
    CHECK(sim::effectOf(b.d, st, p) == 16);  // a post costs soldiers
    sim::take(b.d, st, v, nullptr);
    CHECK(sim::effectOf(b.d, st, m) == 13);  // a mire halves (rounding up): 25 -> 13
    i64 need = 0;
    CHECK(sim::whyNot(b.d, st, k, &need) == 1);  // not reachable yet
    sim::take(b.d, st, p, nullptr);
    CHECK(st.s == 21);
    CHECK(sim::whyNot(b.d, st, k, &need) == 2 && need == 50);
    sim::take(b.d, st, s, nullptr);
    CHECK(st.s == 63);
    sim::take(b.d, st, k, &ev);
    CHECK(st.won() && st.s == 113);

    // A post needs more than its cost; a strand loses.
    sim::LevelBuilder b2;
    const int h2 = b2.site(sim::kHome, 50, 150, 0), p2 = b2.site(sim::kPost, 50, 100, 10), k2 = b2.site(sim::kKeep, 50, 40, 5);
    b2.road(h2, p2), b2.road(p2, k2);
    b2.d.start = 10;
    st = sim::start(b2.d);
    CHECK(sim::takeable(b2.d, st) == 0);
    // Nothing takeable at the start is not a loss until a move ends; the solver says unwinnable.
    sim::Solver solver;
    CHECK(!solver.run(b2.d, st).winnable);

    // A fork burns the others; a key tower opens its roads.
    sim::LevelBuilder b3;
    const int h3 = b3.site(sim::kHome, 50, 150, 0), l = b3.site(sim::kCamp, 20, 110, 3), r = b3.site(sim::kVillage, 80, 110, 2),
              t = b3.site(sim::kKey, 50, 110, 5), k3 = b3.site(sim::kKeep, 50, 40, 10);
    b3.road(h3, l), b3.road(h3, r), b3.road(h3, t), b3.road(l, k3, t), b3.road(r, k3, t);
    b3.fork(l, 1), b3.fork(r, 1);
    b3.d.start = 6;
    st = sim::start(b3.d);
    sim::take(b3.d, st, l, &ev);
    CHECK((st.burnt & bit(r)) && !(sim::reachable(b3.d, st) & bit(k3)));
    sim::take(b3.d, st, t, &ev);
    CHECK(sim::reachable(b3.d, st) & bit(k3));
    bool opened = false;
    for (int i = 0; i < ev.n; ++i) opened = opened || ev.e[i].type == sim::kEvOpen;
    CHECK(opened);

    // A growing camp and a deserting one; a fading shrine.
    sim::LevelBuilder b4;
    const int h4 = b4.site(sim::kHome, 50, 150, 0), g = b4.site(sim::kCamp, 20, 110, 4), dd = b4.site(sim::kCamp, 80, 110, 20),
              f = b4.site(sim::kShrine, 50, 110, 3), x = b4.site(sim::kVillage, 50, 80, 1);
    b4.grow(g, 3, 1, 10);
    b4.grow(dd, -4, 9, 0);
    b4.d.sites[f].until = 1;
    b4.d.sites[f].aux = 2;
    b4.road(h4, g), b4.road(h4, dd), b4.road(h4, f), b4.road(h4, x);
    b4.d.start = 5;
    st = sim::start(b4.d);
    CHECK(sim::number(b4.d, st, g) == 4 && sim::number(b4.d, st, dd) == 20 && sim::number(b4.d, st, f) == 3);
    st.turn = 2;
    CHECK(sim::number(b4.d, st, g) == 10 && sim::number(b4.d, st, dd) == 12 && sim::number(b4.d, st, f) == 2);
    st.turn = 5;
    CHECK(sim::number(b4.d, st, g) == 10 && sim::number(b4.d, st, dd) == 9);

    // Depots cut the Keep (never below 1).
    sim::LevelBuilder b5;
    const int h5 = b5.site(sim::kHome, 50, 150, 0), dp = b5.site(sim::kDepot, 50, 110, 5), k5 = b5.site(sim::kKeep, 50, 40, 30);
    b5.d.sites[dp].aux = 12;
    b5.road(h5, dp), b5.road(dp, k5);
    b5.d.start = 6;
    st = sim::start(b5.d);
    CHECK(sim::number(b5.d, st, k5) == 30);
    sim::take(b5.d, st, dp, nullptr);
    CHECK(sim::number(b5.d, st, k5) == 18);

    // Raids: one too strong ends the level; a Last Stand is won when the last is absorbed.
    sim::LevelBuilder b6;
    const int h6 = b6.site(sim::kHome, 50, 150, 0), c6 = b6.site(sim::kCamp, 50, 110, 4), v6 = b6.site(sim::kVillage, 20, 110, 6);
    b6.road(h6, c6), b6.road(h6, v6);
    b6.raid(1, c6, 12);
    b6.raid(2, v6, 30);
    b6.d.start = 8;
    st = sim::start(b6.d);
    sim::take(b6.d, st, c6, nullptr);  // 12: the first raid joins -> 24
    CHECK(st.s == 24 && st.raidNext == 1);
    sim::take(b6.d, st, v6, nullptr);  // 30 meets the second: 60, won
    CHECK(st.won() && st.s == 60);
    st = sim::start(b6.d);
    sim::take(b6.d, st, v6, nullptr);  // 14 meets 12: 26; then nothing to take: turn 2 comes by itself, 26 < 30: lost
    CHECK(st.raidNext == 1 || st.over());

    // The rival: it takes camps on its route, then marches on Home.
    sim::LevelBuilder b7;
    const int h7 = b7.site(sim::kHome, 50, 150, 0), a7 = b7.site(sim::kCamp, 20, 110, 5), r7 = b7.site(sim::kCamp, 80, 110, 6),
              w7 = b7.site(sim::kRivalCamp, 80, 60, 10), k7 = b7.site(sim::kKeep, 50, 40, 30);
    b7.road(h7, a7), b7.road(h7, r7), b7.road(r7, w7), b7.road(a7, k7), b7.road(w7, k7);
    b7.d.start = 10;
    b7.d.rivalStart = 10;
    b7.d.routeLen = 2;
    b7.d.route[0] = u8(w7);
    b7.d.route[1] = u8(r7);
    st = sim::start(b7.d);
    CHECK(st.rivalAlive() && sim::rivalSite(b7.d, st) == w7);
    sim::take(b7.d, st, a7, nullptr);  // 15; the rival takes r7: 16
    CHECK(st.s == 15 && st.rival == 16 && (st.razed & bit(r7)));
    // Next: the rival marches on Home (at the gates), then arrives: 15 < 16 loses unless the host grows.
    CHECK(solver.run(b7.d, sim::start(b7.d)).winnable);
}

void testHandLevels() {
    struct Want {
        int n;
        i64 best;
    } wants[] = {{1, 43}, {2, 58}, {3, 45}, {4, 80}, {5, 87}, {6, 61}, {12, 396}};
    sim::Solver solver;
    for (const Want& w : wants) {
        sim::LevelDef d;
        if (!sim::handLevel(w.n, d)) {
            check(false, "a hand-made level is missing");
            continue;
        }
        const sim::Solve s = solver.run(d, sim::start(d));
        char what[64];
        std::snprintf(what, sizeof what, "hand-made level %d: best %lld (want %lld)", w.n, (long long)s.best, (long long)w.best);
        check(s.winnable && s.proven && s.best == w.best, what);
    }
    // Level 4: the shrine first strands at 26; level 6: the village side strands at 22; level 12's two lines.
    sim::LevelDef d;
    sim::handLevel(4, d);
    sim::State st = sim::start(d);
    sim::take(d, st, 2, nullptr);  // the shrine
    sim::take(d, st, 1, nullptr);
    sim::take(d, st, 3, nullptr);
    CHECK(st.lost() && st.s == 26);
    sim::handLevel(6, d);
    st = sim::start(d);
    sim::take(d, st, 1, nullptr);
    sim::take(d, st, 3, nullptr);
    CHECK(st.lost() && st.s == 22);
    sim::handLevel(12, d);
    st = sim::start(d);
    const int obvious[] = {1, 2, 4, 3, 5, 6, 7, 8, 9};
    for (int m : obvious) sim::take(d, st, m, nullptr);
    CHECK(st.won() && st.s == 285);
    st = sim::start(d);
    const int best[] = {1, 2, 3, 5, 6, 7, 8, 4, 9};
    for (int m : best) sim::take(d, st, m, nullptr);
    CHECK(st.won() && st.s == 396);
    st = sim::start(d);
    sim::take(d, st, 1, nullptr);
    sim::take(d, st, 2, nullptr);
    CHECK(st.s == 24 && sim::number(d, st, 8) == 18);  // the concept frame
}

void testSolverAgainstBruteForce() {
    sim::Solver solver;
    Rng r(4242, 3);
    int mismatches = 0, winnable = 0;
    for (int i = 0; i < 2000; ++i) {
        const sim::LevelDef d = randomLevel(r, i);
        const sim::State st = sim::start(d);
        const sim::Solve s = solver.run(d, st);
        const i64 b = brute(d, st, 0);
        const bool ok = s.proven && (b < 0 ? !s.winnable : (s.winnable && s.best == b));
        if (s.winnable) ++winnable;
        if (!ok) {
            ++mismatches;
            if (mismatches <= 5) {
                char buf[2048];
                sim::encodeLevel(d, buf, sizeof buf);
                std::printf("solver %lld (winnable %d) vs brute %lld on: %s\n", (long long)s.best, s.winnable, (long long)b, buf);
            }
        }
        // The first move it names reaches the best.
        if (s.winnable && s.first >= 0) {
            sim::State c = st;
            sim::take(d, c, s.first, nullptr);
            const i64 after = brute(d, c, 0);
            if (after != s.best) ++mismatches;
        }
    }
    char what[96];
    std::snprintf(what, sizeof what, "solver = brute force on 2000 random levels (%d winnable, %d mismatches)", winnable, mismatches);
    check(mismatches == 0, what);
}

void testEncode() {
    sim::LevelDef d;
    sim::handLevel(12, d);
    d.best = 396;
    d.raidCount = 1;
    d.raids[0] = {3, 2, 77};
    char buf[4096];
    const int n = sim::encodeLevel(d, buf, sizeof buf);
    sim::LevelDef e;
    CHECK(n > 0 && sim::decodeLevel(buf, e));
    char buf2[4096];
    sim::encodeLevel(e, buf2, sizeof buf2);
    CHECK(!std::strcmp(buf, buf2));
    CHECK(e.siteCount == d.siteCount && e.sites[8].perTurn == 3 && e.sites[8].hi == 45 && e.best == 396 && e.raids[0].strength == 77);
}

void testTable() {
    const int count = sim::levelCount();
    sim::Solver solver;
    int bad = 0;
    std::vector<u64> fps;
    int repeats = 0;
    for (int n = 1; n <= count; ++n) {
        const sim::LevelDef& d = sim::level(n);
        const sim::Solve s = solver.run(d, sim::start(d));
        if (!s.winnable || !s.proven || s.best != d.best) {
            ++bad;
            if (bad < 5) std::printf("level %d: best %lld, the table says %lld\n", n, (long long)s.best, (long long)d.best);
        }
        const u64 fp = gen::fingerprint(d);
        for (u64 f : fps) repeats += f == fp;
        fps.push_back(fp);
    }
    char what[96];
    std::snprintf(what, sizeof what, "every level of the table (%d) is winnable with its stored best", count);
    check(bad == 0, what);
    std::snprintf(what, sizeof what, "no two levels share a layout fingerprint (%d repeats)", repeats);
    check(repeats == 0, what);
}

void testPalettes() {
    for (int w = 1; w <= kWorldCount; ++w) {
        const WorldPalette& p = worldPalette(w);
        char what[96];
        const float lum = luminance(p.terrace1);
        std::snprintf(what, sizeof what, "%s: middle terrace luminance %.2f in [0.25, 0.72]", p.name, double(lum));
        check(lum >= 0.25f && lum <= 0.72f, what);
        std::snprintf(what, sizeof what, "%s: ivory on the terrace %.2f >= 1.25", p.name, double(contrast(col::kIvory, p.terrace1)));
        check(contrast(col::kIvory, p.terrace1) >= 1.25f, what);
        std::snprintf(what, sizeof what, "%s: ink on the terrace %.2f >= 3", p.name, double(contrast(col::kInk, p.terrace1)));
        check(contrast(col::kInk, p.terrace1) >= 3.0f, what);
        const WorldPalette& q = worldPalette(w % kWorldCount + 1);
        const float dh = std::fabs(hueOf(p.land) - hueOf(q.land));
        const float hue = dh > 180 ? 360 - dh : dh;
        std::snprintf(what, sizeof what, "%s and %s differ (land hue %.0f, page luminance %.2f)", p.name, q.name, double(hue),
                      double(std::fabs(luminance(p.page) - luminance(q.page))));
        check(hue >= 30.0f || std::fabs(luminance(p.page) - luminance(q.page)) >= 0.2f, what);
    }
}

void testSave(const char* dir) {
    char path[512];
    std::snprintf(path, sizeof path, "%s/test_save.txt", dir);
    SaveData s;
    s.level = 37;
    s.stars[0] = 3;
    s.stars[36] = 2;
    s.fast = true;
    s.finished = true;
    s.hints = 0x55;
    CHECK(writeSave(path, s));
    SaveData t;
    CHECK(loadSave(path, t));
    CHECK(t.level == 37 && t.stars[0] == 3 && t.stars[36] == 2 && t.fast && t.finished && t.hints == 0x55 && t.totalStars() == 5);
}

void testFormat() {
    char buf[32];
    formatThousands(1234567, buf, sizeof buf);
    CHECK(!std::strcmp(buf, "1,234,567"));
    formatHost(999999, buf, sizeof buf);
    CHECK(!std::strcmp(buf, "999,999"));
    formatHost(1240000, buf, sizeof buf);
    CHECK(!std::strcmp(buf, "1.24M"));
}

}  // namespace

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : ".";
    {
        u8 px[3 * 2 * 4] = {};
        px[(1 * 3 + 2) * 4] = 200;  // (2, 1) red 200
        for (int i = 0; i < 6; ++i) px[i * 4 + 3] = 255;
        char path[512];
        std::snprintf(path, sizeof path, "%s/test_png.png", dir);
        CHECK(writePng(path, px, 3, 2, false));
    }
    testRules();
    testHandLevels();
    testEncode();
    testSolverAgainstBruteForce();
    testTable();
    testPalettes();
    testSave(dir);
    testFormat();
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
