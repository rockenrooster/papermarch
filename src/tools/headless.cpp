// papermarch_headless (TECH_SPEC 2): the rules, the solver, the generator and the bake, and offline audio, with no window.
//   --solve --level N                 the best line of level N, move by move
//   --sim table [--from A --to B]     every level: kind, recipe, sites, best, random and greedy plays, solver cost
//   --sim tablehash                   the determinism hash (the app's sim_hash prints the same)
//   --gen --level N --recipe R --seed S   one generated candidate, its grade and its text
//   --bake-levels --from A --to B [--keep-to C] [--jobs J] [--seeds S] --out src\sim\levels_baked.h --report r.md
//   --render-audio NAME --out x.wav [--world N] [--layers K] [--boss] [--seconds S]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/rng.h"
#include "core/tuning.h"
#include "gen/gen.h"
#include "sim/level.h"
#include "sim/levels_hand.h"
#include "tools/audio_render.h"

using namespace pm;

namespace {

const char* arg(int argc, char** argv, const char* key, const char* def = nullptr) {
    for (int i = 1; i + 1 < argc; ++i)
        if (!std::strcmp(argv[i], key)) return argv[i + 1];
    return def;
}
bool flag(int argc, char** argv, const char* key) {
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], key)) return true;
    return false;
}
int argi(int argc, char** argv, const char* key, int def) {
    const char* v = arg(argc, argv, key);
    return v ? std::atoi(v) : def;
}
double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
const char* kindShort(u8 k) {
    switch (k) {
        case sim::kGauntlet: return "gauntlet";
        case sim::kSiege: return "siege";
        case sim::kRace: return "race";
        case sim::kLastStand: return "laststand";
        default: return "conquest";
    }
}

int cmdSolve(int n) {
    const sim::LevelDef& d = sim::level(n);
    sim::Solver solver;
    sim::State st = sim::start(d);
    const double t0 = nowSec();
    const sim::Solve s = solver.run(d, st);
    std::printf("level %d: %s, %d sites, start %d: best %lld, winnable %d, proven %d, %d expansions, %.2f ms\n", n, kindShort(d.kind),
                d.siteCount, d.start, (long long)s.best, s.winnable, s.proven, s.expansions, (nowSec() - t0) * 1000.0);
    while (!st.over()) {
        const sim::Solve here = solver.run(d, st);
        if (here.first < 0) break;
        const i64 before = st.s;
        sim::take(d, st, here.first, nullptr);
        if (here.first == sim::kRivalMove) std::printf("  strike the Warlord: %lld -> %lld\n", (long long)before, (long long)st.s);
        else std::printf("  %2d %-8s %lld -> %lld (turn %d)\n", here.first, sim::kindName(d.sites[here.first].kind), (long long)before,
                         (long long)st.s, st.turn);
    }
    std::printf("  %s with %lld\n", st.won() ? "won" : "lost", (long long)st.s);
    return st.won() ? 0 : 1;
}

int cmdTable(int from, int to, bool hashOnly) {
    sim::Solver solver;
    u64 h = 1469598103934665603ULL;
    int failures = 0;
    to = std::min(to, sim::levelCount());
    if (!hashOnly) std::printf("level kind      recipe        sites start   best      exp   us   rand%% 3star%% greedy decision\n");
    for (int n = from; n <= to; ++n) {
        const sim::LevelDef& d = sim::level(n);
        const double t0 = nowSec();
        const sim::Solve s = solver.run(d, sim::start(d));
        const double us = (nowSec() - t0) * 1e6;
        h = (h ^ u64(s.best)) * 1099511628211ULL;
        h = (h ^ u64(s.first + 1)) * 1099511628211ULL;
        if (hashOnly) continue;
        const gen::PlayStats p = gen::randomPlays(d, 400, u64(n) * 9176 + 7);
        const bool g = gen::greedyWins(d);
        const bool dec = gen::hasDecision(d, solver);
        const bool ok = s.winnable && s.proven && s.best == d.best;
        if (!ok) ++failures;
        std::printf("%5d %-9s %-13s %5d %5d %6lld %8d %4.0f %5d %6d %6s %8s%s\n", n, kindShort(d.kind),
                    d.recipe < gen::kRecipeCount ? gen::recipeName(d.recipe) : "HAND", d.siteCount, d.start, (long long)s.best, s.expansions,
                    us, p.wins * 100 / std::max(1, p.plays), p.threeStars * 100 / std::max(1, p.plays), g ? "wins" : "loses",
                    dec ? "yes" : "no", ok ? "" : "  <-- best differs or unproven");
    }
    std::printf("tablehash %016llx over %d levels\n", (unsigned long long)h, to - from + 1);
    return failures ? 1 : 0;
}

