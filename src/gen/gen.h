#pragma once
// The generator and the bake (TECH_SPEC 4): a level is a road of sections (columns of +1 gates, gate pairs, barrels,
// red barricades, hordes) and a boss; the bake plays each candidate with the bot and scales the hordes and the boss until
// the bot wins by the margin the level's place in the game asks for. Linked into the headless tool and the tests only.

#include "sim/battle.h"

namespace pm::gen {

// A level built up to its end fight: the road's sections, each sized against the squad the bot really has there (its
// hordes made to cost it a share of it that grows with n), and what the end fight is sized from.
struct Shape {
    sim::LevelDef d;
    bool boss = false;           // the end: the boss, or a last stand of the horde
    float endSquad = 0;          // the bot's squad at the end of the road (0: it never got there)
    float lastZ = 0;
    bool lastRiders = false, lastBrutes = false;
    u8 standPowers[2] = {};      // a last stand: the supply crates before the squad
    i32 hordeMax = 900;          // foot in a horde at most (20 on the first three levels: whoever steers anywhere wins)
};
void build(int n, u64 seed, Shape& out);
// The end fight at strength m: the boss's hit points (the end squad's fire for m seconds) or the last stand (m times the
// end squad in foot, with riders and brutes). Through the text form, so the bot plays what the table holds.
void finish(const Shape& s, float m, sim::LevelDef& out);
// build + finish (the sweep).
void generate(int n, u64 seed, float m, sim::LevelDef& out);

struct Baked {
    sim::LevelDef d;
    bool ok = false;
    i32 botEnd = 0, botPeak = 0;
    float m = 1, target = 0;
    float seconds = 0;  // the best run's length
    int plays = 0;
    char why[80] = "";
};
// The difficulty (GAME_DESIGN 6): the share of its peak squad the best run keeps at the end of level n, and whether n is
// one of the hard levels thrown in among the others.
// The game is 100 levels (the owner, 2026-11-27), each longer than the one before: about half a minute at level 1 and
// fifteen at level 100 (levelSeconds: 30 s x 30^((n - 1) / 99)).
constexpr int kLevels = 100;
float levelSeconds(int n);
float marginTarget(int n);
bool isSpike(int n);
// The level's theme (src/gen/gen.cpp: what its road leans on, its pace; never the same two levels running).
int themeOf(int n);
// The weapon level n offers (a sim::WeaponKind, 0 none): its introduction on the level that brings it in, then one of
// those known on most levels, never the one the level before offered (docs/CONTENT_PASS.md 4.1 rule 4).
int weaponOffer(int n);
// Level n's mode (a sim::LevelMode; until the rotation of docs/CONTENT_PASS.md 4.1): in each block of ten the 10th is the
// boss's, the 5th a gold rush from 15, the 2nd an arsenal from 22, the 4th a rescue from 24; a hard level is a march.
u8 levelMode(int n);
// Tries seeds and dials until the planner's best run wins inside the level's band; sets the plan and the stars from it.
Baked bakeLevel(int n, int maxSeeds);

}  // namespace pm::gen
