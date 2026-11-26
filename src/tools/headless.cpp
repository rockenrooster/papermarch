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

#include "core/tuning.h"
#include "gen/gen.h"
#include "sim/battle.h"
#include "sim/bot.h"
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

int cmdPlay(int n) {
    const sim::LevelDef& d = sim::level(n);
    static sim::Battle b;
    b.init(d);
    sim::BattleEvents ev;
    int lastSecond = -1, kills = 0, lost = 0, shots = 0;
    while (!b.won && !b.lost && b.time < 300) {
        b.tick(sim::botSteer(b), true, ev);
        kills += ev.kills;
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
        std::printf("%5d %7.0f %5d %7d %5d %6d %9d %8d %7d %7d  %s\n", n, double(d.length), d.waveCount, enemies, d.bossHp, d.start, peak,
                    left, d.star2, d.star3, ok ? "ok" : (won ? "end differs" : "LOST"));
    }
    return failures ? 1 : 0;
}

int cmdBake(int argc, char** argv) {
    const int from = argi(argc, argv, "--from", 1), to = argi(argc, argv, "--to", 300);
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
    static char buf[16384];
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
        std::fprintf(r, "| Level | Length | Waves | Enemies | Boss | Start | Bot peak | Bot end | Dial | Plays | Note |\n");
        std::fprintf(r, "|---|---|---|---|---|---|---|---|---|---|---|\n");
        for (int n = from; n <= to; ++n) {
            const gen::Baked& b = baked[size_t(n)];
            int enemies = 0;
            for (int w = 0; w < b.d.waveCount; ++w) enemies += b.d.waves[w].count;
            std::fprintf(r, "| %d | %.0f | %d | %d | %d | %d | %d | %d | %.2f | %d | %s |\n", n, double(b.d.length), b.d.waveCount, enemies,
                         b.d.bossHp, b.d.start, b.botPeak, b.botEnd, double(b.m), b.plays, b.why);
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
    if (flag(argc, argv, "--play")) return cmdPlay(argi(argc, argv, "--level", 1));
    if (const char* s = arg(argc, argv, "--sim")) {
        if (!std::strcmp(s, "table")) return cmdTable(argi(argc, argv, "--from", 1), argi(argc, argv, "--to", 9999));
        std::fprintf(stderr, "unknown --sim %s\n", s);
        return 2;
    }
    if (flag(argc, argv, "--bake-levels")) return cmdBake(argc, argv);
    if (const char* name = arg(argc, argv, "--render-audio")) {
        const char* outPath = arg(argc, argv, "--out", "out.wav");
        const char* sec = arg(argc, argv, "--seconds");
        return renderAudio(name, outPath, argi(argc, argv, "--world", 1), argi(argc, argv, "--layers", 5), flag(argc, argv, "--boss"),
                           sec ? float(std::atof(sec)) : 0.0f);
    }
    std::fprintf(stderr, "usage: see the comment at the top of src/tools/headless.cpp\n");
    return 2;
}