// ---- The bake (TECH_SPEC 4) --------------------------------------------------------------------------------------------

struct Candidate {
    sim::LevelDef d;
    gen::Grade g;
    u64 fp = 0;
    u32 elems = 0;
};

u32 elementSet(const sim::LevelDef& d) {
    u32 e = 0;
    for (int i = 0; i < d.siteCount; ++i) {
        e |= 1u << d.sites[i].kind;
        if (d.sites[i].perTurn > 0) e |= 1u << 16;
        if (d.sites[i].perTurn < 0) e |= 1u << 17;
        if (d.sites[i].fork) e |= 1u << 18;
        if (d.sites[i].until >= 0) e |= 1u << 19;
    }
    return e;
}

const int* recipesFor(u8 kind, int& count) {
    static const int conquest[] = {gen::kTree, gen::kLadder, gen::kRing, gen::kHourglass, gen::kIslands, gen::kCrossroads,
                                   gen::kTwinValleys, gen::kSpiral};
    static const int gauntlet[] = {gen::kPairs, gen::kTriples, gen::kBraid};
    static const int siege[] = {gen::kRingedKeep, gen::kSupplyLine};
    static const int race[] = {gen::kParallel, gen::kPincer};
    static const int stand[] = {gen::kThreeRoads, gen::kHomestead};
    switch (kind) {
        case sim::kGauntlet: count = 3; return gauntlet;
        case sim::kSiege: count = 2; return siege;
        case sim::kRace: count = 2; return race;
        case sim::kLastStand: count = 2; return stand;
        default: count = 8; return conquest;
    }
}

