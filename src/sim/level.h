#pragma once
// The rules of PAPERMARCH (GAME_DESIGN 3, 5 and 6; TECH_SPEC 3): a level, its state, a move, the solver. Integers only,
// no allocation after start-up, no clock: the app, the headless tool, the bake and the tests run exactly these rules.
//
// The one rule every element keeps (GAME_DESIGN 3.5): with everything else equal, a larger host is never worse. The
// solver's pruning rests on it, and a unit test compares the solver with a brute-force search to hold it.

#include "core/types.h"

namespace pm::sim {

constexpr int kMaxSites = 30;   // a move is a site index; move kRivalMove strikes the rival host
constexpr int kRivalMove = 30;
constexpr int kMaxRoads = 48;
constexpr int kMaxRaids = 6;
constexpr int kMaxRoute = 8;
constexpr i64 kCap = 999'999'999;
constexpr u8 kNoLock = 0xFF;
constexpr int kMaxTurns = 200;

enum Kind : u8 { kHome, kCamp, kVillage, kShrine, kPost, kMire, kDepot, kKey, kKeep, kRivalCamp, kKindCount };
enum LevelKind : u8 { kConquest, kGauntlet, kSiege, kRace, kLastStand, kLevelKindCount };

struct Site {
    u8 kind = kCamp;
    u8 fork = 0;          // fork group (0: none): take one and the free others burn
    i16 x = 0, y = 0;     // map units on the page (100 wide, `pageH` tall, y down)
    i32 value = 0;        // camp, depot, key, keep: its strength; village +A; shrine xM; post -T
    i16 perTurn = 0;      // growing (> 0) or deserting (< 0): the number moves this much a turn
    i32 lo = 1, hi = 0;   // and stays in [lo, hi] (hi 0: no cap)
    i16 until = -1;       // shrine: the last turn at full strength (-1: it never fades); after it, `aux`
    i32 aux = 0;          // depot: what it takes off the Keep; fading shrine: its multiplier after `until`
};

struct Road {
    u8 a = 0, b = 0;
    u8 lock = kNoLock;    // a key tower's site: the road is open once that tower is the host's
};

struct Raid {
    u8 turn = 0;          // the turn it arrives (raids are sorted by it)
    u8 entry = 0;         // the site whose side of the page it comes from (drawing only)
    i32 strength = 0;
};

struct LevelDef {
    u8 kind = kConquest, recipe = 0, world = 1, boss = 0;
    u8 siteCount = 0, roadCount = 0, raidCount = 0, routeLen = 0;
    i16 pageH = 160;      // the page's height in map units (a Gauntlet's is taller)
    u8 hint = 0;          // a hint line (game/strings), 0: none
    i32 start = 10;       // the host's strength at the start
    i32 rivalStart = 0;   // the rival host's (Race)
    u32 seed = 0;         // the generator's seed: the dressing (trees, terraces) is drawn from it
    i64 best = 0;         // the largest final host of any winning line (the solver; the bake stores it)
    Site sites[kMaxSites];
    Road roads[kMaxRoads];
    Raid raids[kMaxRaids];
    u8 route[kMaxRoute] = {};  // the rival's route: route[0] is its camp
    int keep() const;          // the Keep's site, or -1 (a Last Stand has none)
};

// Flags of a state.
enum : u8 { kWon = 1, kLost = 2, kRivalAlive = 4 };
enum LossReason : u8 { kLossNone = 0, kLossStranded = 1, kLossRaid = 2, kLossRival = 3 };

struct State {
    i64 s = 0;            // the host
    i64 rival = 0;        // the rival host (0: none, or it fell)
    u32 mine = 1;         // sites that are the host's (site 0 is Home)
    u32 burnt = 0;        // sites lost to a fork
    u32 razed = 0;        // sites the rival took
    u8 turn = 0, rivalAt = 0, raidNext = 0, flags = 0, reason = 0;
    bool won() const { return flags & kWon; }
    bool lost() const { return flags & kLost; }
    bool over() const { return flags & (kWon | kLost); }
    bool rivalAlive() const { return flags & kRivalAlive; }
};

// What a move did, in order (TECH_SPEC 3.3): the view turns these into clips and sounds.
enum EventType : u8 {
    kEvTake,        // site, a = host before, b = after
    kEvBurn,        // site burnt by a fork
    kEvOpen,        // site = the road (index) a key tower opened
    kEvTurn,        // a = the new turn
    kEvRivalMove,   // site = the camp it took, a = rival before, b = after
    kEvRivalGates,  // the rival marches on Home
    kEvRivalJoin,   // the rival fell to the host (a strike, or at the gates): a = host before, b = after
    kEvRivalWin,    // the rival took Home: lost
    kEvRaidJoin,    // site = raid index; a = host before, b = after
    kEvRaidWin,     // site = raid index: lost
    kEvWon,
    kEvLost,        // a = the reason
};
struct Event {
    u8 type = 0, site = 0;
    i64 a = 0, b = 0;
};
struct Events {
    static constexpr int kMax = 64;
    Event e[kMax];
    int n = 0;
    void add(u8 type, int site, i64 a = 0, i64 b = 0) {
        if (n < kMax) e[n++] = {type, u8(site), a, b};
    }
};

State start(const LevelDef& d);
// A site's number now (GAME_DESIGN 3.1): growth, fading, the Keep minus the depots taken.
i64 number(const LevelDef& d, const State& st, int site);
bool roadOpen(const LevelDef& d, const State& st, int road);
bool isFree(const State& st, int site);
// Free sites joined to the host's land by an open road (bit kRivalMove: the rival can be struck).
u32 reachable(const LevelDef& d, const State& st);
// Reachable sites whose condition holds (GAME_DESIGN 5.1), and the rival when it can be struck.
u32 takeable(const LevelDef& d, const State& st);
// Why a site cannot be taken: 0 it can, 1 not reachable, 2 too strong (`need` the host it takes), 3 burnt, 4 the host's
// already, 5 road locked, 6 the rival's.
int whyNot(const LevelDef& d, const State& st, int site, i64* need = nullptr);
// The move: `site` must be takeable. Runs the end of the turn (the rival, the raids) and passes turns while nothing can
// be taken and something is still coming (GAME_DESIGN 3.3, 3.4).
void take(const LevelDef& d, State& st, int site, Events* ev);
// Where the rival host stands (a site), or -1 at Home's gates or when it is gone.
int rivalSite(const LevelDef& d, const State& st);
// The final host if `site` wins the level now (the Keep, or the last raid ahead), else 0. For stars and the HUD.
i64 effectOf(const LevelDef& d, const State& st, int site);  // the host after taking `site` (no turn end)

// The solver (TECH_SPEC 3.5): the largest final host over all winning lines from a state, and a first move of a line
// that reaches it. Owns a memo table allocated once; not thread-safe (one per thread).
struct Solve {
    bool winnable = false;
    bool proven = true;   // false: the search hit its limits and the answer is not exact
    i64 best = 0;
    int first = -1;       // a move toward `best` (-1: none wins)
    int expansions = 0;
};
class Solver {
public:
    static constexpr int kMaxExpansions = 400000;
    Solver();
    ~Solver();
    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;
    Solve run(const LevelDef& d, const State& from, int maxExpansions = kMaxExpansions);

private:
    struct Entry;
    void dfs(const LevelDef& d, const State& st, int depth);
    bool memo(const State& st);  // true: a state as good or better was already searched
    Entry* table_ = nullptr;
    u32 gen_ = 0;
    int used_ = 0, expansions_ = 0, limit_ = 0, root_ = -1;
    bool aborted_ = false;
    Solve out_;
};

// Stars for a won level (GAME_DESIGN 7.1): 3 at the best final host, 2 at star2Pct of it, else 1; a hint caps at 2.
int starsFor(i64 finalHost, i64 best, bool hinted, int star2Pct = 70);

// Levels (src/sim/levels.cpp): the hand-made ones and the baked table, as one numbered list from 1.
int levelCount();
const LevelDef& level(int n);  // 1..levelCount()
// The compact text form of a level (the bake writes it, the app reads it): false on a malformed line.
bool decodeLevel(const char* text, LevelDef& out);
int encodeLevel(const LevelDef& d, char* out, int cap);  // bytes written (0: did not fit)
const char* kindName(u8 kind);       // "CAMP", ...
const char* objective(const LevelDef& d);  // the objective chip: "TAKE THE KEEP", ...

}  // namespace pm::sim
