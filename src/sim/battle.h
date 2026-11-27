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
enum WeaponKind : u8 { kMusket, kTwin, kScatter, kLance, kRockets, kWeaponCount };  // docs/CONTENT_PASS.md 1
// An add gate below zero is a red barricade; a weapon gate's value is the WeaponKind it hands over.
enum GateKind : u8 { kGateAdd, kGateMul, kGateWeapon };
constexpr i32 kHp = 10;  // hit points are counted in tenths (a twin rifle's ball does 0.6 of a musket ball)

// What a weapon does, times the musket's: its shots a second and damage a ball; how far and fast its balls fly; balls a
// shot (two streams `spread` apart, or a fan of half angle `spread` radians); enemies a ball passes through; a blast
// where it strikes (radius, damage).
struct WeaponStats {
    float rate, damage, range, speed;
    u8 balls;
    float spread;
    u8 pierce;
    float splash, splashDamage;
};
const WeaponStats& weaponStats(u8 kind);

struct GateDef {
    float x0 = -4, x1 = 0, z = 20;
    i32 value = 1;
    u8 kind = kGateAdd;
    u8 grows = 1;     // bullets that hit it raise it by one each, up to `cap` (a multiplier never grows)
    i32 cap = 9;
};
struct BarrelDef {
    float x = 0, z = 30;
    i32 hp = 10;      // musket balls it takes to break
    i32 reward = 10;  // recruits it frees
    u8 weapon = 0;    // > 0: a weapon crate, which hands over that weapon instead of recruits
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
    float drain = 22;    // soldiers a second the boss crushes once it reaches the squad (sized so a fight lasts seconds)
    i32 bossHp = 0;      // 0: no boss, the level ends at `length`
    u8 world = 1, boss = 0, hint = 0, pad = 0;
    u32 seed = 0;
    i32 star2 = 0, star3 = 0;  // soldiers left at the end for two and three stars (the bake sets them from its plan)
    i32 botEnd = 0;            // what the bake's planned run ended with (the report; the tests replay it)
    u8 spike = 0;              // one of the hard levels thrown in among the others (GAME_DESIGN 6)
    u8 gun = kMusket;          // the weapon the squad starts with
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
    i32 hp = 1;         // tenths (kHp)
    i32 incoming = 0;   // damage of the bullets in flight aimed at it (the fire spreads over the nearest enemies)
    u8 kind = kFoot, alive = 0;
    u16 seed = 0;
    float hit = 0;      // seconds since it was last hit (the view flashes it)
    i8 wave = -1;
};
struct Bullet {
    float x = 0, z = 0, zEnd = 0;
    float vx = 0;     // sideways per unit forward: aimed at its target (straight up the road without one)
    i32 dmg = kHp;    // the damage it carries, tenths (the squad's when it was fired)
    i16 target = -1;  // the enemy it was fired at
    u8 kind = kMusket;
    u8 pierce = 0;    // enemies it may still pass through
    u8 grows = 1;     // it raises a gate it strikes (a scattergun's outer pellets do not)
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
    int weapon = -1;        // a weapon taken (from a gate or a crate)
    bool bossHit = false, bossDown = false, won = false, lostAll = false;
    static constexpr int kMaxKills = 24;
    float killX[kMaxKills] = {}, killZ[kMaxKills] = {};
    u8 killKind[kMaxKills] = {};
    int killN = 0;
    // Where a rocket burst (what 0) or a lance bolt went through an enemy (what 1).
    static constexpr int kMaxSpots = 16;
    float spotX[kMaxSpots] = {}, spotZ[kMaxSpots] = {};
    u8 spotWhat[kMaxSpots] = {};
    int spotN = 0;
    void spot(float x, float z, u8 what) {
        if (spotN < kMaxSpots) {
            spotX[spotN] = x;
            spotZ[spotN] = z;
            spotWhat[spotN++] = what;
        }
    }
    void clear() { *this = BattleEvents(); }
};

class Battle {
public:
    void init(const LevelDef& d);
    // One step. `steerX`: where the player (or the bot) wants the squad. `go`: the player has started (until then nothing
    // moves).
    void tick(float steerX, bool go, BattleEvents& ev);
    float radius() const;  // the squad's footprint, units
    // The footprint across the road: x ± radius, squeezed inside the road (a big squad bunches up at an edge).
    void footprint(float& lo, float& hi) const;
    float fireRate() const;  // a musket's shots a second (the weapon multiplies it)
    i32 damage() const;      // a musket ball's damage, tenths (the weapon multiplies it)
    int aliveEnemies() const { return alive_; }
    int bossIndex() const { return boss_; }
    float progress() const;  // 0 at the start, 1 at the boss

    const LevelDef* def = nullptr;
    i32 squad = 0, peak = 0;
    u8 weapon = kMusket;
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
    float bossDrain = 0, bossRate = 0;  // the boss's crushing: its debt in soldiers; its rate, set when it reaches the squad
    i32 waveLost[kMaxWaves] = {};  // soldiers each wave's enemies took when they reached the squad (the generator's measure)

private:
    void spawnWave(int w, BattleEvents& ev);
    void bucket();                         // enemies into cells by distance ahead
    int pickTarget(float bz, float range);  // the nearest enemy ahead still needing damage, -1 none
    void release(Bullet& b);               // a bullet strikes or ends: its damage no longer incoming
    void fire(BattleEvents& ev);
    void moveBullets(BattleEvents& ev);
    void moveEnemies(BattleEvents& ev);
    void kill(int i, BattleEvents& ev);
    void hurt(int i, i32 dmg, BattleEvents& ev);              // an enemy takes damage (and dies at none left)
    void blast(float bx, float bz, i32 dmg, int spare, BattleEvents& ev);  // a rocket's burst: all near but `spare`
    void breakBarrel(int k, BattleEvents& ev);                // shot open: its recruits or its weapon
    u32 rand();
    float fireAcc_ = 0;
    u32 rng_ = 1;
    int alive_ = 0;
    int boss_ = -1;
    // Enemies by distance ahead of the squad, one unit a cell (rebuilt every step).
    static constexpr int kCells = 64;
    i16 cellHead_[kCells];
    i16 next_[kMaxEnemies];
    i16 remap_[kMaxEnemies];  // old index to new when the dead are compacted away (bullets' targets follow)
};

// Levels (src/sim/levels.cpp): the baked table, numbered from 1, and its text form.
int levelCount();
const LevelDef& level(int n);
bool decodeLevel(const char* text, LevelDef& out);
int encodeLevel(const LevelDef& d, char* out, int cap);
const char* hintText(int hint);
constexpr int kHintCount = 32;
constexpr int kHintHard = 11;  // shown on every hard level
int starsFor(const LevelDef& d, i32 squadLeft);

}  // namespace pm::sim
