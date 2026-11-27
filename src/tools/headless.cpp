// papermarch_headless (TECH_SPEC 2): the battle, the bot, the generator and the bake, and offline audio, with no window.
//   --play --level N                  the bot plays level N: the squad over time, the result
//   --sim table [--from A --to B]     every level: length, waves, boss, the bot's peak and end, stars
//   --bake-levels --from A --to B [--jobs J] [--seeds S] --out src\sim\levels_baked.h --report r.md
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
#include "sim/battle.h"
#include "sim/bot.h"
#include "sim/endless.h"
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

// --steer X: hold the squad at X instead of the bot's steering (a player who never moves), to reproduce a run.
int cmdPlay(int n, float steer) {
    const sim::LevelDef& d = sim::level(n);
    static sim::Battle b;
    b.init(d);
    sim::BattleEvents ev;
    int lastSecond = -1, kills = 0, lost = 0, shots = 0;
    while (!b.won && !b.lost && b.time < 3600) {
        b.tick(steer > -99.0f ? steer : sim::botSteer(b), true, ev);
        kills += ev.kills;
        if (ev.blow) std::printf("  %5.1f s blow at %.1f: -%d (squad %d)\n", double(b.time), double(ev.blowX), int(ev.blowLost), int(b.squad));
        lost += ev.lost;
        shots += ev.shots;
        if (ev.gate >= 0) std::printf("  %5.1f s gate %d: %d -> %d\n", double(b.time), ev.gate, ev.gateBefore, ev.gateAfter);
        if (ev.barrel >= 0) std::printf("  %5.1f s barrel %d: +%d\n", double(b.time), ev.barrel, ev.barrelReward);
        if (ev.wave >= 0) std::printf("  %5.1f s wave %d wakes (%d)\n", double(b.time), ev.wave, d.waves[ev.wave].count);
        if (int(b.time) != lastSecond && int(b.time) % 5 == 0) {
            lastSecond = int(b.time);
            std::printf("  %5.1f s squad %d, z %.1f, enemies %d\n", double(b.time), b.squad, double(b.z), b.aliveEnemies());
        }
    }
    std::printf("level %d: %s at %.1f s with %d (peak %d); %d kills, %d lost, %d shots; stars %d (2 at %d, 3 at %d)\n", n,
                b.won ? "won" : "lost", double(b.time), b.squad, b.peak, kills, lost, shots, sim::starsFor(d, b.squad), d.star2, d.star3);
    return b.won ? 0 : 1;
}

int cmdTable(int from, int to) {
    to = std::min(to, sim::levelCount());
    int failures = 0;
    std::printf("level  length waves enemies boss  start  bot peak  bot end  stars2  stars3  result\n");
    for (int n = from; n <= to; ++n) {
        const sim::LevelDef& d = sim::level(n);
        i32 left = 0, peak = 0;
        const bool won = sim::botPlay(d, &left, &peak);
        int enemies = 0;
        for (int w = 0; w < d.waveCount; ++w) enemies += d.waves[w].count;
        const bool ok = won && (d.botEnd == 0 || left == d.botEnd);
        if (!ok) ++failures;
        std::printf("%5d %7.0f %5d %7d %5d %6d %9d %8d %7d %7d  %s%s\n", n, double(d.length), d.waveCount, enemies, d.bossHp, d.start, peak,
                    left, d.star2, d.star3, ok ? "ok" : (won ? "end differs" : "LOST"), d.spike ? " (hard)" : "");
    }
    return failures ? 1 : 0;
}

// The dial against the best run, for one level and seed: how the margin moves (tuning the bake).
int cmdSweep(int n, int s) {
    const u64 seed = hash64(u64(n) * 1000003ULL + u64(s));
    std::printf("level %d seed %d: target %.3f%s\n   dial  won  end  peak  kept   enemies  boss  look\n", n, s, double(gen::marginTarget(n)),
                gen::isSpike(n) ? " (hard)" : "");
    static gen::Shape shape;
    gen::build(n, seed, shape);
    std::printf("   (the road built: the bot's squad at its end %.0f, %s)\n", double(shape.endSquad), shape.boss ? "a boss" : "a last stand");
    for (float m = 0.1f; m <= 90.0f; m *= 1.45f) {
        static sim::LevelDef d;
        gen::finish(shape, m, d);
        const sim::PlayResult r = sim::planLevel(d);
        int enemies = 0;
        for (int w = 0; w < d.waveCount; ++w) enemies += d.waves[w].count;
        std::printf("  %5.2f  %3s %4d %5d  %.3f  %7d %5d  %4.0f\n", double(m), r.won ? "yes" : "no", r.left, r.peak,
                    double(r.won ? float(r.left) / float(maxv<i32>(r.peak, 1)) : 0.0f), enemies, d.bossHp,
                    double(d.planLook > 0 ? d.planLook : 15.0f));
    }
    return 0;
}

