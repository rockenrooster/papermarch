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
// A level's road (the last of the 100 runs about 15 minutes: thousands of units).
constexpr int kMaxGates = 1024;
constexpr int kMaxBarrels = 192;
constexpr int kMaxWaves = 384;
constexpr int kMaxRows = 384;       // rows of two or more gates side by side: the level's choices
constexpr i32 kSquadCap = 99999;

// The archer stands in its ranks and looses an arrow every 1.6 s at the squad from 18 units; the catapult stands at the
// road's side and hurls a boulder every 3.2 s; the giant is a mini-boss that drops a medallion; the shield bearer's
// shield takes bullets from the front; the bomber bursts when it dies (other bombers chain); a drummer hurries the
// enemies around it (docs/CONTENT_PASS.md 3.1).
enum EnemyKind : u8 { kFoot, kRider, kBrute, kBoss, kArcher, kCatapult, kGiant, kShieldman, kBomber, kDrummer, kEnemyKindCount };
// The boss's attacks (docs/CONTENT_PASS.md 3.1), as bits of LevelDef::bossAttacks.
enum BossAttack : u8 { kAttackSlam = 1, kAttackToss = 2, kAttackCry = 4, kAttackStomp = 8 };
// What winning a level means (docs/CONTENT_PASS.md 3.3): gold rush has no enemies and cannot be lost (a barricade never
// takes the last 3); rescue starts with 3 and frees prisoners from cages; arsenal starts with a weapon; siege breaks fort
// walls; the gauntlet runs rollers and mines; rush is a fast, short road; giants' road puts a giant in every horde.
enum LevelMode : u8 { kModeMarch, kModeBoss, kModeGold, kModeRescue, kModeArsenal, kModeSiege, kModeGauntlet, kModeRush, kModeGiants, kModeCount };
// docs/CONTENT_PASS.md 1. The flamethrower burns a cone, the cannon rolls a ball through everything on its line, the
// storm coil's lightning jumps, the mortar lobs shells over everything 18 ahead, the frost blaster slows what it hits.
enum WeaponKind : u8 { kMusket, kTwin, kScatter, kLance, kRockets, kFlame, kCannon, kCoil, kMortar, kFrost, kWeaponCount };
// An add gate below zero is a red barricade; a weapon gate's value is the WeaponKind it hands over.
enum GateKind : u8 { kGateAdd, kGateMul, kGateWeapon };
constexpr i32 kHp = 10;  // hit points are counted in tenths (a twin rifle's ball does 0.6 of a musket ball)
// Power-ups (docs/CONTENT_PASS.md 2): rapid fire (rate x2, 6 s), power shot (damage x2, 6 s), a shield (absorbs the next
// max(10, 30% of the squad) losses, 8 s), freeze (the horde within 45 stops for 3 s, the boss for half that), an airstrike
// (three bombs 12, 20 and 28 ahead, 0.6 s apart, each 12x a musket ball's damage within 3).
// Then: magnet (barrels, crates and cages within 25 open by themselves, 5 s), giants (the squad tramples what touches it,
// 5 s), reinforcements (max(8, 15% of the peak) parachute in), quick march (march x1.6, 5 s), hourglass (the enemy and
// its blows at 40%, 5 s), double gates (the next add gate counts twice, 12 s at most), field gun (a cannon at the squad's
// side fires every second, 8 s).
enum PowerKind : u8 {
    kPowerNone, kRapid, kPowerShot, kShield, kFreeze, kAirstrike, kMagnet, kGiants, kReinforce, kQuick, kHourglass, kDouble,
    kFieldGun, kPowerCount
};
constexpr int kMaxMedals = 64, kMaxBombs = 6;
constexpr int kMaxMines = 192, kMaxCages = 32, kMaxArrows = 96;

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
    // 1 a flip gate: its value and `alt` by turns, 1.5 s each (it never grows); 2 a mystery gate: it shows "?" and two
    // outcomes, its value and `alt` (a decoy); five hits show its true face, and then it grows.
    u8 trick = 0;
    i32 alt = 0;
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
// A boulder waiting at z: once the squad is within 30 it rolls down its lane (x ± 1.2, marked on the road) and takes
// whoever of the squad stands in it (at most 40%). It cannot be shot: step out of the lane.
struct RollerDef {
    float x = 0, z = 40;
};
// A fort wall across the road: the march stops at it and the horde waits behind it until it is shot down (hit points in
// musket balls; rockets, the cannon and the mortar do three times).
struct WallDef {
    float z = 40;
    i32 hp = 60;
};
// A stretch of road with weather: wind pushes the squad sideways at `push` units a second (+ to the right); on ice the
// squad steers a third as quickly.
enum StripKind : u8 { kStripWind = 1, kStripIce = 2 };
struct StripDef {
    float z0 = 40, z1 = 60;
    u8 kind = kStripWind;
    float push = 0;
};
// The level's sky (the view only): fog hides what is further than about 22 units, night darkens all but the near road.
enum SkyKind : u8 { kSkyClear, kSkyFog, kSkyNight };
constexpr int kMaxRollers = 96, kMaxWalls = 32, kMaxStrips = 48;
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
    u8 sky = kSkyClear;        // SkyKind
    // The bake's best run (the proof the level can be won, src/sim/bot.*): how far ahead the bot lines up with a row of
    // gates (0: its everyday 15 units) and, for each row of two or more gates, which one it takes (from the left; -1 its
    // own choice). rowZ lists those rows up the road (from the gates: indexRows).
    float planLook = 0;
    u16 rowCount = 0, planCount = 0;
    float rowZ[kMaxRows] = {};
    i8 plan[kMaxRows] = {};
    int gateCount = 0, barrelCount = 0, waveCount = 0, medalCount = 0, mineCount = 0, cageCount = 0, rollerCount = 0, wallCount = 0,
        stripCount = 0;
    GateDef gates[kMaxGates];
    BarrelDef barrels[kMaxBarrels];
    WaveDef waves[kMaxWaves];
    MedalDef medals[kMaxMedals];
    MineDef mines[kMaxMines];
    CageDef cages[kMaxCages];
    RollerDef rollers[kMaxRollers];
    WallDef walls[kMaxWalls];
    StripDef strips[kMaxStrips];
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
    float cool = 0;     // an archer: seconds to its next arrow (a catapult: its next boulder)
    i32 shield = 0;     // a shield bearer's shield, tenths: bullets from the front wear it down first
    float slow = 0;     // seconds left slowed by the frost blaster
    i16 wave = -1;
};
struct Shell {          // a mortar's shell on its way: it lands at x, z when t reaches 0
    float x0 = 0, z0 = 0, x = 0, z = 0, t = 0;
    i32 dmg = 0;
};
struct Arrow {          // an archer's arrow on its way: it lands on the squad when t reaches 0 and takes a soldier
    float x0 = 0, z0 = 0, t = 0;
    i16 wave = -1;
};
// A blow shown on the road before it lands (dodge it sideways): a boulder's red circle (kind 0, radius r, at x, z), the
// club's half of the road (kind 1, x < 0 the left half), a roller down the lane x ± r (kind 3, from z, like the ridge),
// a stomp's ridge with a gap 3 wide at x (kind 2, it reaches the
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
    u8 seen = 0;  // a mystery gate: hits taken (it shows its face at 5)
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
    int magnet = -1;        // a barrel (or crate) the magnet opened; a cage it opened is 100 + its index
    i32 reinforced = 0;     // soldiers who parachuted in this step
    bool doubled = false;   // DOUBLE GATES spent on a gate
    int wallDown = -1;      // a fort wall that fell this step
    // The storm coil's bolts this step, from point to point.
    static constexpr int kMaxZaps = 16;
    float zapX0[kMaxZaps] = {}, zapZ0[kMaxZaps] = {}, zapX1[kMaxZaps] = {}, zapZ1[kMaxZaps] = {};
    int zapN = 0;
    void zap(float x0, float z0, float x1, float z1) {
        if (zapN < kMaxZaps) {
            zapX0[zapN] = x0;
            zapZ0[zapN] = z0;
            zapX1[zapN] = x1;
            zapZ1[zapN++] = z1;
        }
    }
    int thrown = 0;         // boulders thrown this step (catapults, the boss)
    i32 absorbed = 0;       // soldiers the shield saved this step
    bool shieldGone = false;
    bool bossHit = false, bossDown = false, won = false, lostAll = false;
    // A blow that landed on the squad this step (the boss's club, a giant's): where, how many it took, whose.
    bool blow = false, blowBoss = false;
    float blowX = 0, blowZ = 0;
    i32 blowLost = 0;
    static constexpr int kMaxKills = 24;
    float killX[kMaxKills] = {}, killZ[kMaxKills] = {};
    u8 killKind[kMaxKills] = {};
    int killN = 0;
    // Where a rocket burst (what 0), a lance bolt went through an enemy (what 1), an airstrike's bomb landed (what 2), a
    // mine went off (what 3), a bomber burst (what 4), a mortar's shell landed (what 5), a cannonball struck (what 6).
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
    bool halted = false;  // stopped (before the boss, a giant or a wall): the view stops the soldiers' march
    // Won once every enemy is beaten, not only at the finish line (init leaves it as it is: the generator turns it off for
    // its run of a road it is still laying, and the bake for its run to just before the end fight).
    bool clearWins = true;
    Enemy enemies[kMaxEnemies];
    int enemyCount = 0;
    Bullet bullets[kMaxBullets];
    int bulletCount = 0;
    GateState gates[kMaxGates];
    BarrelState barrels[kMaxBarrels];
    int nextWave = 0;
    float bossDrain = 0, bossRate = 0;  // the boss's crushing: its debt in soldiers; its rate, set when it reaches the squad
    // The boss strikes with its club every kBossBlow seconds while it stands on the squad (the view swings it in time):
    // the clock to its next blow, and whether it is on the squad now.
    static constexpr float kBossBlow = 0.9f;
    float bossSwing = 0;
    bool bossContact = false;
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
    float enemySpeed(const Enemy& e) const;  // units a second down the road now (the view moves it between steps)
    i32 gateNow(int g, float t) const;       // what gate g gives `t` seconds into the march (a flip gate's face then)
    const StripDef* stripAt(float zz) const; // the weather over the road at zz, null none
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
    // The mortar's shells in the air; the clocks of the flamethrower's ticks, the field gun, the magnet; soldiers still to
    // parachute in (REINFORCEMENTS) and how many a second.
    // Rollers that have started (or gone); each fort wall's hit points left (tenths) and seconds since it was hit.
    u8 rollerGone[kMaxRollers] = {};
    i32 wallHp[kMaxWalls] = {};
    float wallHit[kMaxWalls] = {};
    int wallAhead() const;  // the nearest standing wall ahead of the squad, -1 none
    static constexpr int kMaxShells = 16;
    Shell shells[kMaxShells] = {};
    int shellCount = 0;
    float flameAcc = 0, fieldGunAcc = 0, magnetT = 0, reinforceLeft = 0, reinforceRate = 0, reinforceAcc = 0;
    // Where gate g stands across the road `t` seconds into the march (a moving gate slides; the others stand still).
    void gateSpan(int g, float t, float& lo, float& hi) const;

