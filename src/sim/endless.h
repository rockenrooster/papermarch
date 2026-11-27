#pragma once
// Endless (src/sim/endless.cpp): a stage laid as it is played, a section at a time ahead of the squad.

#include "core/rng.h"
#include "sim/battle.h"

namespace pm::sim {

class Endless {
public:
    // Starts stage `stage` (from 1) in d: its world, pace and length; nothing laid yet.
    void begin(int stage, u64 seed, LevelDef& d);
    // Lays what lies within 110 units of the squad (sized against the squad it has now), and the end once the road is
    // laid; call it every step (cheap when there is nothing to lay). Sets b.clearWins once the end is laid.
    void extend(LevelDef& d, Battle& b);
    bool done() const { return done_; }

private:
    Rng r_;
    int k_ = 1;
    float z_ = 0, roadEnd_ = 0;
    int since_ = 0, last_ = -1, section_ = 0, sinceWeapon_ = 0, sincePower_ = 0;
    bool done_ = false;
};

}  // namespace pm::sim