// Endless with the bot: stage after stage, the squad carried over, until it falls (or `maxStages`).
int cmdEndless(u64 seed, int maxStages) {
    static sim::LevelDef d[2];
    static sim::Battle b[2];
    int cur = 0;
    i32 squad = 5;
    float total = 0, distance = 0;
    for (int k = 1; k <= maxStages; ++k) {
        static sim::Endless gen;
        gen.begin(k, seed * 1000003ULL + u64(k), d[cur]);
        b[cur].init(d[cur]);
        if (k > 1) b[cur].carry(b[1 - cur]);
        else b[cur].squad = b[cur].peak = squad;
        gen.extend(d[cur], b[cur]);
        sim::BattleEvents ev;
        int enemies = 0;
        for (int w = 0; w < d[cur].waveCount; ++w) enemies += d[cur].waves[w].count;
        const i32 in = b[cur].squad;
        static const bool trace = std::getenv("PM_ENDLESS_TRACE") != nullptr;
        int lastSecond = -1;
        while (!b[cur].won && !b[cur].lost && b[cur].time < 600) {
            b[cur].tick(sim::botSteer(b[cur]), true, ev);
            gen.extend(d[cur], b[cur]);
            if (trace && (int(b[cur].time) != lastSecond || ev.lost > 20)) {
                lastSecond = int(b[cur].time);
                std::printf("    %5.1f s z %5.1f squad %6d alive %4d lost %d gate %d crashed %d mine %d strike %d blow %d\n", double(b[cur].time),
                            double(b[cur].z), int(b[cur].squad), b[cur].aliveEnemies(), int(ev.lost), ev.gate, ev.crashed, int(ev.mineStepped),
                            ev.strikeLanded, int(ev.blow));
            }
        }
        total += b[cur].time;
        distance += b[cur].z;
        std::printf("stage %2d (world %d, %s): %.0f long, %d enemies, boss %d; in %d, peak %d, out %d, %.0f s%s\n", k, d[cur].world,
                    d[cur].bossHp > 0 ? "boss" : "stand", double(d[cur].length), enemies, d[cur].bossHp, int(in), int(b[cur].peak), int(b[cur].squad),
                    double(b[cur].time), b[cur].won ? "" : "  FALLEN");
        if (!b[cur].won) {
            std::printf("endless: fell on stage %d after %.0f s, %.0f units\n", k, double(total), double(distance));
            return 0;
        }
        squad = b[cur].squad;
        cur = 1 - cur;
    }
    std::printf("endless: all %d stages in %.0f s\n", maxStages, double(total));
    return 0;
}