private:
    void spawnWave(int w, BattleEvents& ev);
    void bucket();                         // enemies into cells by distance ahead
    // The nearest enemy ahead still needing damage (its shield counting for a bullet from the front), a bomber or drummer
    // a little further on first; -1 none.
    int pickTarget(float bz, float range, bool front);
    void release(Bullet& b);               // a bullet strikes or ends: its damage no longer incoming
    void fire(BattleEvents& ev);
    void moveBullets(BattleEvents& ev);
    void moveEnemies(BattleEvents& ev);
    void kill(int i, BattleEvents& ev);
    // An enemy takes damage (and dies at none left); `front`: a bullet from the front, which a shield takes first.
    void hurt(int i, i32 dmg, BattleEvents& ev, bool front = false);
    // What a blow of dmg (the squad's, which grows past 50 soldiers) does to e: the boss and giants take it all; to the
    // horde it is a musket ball's worth whatever the squad's size, so a few soldiers lost do not slow its killing and a
    // horde's bite grows with its size instead of all at once (the squad stands for hordes: the owner, 2026-11-27).
    i32 against(const Enemy& e, i32 dmg) const;
    // Soldiers a blow of n takes from the squad: n, or n for every 50 in a bigger squad (as its balls grow past 50), so a
    // horde takes the same share of any squad.
    i32 toll(i32 n) const;
    void lightning(float x0, float z0, i32 dmg, BattleEvents& ev);  // the storm coil's bolt and its four jumps
    void scorch(i32 dmg, BattleEvents& ev);                   // the flamethrower's cone, one tick
    float drive(const Enemy& e) const;                        // its speed factor now (frost, drums, the hourglass)
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
    float drumX_[16] = {}, drumZ_[16] = {};  // the drummers this step (their beat hurries whoever is within 5)
    int drumN_ = 0;
    // Gates and barrels whose state has been set up: the generator lengthens the road while a battle plays it (sync).
    int gatesInit_ = 0, barrelsInit_ = 0, rollersInit_ = 0, wallsInit_ = 0;
    void hitWall(int w, i32 dmg, BattleEvents& ev);  // a fort wall takes dmg (and falls at none left)
    void sync();
};

// Levels (src/sim/levels.cpp): the baked table, numbered from 1, and its text form.
int levelCount();
const LevelDef& level(int n);
bool decodeLevel(const char* text, LevelDef& out);
int encodeLevel(const LevelDef& d, char* out, int cap);
const char* hintText(int hint);
constexpr int kHintCount = 58;
constexpr int kHintHard = 11;  // shown on every hard level
int starsFor(const LevelDef& d, i32 squadLeft);

}  // namespace pm::sim
