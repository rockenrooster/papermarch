#pragma once
// The bot (TECH_SPEC 4): where a sensible player steers. It reads what is ahead: the gate worth passing, the barrel worth
// shooting, the horde to aim at. The bake plays every level with it; bench_play and the flow test use it too.

#include "sim/battle.h"

namespace pm::sim {

float botSteer(const Battle& b);

// A whole level played by the bot: true if it won; `left` the squad at the end.
bool botPlay(const LevelDef& d, i32* left = nullptr, i32* peak = nullptr, int maxSeconds = 240);

}  // namespace pm::sim
