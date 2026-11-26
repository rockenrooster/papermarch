#pragma once
// The generator and the bake (GAME_DESIGN 8.3, 8.4; TECH_SPEC 4): recipes, numbers and traps, the plays that grade a
// level, acceptance. Linked into the headless tool and the tests only, never the app.

#include "sim/level.h"

namespace pm::gen {

// Plays that grade a level (GAME_DESIGN 8.4): uniformly random takeable moves, and greedy (the weakest camp first).
struct PlayStats {
    int plays = 0, wins = 0, threeStars = 0;
};
PlayStats randomPlays(const sim::LevelDef& d, int plays, u64 seed);
bool greedyWins(const sim::LevelDef& d, i64* final = nullptr);
int greedyMove(const sim::LevelDef& d, const sim::State& st);

// At least one real decision on the best line: a turn with two or more takeable moves where only some keep the best.
bool hasDecision(const sim::LevelDef& d, sim::Solver& solver);

// A layout fingerprint for the no-repeat rule (GAME_DESIGN 8.4 line 5).
u64 fingerprint(const sim::LevelDef& d);

// The calendar and the recipes (GAME_DESIGN 8.2, 8.3).
enum Recipe : u8 {
    kTree, kLadder, kRing, kHourglass, kIslands, kCrossroads, kTwinValleys, kSpiral,  // Conquest
    kPairs, kTriples, kBraid,                                                        // Gauntlet
    kRingedKeep, kSupplyLine,                                                        // Siege
    kParallel, kPincer,                                                              // Race
    kThreeRoads, kHomestead,                                                         // Last Stand
    kRecipeCount
};
const char* recipeName(int r);
struct Slot {
    u8 kind = sim::kConquest;
    u8 boss = 0;
    u8 world = 1;
    u8 focus = 0;  // an introduction (GAME_DESIGN 5.2): 1 key tower, 2 mire, 3 deserting camp, 4 fading shrine
    u8 hint = 0;   // its hint line
};
Slot calendar(int level);  // the kind of level n and its world

// Elements allowed at a level (GAME_DESIGN 5.2).
struct Allowed {
    bool village = false, shrine = false, post = false, fork = false, grow = false, key = false, mire = false, desert = false,
         fade = false, depot = false;
};
Allowed allowedAt(int level);

// One candidate level from a recipe and a seed: layout, numbers, traps. False if the recipe could not place it.
bool generate(int level, const Slot& slot, int recipe, u64 seed, sim::LevelDef& out);

// Acceptance (GAME_DESIGN 8.4) on a candidate whose best the solver has filled in. `why` gets the first failed line.
struct Grade {
    bool ok = false;
    i64 best = 0;
    int expansions = 0;
    int randomWinPct = 0, randomThreePct = 0;
    bool greedyWin = false;
    bool decision = false;
    char why[64] = "";
};
Grade grade(int level, sim::LevelDef& d, sim::Solver& solver, bool hand);

}  // namespace pm::gen