int cmdBake(int argc, char** argv) {
    const int from = argi(argc, argv, "--from", 1), to = argi(argc, argv, "--to", 300);
    const int keepTo = argi(argc, argv, "--keep-to", to);
    const int jobs = std::max(1, std::min(4, argi(argc, argv, "--jobs", 4)));
    const int maxSeeds = argi(argc, argv, "--seeds", 600);
    const int perLevel = 4;  // accepted candidates kept per level for the no-repeat pass
    const char* out = arg(argc, argv, "--out", "src/sim/levels_baked.h");
    const char* report = arg(argc, argv, "--report", "captures/levels/report.md");
    const double t0 = nowSec();
    // Existing levels outside [from, to] are kept as they are (up to keepTo).
    std::vector<sim::LevelDef> table(size_t(std::max(to, keepTo)) + 1);
    std::vector<int> have(table.size(), 0);
    for (int n = 1; n <= std::min(sim::levelCount(), keepTo); ++n)
        if (n < from || n > to) {
            table[size_t(n)] = sim::level(n);
            have[size_t(n)] = 1;
        }
    std::vector<std::vector<Candidate>> cands(table.size());
    std::vector<std::string> notes(table.size());
    std::mutex mu;
    int nextLevel = from;
    auto worker = [&]() {
        sim::Solver solver;
        for (;;) {
            int n;
            {
                std::lock_guard<std::mutex> lock(mu);
                if (nextLevel > to) return;
                n = nextLevel++;
            }
            std::vector<Candidate> found;
            char note[160] = "";
            sim::LevelDef hand;
            if (sim::handLevel(n, hand)) {
                Candidate c;
                c.d = hand;
                c.g = gen::grade(n, c.d, solver, true);
                c.fp = gen::fingerprint(c.d);
                c.elems = elementSet(c.d);
                if (!c.g.best) std::snprintf(note, sizeof note, "HAND LEVEL NOT WINNABLE: %s", c.g.why);
                found.push_back(c);
            } else {
                const gen::Slot slot = gen::calendar(n);
                int rc = 0;
                const int* rs = recipesFor(slot.kind, rc);
                std::vector<Candidate> closest;  // the nearest misses, for a level no candidate passes
                int tried = 0;
                for (int seed = 0; seed < maxSeeds && int(found.size()) < perLevel; ++seed) {
                    Candidate c;
                    const int recipe = rs[(n * 7 + seed) % rc];
                    if (!gen::generate(n, slot, recipe, hash64(u64(n) * 1000003ULL + u64(seed)), c.d)) continue;
                    ++tried;
                    c.g = gen::grade(n, c.d, solver, false);
                    if (c.g.ok) {
                        c.fp = gen::fingerprint(c.d);
                        c.elems = elementSet(c.d);
                        found.push_back(c);
                    } else if (c.g.best > 0 && c.g.decision) {
                        c.fp = gen::fingerprint(c.d);
                        c.elems = elementSet(c.d);
                        closest.push_back(c);
                        std::sort(closest.begin(), closest.end(), [](const Candidate& a, const Candidate& b) {
                            return std::abs(a.g.randomWinPct - 50) + a.g.randomThreePct < std::abs(b.g.randomWinPct - 50) + b.g.randomThreePct;
                        });
                        if (closest.size() > 40) closest.resize(40);
                    }
                }
                if (found.empty()) {
                    if (!closest.empty()) {
                        found = closest;
                        std::snprintf(note, sizeof note, "band relaxed after %d candidates: %s", tried, closest[0].g.why);
                    } else {
                        std::snprintf(note, sizeof note, "NO WINNABLE CANDIDATE in %d", tried);
                    }
                }
            }
            std::lock_guard<std::mutex> lock(mu);
            cands[size_t(n)] = found;
            notes[size_t(n)] = note;
            std::fprintf(stderr, "\r[bake] level %d: %zu candidates   ", n, found.size());
        }
    };
    std::vector<std::thread> threads;
    for (int j = 0; j < jobs; ++j) threads.emplace_back(worker);
    for (auto& t : threads) t.join();
    std::fprintf(stderr, "\n");
    // The no-repeat pass, in order: a fingerprint used once in the table; within 12 levels no recipe with the same elements.
    std::vector<u64> fps;
    for (int n = 1; n < int(table.size()); ++n)
        if (have[size_t(n)]) fps.push_back(gen::fingerprint(table[size_t(n)]));
    int failed = 0;
    sim::Solver passSolver;
    for (int n = from; n <= to; ++n) {
        auto& cs = cands[size_t(n)];
        auto clashes = [&](const Candidate& c) {
            bool clash = std::find(fps.begin(), fps.end(), c.fp) != fps.end();
            for (int m = std::max(1, n - 11); m < n && !clash; ++m)
                if (have[size_t(m)] && table[size_t(m)].recipe == c.d.recipe && table[size_t(m)].recipe < gen::kRecipeCount &&
                    elementSet(table[size_t(m)]) == c.elems)
                    clash = true;
            return clash;
        };
        int pick = -1;
        for (int k = 0; k < int(cs.size()) && pick < 0; ++k)
            if (!clashes(cs[size_t(k)]) || sim::handLevel(n, table[0])) pick = k;
        // Every candidate repeats an earlier level: search on, now that the earlier levels are known.
        if (pick < 0 && !sim::handLevel(n, table[0])) {
            const gen::Slot slot = gen::calendar(n);
            int rc = 0;
            const int* rs = recipesFor(slot.kind, rc);
            for (int seed = maxSeeds; seed < maxSeeds * 4 && pick < 0; ++seed) {
                Candidate c;
                const int recipe = rs[(n * 7 + seed) % rc];
                if (!gen::generate(n, slot, recipe, hash64(u64(n) * 1000003ULL + u64(seed)), c.d)) continue;
                c.g = gen::grade(n, c.d, passSolver, false);
                if (!c.g.ok) continue;
                c.fp = gen::fingerprint(c.d);
                c.elems = elementSet(c.d);
                if (clashes(c)) continue;
                cs.push_back(c);
                pick = int(cs.size()) - 1;
            }
        }
        if (pick < 0 && !cs.empty()) {
            pick = 0;
            notes[size_t(n)] += notes[size_t(n)].empty() ? "repeats a neighbour (no other candidate)" : "; repeats a neighbour";
        }
        if (pick < 0) {
            ++failed;
            continue;
        }
        table[size_t(n)] = cs[size_t(pick)].d;
        have[size_t(n)] = 1;
        fps.push_back(cs[size_t(pick)].fp);
    }
    // The table: levels 1.. in order (a gap stops it).
    int last = 0;
    while (last + 1 < int(table.size()) && have[size_t(last + 1)]) ++last;
    FILE* f = std::fopen(out, "wb");
    if (!f) {
        std::fprintf(stderr, "bake: cannot write %s\n", out);
        return 2;
    }
    std::fprintf(f, "// Generated by papermarch_headless --bake-levels (TECH_SPEC 4) - do not edit. %d levels.\n", last);
    std::fprintf(f, "static const char* const kBaked[] = {\n");
    char buf[4096];
    for (int n = 1; n <= last; ++n) {
        const int len = sim::encodeLevel(table[size_t(n)], buf, sizeof buf);
        if (len <= 0) {
            std::fclose(f);
            std::fprintf(stderr, "bake: level %d does not encode\n", n);
            return 2;
        }
        std::fprintf(f, "    \"%s\",\n", buf);
    }
    std::fprintf(f, "};\nstatic const int kBakedCount = %d;\n", last);
    std::fclose(f);
    // The report.
    FILE* r = std::fopen(report, "wb");
    if (r) {
        std::fprintf(r, "# The bake, levels %d-%d (%.1f s on %d jobs)\n\n", from, to, nowSec() - t0, jobs);
        std::fprintf(r, "| Level | Kind | Recipe | Sites | Start | Best | Random wins | Random 3 stars | Greedy | Decision | Solver | Note |\n");
        std::fprintf(r, "|---|---|---|---|---|---|---|---|---|---|---|---|\n");
        sim::Solver solver;
        for (int n = from; n <= std::min(to, last); ++n) {
            sim::LevelDef d = table[size_t(n)];
            const gen::Grade g = gen::grade(n, d, solver, true);
            std::fprintf(r, "| %d | %s%s | %s | %d | %d | %lld | %d%% | %d%% | %s | %s | %d | %s |\n", n, kindShort(d.kind), d.boss ? " (boss)" : "",
                         d.recipe < gen::kRecipeCount && !sim::handLevel(n, table[0]) ? gen::recipeName(d.recipe) : "hand", d.siteCount, d.start,
                         (long long)d.best, g.randomWinPct, g.randomThreePct, g.greedyWin ? "wins" : "loses", g.decision ? "yes" : "no",
                         g.expansions, notes[size_t(n)].c_str());
        }
        std::fclose(r);
    }
    std::printf("bake: %d levels written to %s in %.1f s (%d failed)\n", last, out, nowSec() - t0, failed);
    return failed ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i)
        if (!std::strcmp(argv[i], "--set") && !setTuningPair(argv[i + 1])) {
            std::fprintf(stderr, "--set %s: unknown name or bad value\n", argv[i + 1]);
            return 2;
        }
    if (flag(argc, argv, "--solve")) return cmdSolve(argi(argc, argv, "--level", 1));
    if (const char* sim = arg(argc, argv, "--sim")) {
        if (!std::strcmp(sim, "table")) return cmdTable(argi(argc, argv, "--from", 1), argi(argc, argv, "--to", 9999), false);
        if (!std::strcmp(sim, "tablehash")) return cmdTable(1, 9999, true);
        std::fprintf(stderr, "unknown --sim %s\n", sim);
        return 2;
    }
    if (flag(argc, argv, "--gen")) {
        const int n = argi(argc, argv, "--level", 20);
        sim::LevelDef d;
        gen::Slot slot = gen::calendar(n);
        if (!gen::generate(n, slot, argi(argc, argv, "--recipe", 0), u64(argi(argc, argv, "--seed", 1)), d)) {
            std::printf("the recipe could not place it\n");
            return 1;
        }
        sim::Solver solver;
        const gen::Grade g = gen::grade(n, d, solver, false);
        char buf[4096];
        sim::encodeLevel(d, buf, sizeof buf);
        std::printf("%s\nok %d best %lld random %d%% three %d%% greedy %s decision %s exp %d %s\n", buf, g.ok, (long long)g.best,
                    g.randomWinPct, g.randomThreePct, g.greedyWin ? "wins" : "loses", g.decision ? "yes" : "no", g.expansions, g.why);
        return g.ok ? 0 : 1;
    }
    if (flag(argc, argv, "--bake-levels")) return cmdBake(argc, argv);
    if (const char* name = arg(argc, argv, "--render-audio")) {
        const char* out = arg(argc, argv, "--out", "out.wav");
        const char* sec = arg(argc, argv, "--seconds");
        return renderAudio(name, out, argi(argc, argv, "--world", 1), argi(argc, argv, "--layers", 5), flag(argc, argv, "--boss"),
                           sec ? float(std::atof(sec)) : 0.0f);
    }
    std::fprintf(stderr, "usage: see the comment at the top of src/tools/headless.cpp\n");
    return 2;
}
