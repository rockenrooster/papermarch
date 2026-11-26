#pragma once
// The bot (TECH_SPEC 4): where a sensible player steers. It reads what is ahead: the gate worth passing, the barrel worth
// shooting, the horde to aim at. A level's plan (LevelDef::planLook, plan) can line it up with a row of gates earlier
// and pick a row's gate for it: the planner searches those for the best run, the proof that the level can be won. The
// bake plays every level with it; bench_play, pictures and the flow test use it too.

#include "sim/battle.h"

namespace pm::sim {

float botSteer(const Battle& b);

struct PlayResult {
    bool won = false;
    i32 left = 0, peak = 0;
    float progress = 0;
};
// A whole level played by the bot with the level's plan.
PlayResult playLevel(const LevelDef& d, int maxSeconds = 240);
bool botPlay(const LevelDef& d, i32* left = nullptr, i32* peak = nullptr, int maxSeconds = 240);

// The planner: how far ahead to line up with the gates and which gate of each row to take, chosen one after another
// by playing the level out each way (a perfect player's run, as near as this search finds it). Writes the best plan
// into `d` and returns its run.
PlayResult planLevel(LevelDef& d);

}  // namespace pm::sim
