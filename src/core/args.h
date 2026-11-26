#pragma once
// Command-line arguments (TECH_SPEC 2.5). Desktop argv; on Android the `args` intent extra (debuggable builds only).

#include "core/types.h"

namespace gt {

struct Args {
    char scenario[24] = "play";    // play | touch_test | bench_idle | bench_flood | bench_play
    int level = 0;                 // 0: the saved level
    i64 seed = -1;                 // -1: the level's own
    int recipe = -1;               // the recipe_top picture: a recipe from the catalogue (-1: the calendar's)
    int world = 0;                 // 0: the level's own (1..10)
    int paletteAlt = 0;            // the review's palette alternatives for worlds 6-10 (1 or 2; 0: the world's own)
    char bot[12] = "";             // greedy | naive | sweep | idle
    char picture[40] = "";
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
    bool tone = false;
    bool alternate = false;        // bench_play: alternate --level N and N + 1 (a world change when N ends a world)
    bool lint = false;             // layout lint on this run's screens (exit 7 on failure)
    bool fresh = false;            // ignore the save file (pictures and tests)
    int setCount = 0;
    char sets[24][64] = {};
};

// Returns false (and logs why) on a bad argument.
bool parseArgs(int argc, char** argv, Args& out);

}  // namespace gt
