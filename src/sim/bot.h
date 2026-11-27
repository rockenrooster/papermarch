#pragma once
// The bot (TECH_SPEC 4): where a sensible player steers. It reads what is ahead: the blow to step out of, the gate worth
// passing, the barrel worth shooting, the horde to aim at. A level's plan (LevelDef::planLook, plan) can line it up with
// a row of gates earlier and pick a row's gate for it: the planner searches those for the best run, the proof that the
// level can be won. The bake plays every level with it; bench_play, pictures and the flow test use it too.

#include "sim/battle.h"

namespace pm::sim {

float botSteer(const Battle& b);

struct PlayResult {
    bool won = false;
    i32 left = 0, peak = 0;
    float progress = 0;
    float seconds = 0;  // how long the run took
};
// A whole level played by the bot with the level's plan (a level runs up to about 15 minutes: the default cap is an hour).
PlayResult playLevel(const LevelDef& d, int maxSeconds = 3600);
bool botPlay(const LevelDef& d, i32* left = nullptr, i32* peak = nullptr, int maxSeconds = 3600);
// Plays b on with its level's plan until it is `zTo` up the road (or the level ends): false if the squad falls first.
bool playTo(Battle& b, float zTo);

// The planner: how far ahead to line up with the gates (three full runs) and which gate of each row to take, row after
// row up the road: from the planned run 64 units before the row (nothing there depends on the row yet), each gate is
// played to 70 units past it and the one that leaves the most soldiers kept. Writes the plan into `d` and returns the
// planned run.
PlayResult planLevel(LevelDef& d);

}  // namespace pm::sim
