#include "core/args.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "core/tuning.h"

namespace pm {

namespace {

bool copyStr(char* dst, size_t n, const char* src) {
    if (std::strlen(src) >= n) return false;
    std::snprintf(dst, n, "%s", src);
    return true;
}

bool toInt(const char* s, i64& v) {
    char* end = nullptr;
    v = std::strtoll(s, &end, 10);
    return end != s && *end == 0;
}

bool toFloat(const char* s, float& v) {
    char* end = nullptr;
    v = std::strtof(s, &end);
    return end != s && *end == 0;
}

}  // namespace

bool parseArgs(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        const char* k = argv[i];
        const char* v = i + 1 < argc ? argv[i + 1] : nullptr;
        auto need = [&](void) -> bool {
            if (!v) {
                PM_ERR("args: %s needs a value", k);
                return false;
            }
            ++i;
            return true;
        };
        i64 iv = 0;
        float fv = 0;
        if (!std::strcmp(k, "--scenario")) {
            if (!need() || !copyStr(a.scenario, sizeof a.scenario, v)) return false;
            const char* ok[] = {"play", "bench_idle", "bench_battle", "bench_play", "sim_hash"};
            bool known = false;
            for (const char* s : ok) known = known || !std::strcmp(s, a.scenario);
            if (!known) {
                PM_ERR("args: unknown scenario %s", a.scenario);
                return false;
            }
        } else if (!std::strcmp(k, "--level")) {
            if (!need() || !toInt(v, iv) || iv < 1) return false;
            a.level = int(iv);
        } else if (!std::strcmp(k, "--seed")) {
            if (!need() || !toInt(v, iv)) return false;
            a.seed = iv;
        } else if (!std::strcmp(k, "--world")) {
            if (!need() || !toInt(v, iv) || iv < 1 || iv > 10) return false;
            a.world = int(iv);
        } else if (!std::strcmp(k, "--bot")) {
            if (!need() || !copyStr(a.bot, sizeof a.bot, v)) return false;
            if (std::strcmp(v, "best") && std::strcmp(v, "greedy") && std::strcmp(v, "random")) {
                PM_ERR("args: unknown bot %s", v);
                return false;
            }
        } else if (!std::strcmp(k, "--picture")) {
            if (!need() || !copyStr(a.picture, sizeof a.picture, v)) return false;
            a.fixedStep = true;
        } else if (!std::strcmp(k, "--moves")) {
            if (!need() || !copyStr(a.moves, sizeof a.moves, v)) return false;
        } else if (!std::strcmp(k, "--screenshot")) {
            if (!need() || !copyStr(a.screenshot, sizeof a.screenshot, v)) return false;
        } else if (!std::strcmp(k, "--frames")) {
            if (!need() || !toInt(v, iv) || iv < 1) return false;
            a.frames = int(iv);
        } else if (!std::strcmp(k, "--strip")) {
            if (!need() || !toInt(v, iv) || iv < 1 || iv > 600) return false;  // the flashing audit takes every frame
            a.strip = int(iv);
        } else if (!std::strcmp(k, "--strip-step")) {
            if (!need() || !toInt(v, iv) || iv < 1) return false;
            a.stripStep = int(iv);
        } else if (!std::strcmp(k, "--seconds")) {
            if (!need() || !toFloat(v, fv) || fv <= 0) return false;
            a.seconds = fv;
        } else if (!std::strcmp(k, "--warmup")) {
            if (!need() || !toFloat(v, fv) || fv < 0) return false;
            a.warmup = fv;
        } else if (!std::strcmp(k, "--fixed-step")) {
            a.fixedStep = true;
        } else if (!std::strcmp(k, "--speed")) {
            if (!need() || !toFloat(v, fv) || fv <= 0) return false;
            a.speed = fv;
        } else if (!std::strcmp(k, "--res")) {
            int w = 0, h = 0;
            if (!need() || std::sscanf(v, "%dx%d", &w, &h) != 2 || w < 64 || h < 64 || w > 8192 || h > 8192) {
                PM_ERR("args: --res WxH");
                return false;
            }
            a.resW = w;
            a.resH = h;
            a.resGiven = true;
        } else if (!std::strcmp(k, "--dp-scale")) {
            if (!need() || !toFloat(v, fv) || fv <= 0) return false;
            a.dpScale = fv;
        } else if (!std::strcmp(k, "--safe")) {
            if (!need() || std::sscanf(v, "%f,%f", &a.safeTop, &a.safeBottom) != 2) return false;
        } else if (!std::strcmp(k, "--set")) {
            if (!need() || a.setCount >= 24 || !copyStr(a.sets[a.setCount], 64, v)) return false;
            if (!setTuningPair(v)) {
                PM_ERR("args: --set %s: unknown name or bad value", v);
                return false;
            }
            ++a.setCount;
        } else if (!std::strcmp(k, "--silent")) {
            a.silent = true;
        } else if (!std::strcmp(k, "--no-audio")) {
            a.noAudio = true;
        } else if (!std::strcmp(k, "--offscreen")) {
            a.offscreen = true;
        } else if (!std::strcmp(k, "--no-hud")) {
            a.noHud = true;
        } else if (!std::strcmp(k, "--reduce-effects")) {
            a.reduceEffects = true;
        } else if (!std::strcmp(k, "--rebuild-gl-at")) {
            if (!need() || !toInt(v, iv) || iv < 1) return false;
            a.rebuildGlAt = int(iv);
        } else if (!std::strcmp(k, "--flow-test")) {
            a.flowTest = true;
            a.fixedStep = true;
            a.fresh = true;
        } else if (!std::strcmp(k, "--fps")) {
            if (!need() || !toFloat(v, fv) || fv <= 0) return false;
            a.fps = fv;
        } else if (!std::strcmp(k, "--lint")) {
            a.lint = true;
        } else if (!std::strcmp(k, "--log-sites")) {
            a.logSites = true;
        } else if (!std::strcmp(k, "--fresh")) {
            a.fresh = true;
        } else {
            PM_ERR("args: unknown argument %s", k);
            return false;
        }
    }
    if (a.screenshot[0] && a.frames == 0) a.frames = 120;
    if (a.screenshot[0]) a.fixedStep = true;
    return true;
}

}  // namespace pm
