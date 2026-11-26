#pragma once
// Command-line arguments (TECH_SPEC 2.5). Desktop argv; on Android the `args` intent extra (debuggable builds only).

#include "core/types.h"

namespace pm {

struct Args {
    char scenario[24] = "play";    // play | bench_idle | bench_battle | bench_play | sim_hash
    int level = 0;                 // 0: the saved level
    i64 seed = -1;                 // -1: the level's own
    int world = 0;                 // 0: the level's own (1..10)
    char bot[12] = "";             // best | greedy | random
    char picture[40] = "";
    char moves[160] = "";          // --moves 1,2,5: sites taken in order before a picture or a strip (TECH_SPEC 9.1)
    char screenshot[260] = "";
    int frames = 0;
    int strip = 0;
    int stripStep = 1;
    float seconds = 10.0f;
    float warmup = 2.0f;
    bool fixedStep = false;
    float speed = 1.0f;
    int resW = 540, resH = 1200;
    bool resGiven = false;
    float dpScale = 0.0f;          // 0: from the matrix (TECH_SPEC 5.2) or the display
    float safeTop = 32.0f, safeBottom = 16.0f;  // dp, desktop only
    bool silent = false;
    bool noAudio = false;
    bool offscreen = false;
    bool noHud = false;
    bool reduceEffects = false;
    int rebuildGlAt = -1;
    bool flowTest = false;
    float fps = 0.0f;
    bool lint = false;             // layout lint on this run's screens (exit 7 on failure)
    bool logSites = false;         // log where the takeable sites and NEXT are (the browser check taps them)
    bool fresh = false;            // ignore the save file (pictures and tests)
    int setCount = 0;
    char sets[24][64] = {};
};

// Returns false (and logs why) on a bad argument.
bool parseArgs(int argc, char** argv, Args& out);

}  // namespace pm
