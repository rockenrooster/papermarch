#pragma once
// The generator and the bake (TECH_SPEC 4): a level is a road of sections (columns of +1 gates, gate pairs, barrels,
// red barricades, hordes) and a boss; the bake plays each candidate with the bot and scales the hordes and the boss until
// the bot wins by the margin the level's place in the game asks for. Linked into the headless tool and the tests only.

#include "sim/battle.h"

namespace pm::gen {

// Level n from a seed; `m` scales the hordes and the boss (the bake's dial).
void generate(int n, u64 seed, float m, sim::LevelDef& out);

struct Baked {
    sim::LevelDef d;
    bool ok = false;
    i32 botEnd = 0, botPeak = 0;
    float m = 1;
    int plays = 0;
    char why[80] = "";
};
// Tries seeds and dials until the bot wins inside the level's band; sets the stars from the bot's result.
Baked bakeLevel(int n, int maxSeeds);

}  // namespace pm::gen