int cmdBake(int argc, char** argv) {
    const int from = argi(argc, argv, "--from", 1), to = argi(argc, argv, "--to", gen::kLevels);
    const int jobs = std::max(1, std::min(4, argi(argc, argv, "--jobs", 4)));
    const int seeds = argi(argc, argv, "--seeds", 40);
    const char* out = arg(argc, argv, "--out", "src/sim/levels_baked.h");
    const char* report = arg(argc, argv, "--report", "captures/levels/report.md");
    const double t0 = nowSec();
    std::vector<gen::Baked> baked(size_t(to) + 1);
    for (int n = 1; n < from && n <= sim::levelCount(); ++n) {  // levels before `from` stay as the table has them
        baked[size_t(n)].d = sim::level(n);
        baked[size_t(n)].ok = true;
        baked[size_t(n)].botEnd = baked[size_t(n)].d.botEnd;
    }
    std::mutex mu;
    int next = from;
    auto worker = [&]() {
        for (;;) {
            int n;
            {
                std::lock_guard<std::mutex> lock(mu);
                if (next > to) return;
                n = next++;
            }
            gen::Baked b = gen::bakeLevel(n, seeds);
            std::lock_guard<std::mutex> lock(mu);
            baked[size_t(n)] = b;
            std::fprintf(stderr, "\r[bake] level %d   ", n);
        }
    };
    std::vector<std::thread> threads;
    for (int j = 0; j < jobs; ++j) threads.emplace_back(worker);
    for (auto& t : threads) t.join();
    std::fprintf(stderr, "\n");
    int failed = 0;
    for (int n = 1; n <= to; ++n)
        if (!baked[size_t(n)].ok) ++failed;
    FILE* f = std::fopen(out, "wb");
    if (!f) return 2;
    std::fprintf(f, "// Generated by papermarch_headless --bake-levels (TECH_SPEC 4) - do not edit. %d levels.\n", to - failed);
    std::fprintf(f, "static const char* const kBaked[] = {\n");
    static char buf[262144];  // a 15-minute road runs to tens of thousands of characters
    for (int n = 1; n <= to; ++n) {
        if (!baked[size_t(n)].ok) continue;
        if (sim::encodeLevel(baked[size_t(n)].d, buf, sizeof buf) <= 0) {
            std::fclose(f);
            std::fprintf(stderr, "bake: level %d does not encode\n", n);
            return 2;
        }
        std::fprintf(f, "    \"%s\",\n", buf);
    }
    std::fprintf(f, "};\nstatic const int kBakedCount = %d;\n", to - failed);
    std::fclose(f);
    if (FILE* r = std::fopen(report, "wb")) {
        std::fprintf(r, "# The bake, levels %d-%d (%.1f s on %d jobs)\n\n", from, to, nowSec() - t0, jobs);
        std::fprintf(r, "The best run (the planner's) keeps `Kept` of its peak; `Target` is the level's margin (GAME_DESIGN 6); "
                        "HARD marks the hard levels thrown in.\n\n");
        std::fprintf(r, "| Level | Length | Time | Waves | Enemies | Boss | Start | Best peak | Best end | Kept | Target | Look | Dial | Plays | Note |\n");
        std::fprintf(r, "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
        for (int n = from; n <= to; ++n) {
            const gen::Baked& b = baked[size_t(n)];
            int enemies = 0;
            for (int w = 0; w < b.d.waveCount; ++w) enemies += b.d.waves[w].count;
            std::fprintf(r, "| %d | %.0f | %d:%02d | %d | %d | %d | %d | %d | %d | %.3f | %.3f%s | %.0f | %.2f | %d | %s |\n", n,
                         double(b.d.length), int(b.seconds) / 60, int(b.seconds) % 60, b.d.waveCount, enemies, b.d.bossHp, b.d.start, b.botPeak, b.botEnd,
                         double(b.botPeak > 0 ? float(b.botEnd) / float(b.botPeak) : 0.0f), double(b.target), b.d.spike ? " HARD" : "",
                         double(b.d.planLook > 0 ? b.d.planLook : 15.0f), double(b.m), b.plays, b.why);
        }
        std::fclose(r);
    }
    std::printf("bake: %d levels written to %s in %.1f s (%d failed)\n", to - failed, out, nowSec() - t0, failed);
    return failed ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i)
        if (!std::strcmp(argv[i], "--set") && !setTuningPair(argv[i + 1])) {
            std::fprintf(stderr, "--set %s: unknown name or bad value\n", argv[i + 1]);
            return 2;
        }
    if (flag(argc, argv, "--play")) {
        const char* st = arg(argc, argv, "--steer");
        return cmdPlay(argi(argc, argv, "--level", 1), st ? float(std::atof(st)) : -100.0f);
    }
    if (const char* s = arg(argc, argv, "--sim")) {
        if (!std::strcmp(s, "table")) return cmdTable(argi(argc, argv, "--from", 1), argi(argc, argv, "--to", 9999));
        std::fprintf(stderr, "unknown --sim %s\n", s);
        return 2;
    }
    if (flag(argc, argv, "--bake-levels")) return cmdBake(argc, argv);
    if (flag(argc, argv, "--endless")) return cmdEndless(u64(argi(argc, argv, "--seed", 1)), argi(argc, argv, "--stages", 40));
    if (const char* s = arg(argc, argv, "--sweep")) return cmdSweep(std::atoi(s), argi(argc, argv, "--seed", 0));
    if (const char* name = arg(argc, argv, "--render-audio")) {
        const char* outPath = arg(argc, argv, "--out", "out.wav");
        const char* sec = arg(argc, argv, "--seconds");
        return renderAudio(name, outPath, argi(argc, argv, "--world", 1), argi(argc, argv, "--layers", 5), flag(argc, argv, "--boss"),
                           sec ? float(std::atof(sec)) : 0.0f);
    }
    std::fprintf(stderr, "usage: see the comment at the top of src/tools/headless.cpp\n");
    return 2;
}
