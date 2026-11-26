#pragma once
// The hand-made levels (GAME_DESIGN 8.1, 5.2): introductions and set pieces, written as data. The bake checks them and
// writes them into the table with their best score; the app reads only the table.

#include "sim/level.h"

namespace pm::sim {

// Level n if it is hand-made (best left at 0), else false.
bool handLevel(int n, LevelDef& out);

// Hint lines (GAME_DESIGN 8.1); index 0 is "no hint". The game shows a level's line once.
constexpr int kHintCount = 24;
const char* hintText(int hint);

// A small builder for writing levels by hand and in the generator.
struct LevelBuilder {
    LevelDef d;
    int site(u8 kind, int x, int y, int value) {
        Site& s = d.sites[d.siteCount];
        s = Site();
        s.kind = kind;
        s.x = i16(x);
        s.y = i16(y);
        s.value = value;
        return d.siteCount++;
    }
    void road(int a, int b, int lock = kNoLock) {
        Road& r = d.roads[d.roadCount++];
        r.a = u8(a);
        r.b = u8(b);
        r.lock = u8(lock);
    }
    void grow(int s, int perTurn, int lo, int hi) {
        d.sites[s].perTurn = i16(perTurn);
        d.sites[s].lo = lo;
        d.sites[s].hi = hi;
    }
    void fork(int s, int group) { d.sites[s].fork = u8(group); }
    void raid(int turn, int entry, int strength) {
        Raid& r = d.raids[d.raidCount++];
        r.turn = u8(turn);
        r.entry = u8(entry);
        r.strength = strength;
    }
};

}  // namespace pm::sim
