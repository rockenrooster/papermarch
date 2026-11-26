#pragma once
// The battle (GAME_DESIGN, rewritten 2026-11-26 from the owner's screenshots of the advertisement): a squad at the foot of a
// road fires up it without pause; the player drags it left and right through gates (+1, +20, x2), shoots barrels open for
// recruits, shoots red barricades until they are worth passing, and holds off hordes of red soldiers and a boss.
// Fixed 60 Hz steps; no SDL, no GL, no allocation after init. The app, the bot, the bake and the tests run these rules.

#include "core/types.h"

namespace pm::sim {

constexpr float kRoadHalf = 5.0f;   // the road: x in [-5, 5]; z runs up the road
constexpr float kDt = 1.0f / 60.0f;
constexpr int kMaxEnemies = 1600;
constexpr int kMaxBullets = 512;
constexpr int kMaxGates = 160;
constexpr int kMaxBarrels = 48;
constexpr int kMaxWaves = 24;
constexpr int kMaxRows = 32;        // rows of two or more gates side by side: the level's choices
constexpr i32 kSquadCap = 99999;

enum EnemyKind : u8 { kFoot, kRider, kBrute, kBoss, kEnemyKindCount };
enum GateKind : u8 { kGateAdd, kGateMul };  // an add gate below zero is a red barricade

struct GateDef {
    float x0 = -4, x1 = 0, z = 20;
    i32 value = 1;
    u8 kind = kGateAdd;
    u8 grows = 1;     // bullets that hit it raise it by one each, up to `cap` (a multiplier never grows)
    i32 cap = 9;
};
struct BarrelDef {
    float x = 0, z = 30;
    i32 hp = 10;      // bullets it takes to break
    i32 reward = 10;  // recruits it frees
};
struct WaveDef {
    float z = 40;     // its front row
    float x0 = -4, x1 = 4;
    i32 count = 10;
    u8 kind = kFoot;
};
struct LevelDef {
    i32 start = 3;
    float length = 120;  // the boss (or the finish line) stands here
    float speed = 5;     // the squad's march, units a second
    float pace = 1;      // the horde's speed (march and closing in) times its kind's own; the boss keeps its own
    i32 bossHp = 0;      // 0: no boss, the level ends at `length`
    u8 world = 1, boss = 0, hint = 0, pad = 0;
    u32 seed = 0;
    i32 star2 = 0, star3 = 0;  // soldiers left at the end for two and three stars (the bake sets them from its plan)
    i32 botEnd = 0;            // what the bake's planned run ended with (the report; the tests replay it)
    u8 spike = 0;              // one of the hard levels thrown in among the others (GAME_DESIGN 6)
    // The bake's best run (the proof the level can be won, src/sim/bot.*): how far ahead the bot lines up with a row of
    // gates (0: its everyday 15 units) and, for each row of two or more gates, which one it takes (from the left; -1 its
    // own choice). rowZ lists those rows up the road (from the gates: indexRows).
    float planLook = 0;
    u8 rowCount = 0, planCount = 0;
    float rowZ[kMaxRows] = {};
    i8 plan[kMaxRows] = {};
    int gateCount = 0, barrelCount = 0, waveCount = 0;
    GateDef gates[kMaxGates];
    BarrelDef barrels[kMaxBarrels];
    WaveDef waves[kMaxWaves];
};
// Finds the rows of two or more gates side by side (rowZ, rowCount). The decoder and the generator call it.
void indexRows(LevelDef& d);

struct EnemyStats {
    i16 hp;
    float speed, radius;
    i32 strength;  // soldiers it takes with it when it reaches the squad
};
const EnemyStats& enemyStats(u8 kind);

struct Enemy {
    float x = 0, z = 0;
    i32 hp = 1;
    u8 kind = kFoot, alive = 0;
    u16 seed = 0;
    float hit = 0;      // seconds since it was last hit (the view flashes it)
    i8 wave = -1;
};
struct Bullet {
    float x = 0, z = 0, zEnd = 0;
    u8 alive = 0;
};
struct GateState {
    i32 value = 0;
    u8 used = 0, passed = 0;
    float hit = 0;
};
struct BarrelState {
    i32 hp = 0;
    u8 broken = 0;
    float hit = 0;
};

// What a step did (the view's effects and the sounds).
struct BattleEvents {
    int shots = 0, hits = 0, kills = 0, lost = 0;
    int gate = -1;          // a gate passed this step,
    i32 gateBefore = 0, gateAfter = 0;
    int barrel = -1;        // a barrel broken,
    i32 barrelReward = 0;
    int crashed = -1;       // a barrel run into
    int wave = -1;          // a wave woken
    bool bossHit = false, bossDown = false, won = false, lostAll = false;
    static constexpr int kMaxKills = 24;
    float killX[kMaxKills] = {}, killZ[kMaxKills] = {};
    u8 killKind[kMaxKills] = {};
    int killN = 0;
    void clear() { *this = BattleEvents(); }
};

class Battle {
public:
    void init(const LevelDef& d);
    // One step. `steerX`: where the player (or the bot) wants the squad. `go`: the player has started (until then nothing
    // moves).
    void tick(float steerX, bool go, BattleEvents& ev);
    float radius() const;  // the squad's footprint, units
    float fireRate() const;  // shots a second
    i32 damage() const;      // per bullet
    int aliveEnemies() const { return alive_; }
    int bossIndex() const { return boss_; }
    float progress() const;  // 0 at the start, 1 at the boss

    const LevelDef* def = nullptr;
    i32 squad = 0, peak = 0;
    float x = 0, z = 0, vx = 0;
    float time = 0;
    bool started = false, won = false, lost = false;
    Enemy enemies[kMaxEnemies];
    int enemyCount = 0;
    Bullet bullets[kMaxBullets];
    int bulletCount = 0;
    GateState gates[kMaxGates];
    BarrelState barrels[kMaxBarrels];
    int nextWave = 0;
    float bossDrain = 0;

private:
    void spawnWave(int w, BattleEvents& ev);
    void fire(BattleEvents& ev);
    void moveBullets(BattleEvents& ev);
    void moveEnemies(BattleEvents& ev);
    void kill(int i, BattleEvents& ev);
    u32 rand();
    float fireAcc_ = 0;
    u32 rng_ = 1;
    int alive_ = 0;
    int boss_ = -1;
    // Enemies by distance ahead of the squad, one unit a cell (rebuilt every step).
    static constexpr int kCells = 48;
    i16 cellHead_[kCells];
    i16 next_[kMaxEnemies];
};

// Levels (src/sim/levels.cpp): the baked table, numbered from 1, and its text form.
int levelCount();
const LevelDef& level(int n);
bool decodeLevel(const char* text, LevelDef& out);
int encodeLevel(const LevelDef& d, char* out, int cap);
const char* hintText(int hint);
constexpr int kHintCount = 16;
constexpr int kHintHard = 11;  // shown on every hard level
int starsFor(const LevelDef& d, i32 squadLeft);

}  // namespace pm::sim
