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
constexpr int kMaxWaves = 32;
constexpr int kMaxRows = 32;        // rows of two or more gates side by side: the level's choices
constexpr i32 kSquadCap = 99999;

// The archer stands in its ranks and looses an arrow every 1.6 s at the squad from 18 units; the catapult stands at the
// road's side and hurls a boulder every 3.2 s; the giant is a mini-boss that drops a medallion (docs/CONTENT_PASS.md 3.1).
enum EnemyKind : u8 { kFoot, kRider, kBrute, kBoss, kArcher, kCatapult, kGiant, kEnemyKindCount };
// The boss's attacks (docs/CONTENT_PASS.md 3.1), as bits of LevelDef::bossAttacks.
enum BossAttack : u8 { kAttackSlam = 1, kAttackToss = 2, kAttackCry = 4, kAttackStomp = 8 };
// What winning a level means (docs/CONTENT_PASS.md 3.3): gold rush has no enemies and cannot be lost (a barricade never
// takes the last 3); rescue starts with 3 and frees prisoners from cages; arsenal starts with a weapon.
enum LevelMode : u8 { kModeMarch, kModeBoss, kModeGold, kModeRescue, kModeArsenal, kModeCount };
enum WeaponKind : u8 { kMusket, kTwin, kScatter, kLance, kRockets, kWeaponCount };  // docs/CONTENT_PASS.md 1
// An add gate below zero is a red barricade; a weapon gate's value is the WeaponKind it hands over.
enum GateKind : u8 { kGateAdd, kGateMul, kGateWeapon };
constexpr i32 kHp = 10;  // hit points are counted in tenths (a twin rifle's ball does 0.6 of a musket ball)
// Power-ups (docs/CONTENT_PASS.md 2): rapid fire (rate x2, 6 s), power shot (damage x2, 6 s), a shield (absorbs the next
// max(10, 30% of the squad) losses, 8 s), freeze (the horde within 45 stops for 3 s, the boss for half that), an airstrike
// (three bombs 12, 20 and 28 ahead, 0.6 s apart, each 12x a musket ball's damage within 3).
enum PowerKind : u8 { kPowerNone, kRapid, kPowerShot, kShield, kFreeze, kAirstrike, kPowerCount };
constexpr int kMaxMedals = 16, kMaxBombs = 6;
constexpr int kMaxMines = 32, kMaxCages = 8, kMaxArrows = 96;

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
    u8 moves = 0;     // > 0: it slides across the road, 3 either way every 3 s, starting `moves`/256 of the way round
};
struct BarrelDef {
    float x = 0, z = 30;
    i32 hp = 10;      // musket balls it takes to break
    i32 reward = 10;  // recruits it frees
    u8 weapon = 0;    // > 0: a weapon crate, which hands over that weapon instead of recruits
    u8 power = 0;     // > 0: a supply crate, which gives that power-up (and costs nothing run into)
};
struct MedalDef {     // a gold medallion over the road: the squad's footprint touching it takes its power-up
    float x = 0, z = 40;
    u8 power = kRapid;
};
struct MineDef {      // touched: 6 soldiers; shot (3 musket balls): a blast of 10 balls' damage to enemies within 2.5
    float x = 0, z = 40;
};
struct CageDef {      // prisoners behind bars (bullets pass through): the footprint touching it frees them
    float x = 0, z = 40;
    i32 reward = 20;
};
struct WaveDef {
    float z = 40;     // its front row
    float x0 = -4, x1 = 4;
    i32 count = 10;
    u8 kind = kFoot;
    i32 hp = 0;       // > 0: each one's hit points (musket balls) instead of its kind's (a giant's are the level's own)
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
    u8 mode = kModeMarch;      // LevelMode
    u8 bossAttacks = 0;        // BossAttack bits
    // The bake's best run (the proof the level can be won, src/sim/bot.*): how far ahead the bot lines up with a row of
    // gates (0: its everyday 15 units) and, for each row of two or more gates, which one it takes (from the left; -1 its
    // own choice). rowZ lists those rows up the road (from the gates: indexRows).
    float planLook = 0;
    u8 rowCount = 0, planCount = 0;
    float rowZ[kMaxRows] = {};
    i8 plan[kMaxRows] = {};
    int gateCount = 0, barrelCount = 0, waveCount = 0, medalCount = 0, mineCount = 0, cageCount = 0;
    GateDef gates[kMaxGates];
    BarrelDef barrels[kMaxBarrels];
    WaveDef waves[kMaxWaves];
    MedalDef medals[kMaxMedals];
    MineDef mines[kMaxMines];
    CageDef cages[kMaxCages];
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
    float cool = 0;     // an archer: seconds to its next arrow
    i8 wave = -1;
};
struct Arrow {          // an archer's arrow on its way: it lands on the squad when t reaches 0 and takes a soldier
    float x0 = 0, z0 = 0, t = 0;
    i8 wave = -1;
};
// A blow shown on the road before it lands (dodge it sideways): a boulder's red circle (kind 0, radius r, at x, z), the
// club's half of the road (kind 1, x < 0 the left half), a stomp's ridge with a gap 3 wide at x (kind 2, it reaches the
// squad when t is 0). When t reaches 0 it takes the soldiers in it, at most `cap`.
struct Strike {
    u8 kind = 0;
    float x = 0, z = 0, r = 1.7f, t = 1.0f, full = 1.0f;
    float fromX = 0, fromZ = 0;  // where a boulder was thrown from (the view's arc)
    i32 cap = 14;
    u8 byBoss = 0;               // the boss threw it (else a catapult)
};
constexpr int kMaxStrikes = 12, kMaxDrops = 8;
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
    int power = -1;         // a power-up taken (from a medallion or a supply crate),
    int medal = -1;         // the medallion it came from
    int cage = -1;          // a cage opened (its prisoners join)
    int arrows = 0;         // arrows loosed this step
    int mine = -1;          // a mine gone off (shot, or stepped on: `mineStepped`)
    bool mineStepped = false;
    int strikeShown = -1;   // a blow shown on the road (its kind) and one landed (its kind, and where)
    int strikeLanded = -1;
    float strikeX = 0, strikeZ = 0;
    bool roar = false;      // the boss's war cry; the boss throwing a boulder
    bool bossThrew = false;
    int thrown = 0;         // boulders thrown this step (catapults, the boss)
    i32 absorbed = 0;       // soldiers the shield saved this step
    bool shieldGone = false;
    bool bossHit = false, bossDown = false, won = false, lostAll = false;
    static constexpr int kMaxKills = 24;
    float killX[kMaxKills] = {}, killZ[kMaxKills] = {};
    u8 killKind[kMaxKills] = {};
    int killN = 0;
    // Where a rocket burst (what 0), a lance bolt went through an enemy (what 1), an airstrike's bomb landed (what 2), a
    // mine went off (what 3).
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
    // Power-ups running: seconds left and their full time (the HUD's rings); what the shield will still absorb; the
    // medallions taken; an airstrike's bombs on their way down (they land at x, z in t seconds).
    float powerLeft[kPowerCount] = {}, powerFull[kPowerCount] = {};
    i32 shieldPool = 0;
    u8 medalTaken[kMaxMedals] = {};
    struct Bomb {
        float x, z, t;
    };
    Bomb bombs[kMaxBombs] = {};
    int bombCount = 0;
    bool powered(u8 p) const { return p < kPowerCount && powerLeft[p] > 0.0f; }
    // Mines (hit points left, gone), cages opened, the archers' arrows in flight.
    i32 mineHp[kMaxMines] = {};
    u8 mineGone[kMaxMines] = {}, cageOpen[kMaxCages] = {};
    Arrow arrows[kMaxArrows] = {};
    int arrowCount = 0;
    // Blows shown on the road; the boss's attack clocks and the war cries it has made; medallions dropped by giants.
    Strike strikes[kMaxStrikes] = {};
    int strikeCount = 0;
    float slamT = 0, tossT = 0, stompT = 0;
    u8 criesDone = 0;
    MedalDef drops[kMaxDrops] = {};
    u8 dropTaken[kMaxDrops] = {};
    int dropCount = 0;
    // Where gate g stands across the road `t` seconds into the march (a moving gate slides; the others stand still).
    void gateSpan(int g, float t, float& lo, float& hi) const;

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
    // A burst at (bx, bz): every enemy within `reach` but `spare` takes dmg; `what` tells the view (BattleEvents::spot).
    void blast(float bx, float bz, float reach, i32 dmg, int spare, u8 what, BattleEvents& ev);
    void breakBarrel(int k, BattleEvents& ev);                // shot open: its recruits, its weapon or its power-up
    void take(u8 power, BattleEvents& ev);                    // a power-up taken
    void lose(i32 n, BattleEvents& ev, int wave = -1);        // soldiers lost (the shield takes them first)
    void powers(BattleEvents& ev);                            // the power-ups' clocks and the bombs falling
    void explode(int m, BattleEvents& ev);                    // a mine goes off
    void volleys(BattleEvents& ev);                           // archers loose arrows; arrows land
    void attacks(BattleEvents& ev);                           // catapults and the boss show their blows; blows land
    void strike(const Strike& s, BattleEvents& ev);           // a blow shown
    float share(float a, float b) const;                      // the squad's share standing across [a, b]
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
constexpr int kHintCount = 48;
constexpr int kHintHard = 11;  // shown on every hard level
int starsFor(const LevelDef& d, i32 squadLeft);

}  // namespace pm::sim
