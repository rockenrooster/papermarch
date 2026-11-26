// The rules (GAME_DESIGN 3, 5.1, 6; TECH_SPEC 3.2): start, numbers, reach, the move and the end of a turn.
#include "sim/level.h"

namespace pm::sim {

namespace {

inline u32 bit(int i) { return 1u << u32(i); }

i64 capped(i64 v) { return v > kCap ? kCap : v; }

void lose(State& st, u8 reason, Events* ev) {
    st.flags |= kLost;
    st.reason = reason;
    if (ev) ev->add(kEvLost, 0, reason);
}

void win(State& st, Events* ev) {
    st.flags |= kWon;
    if (ev) ev->add(kEvWon, 0, st.s);
}

// Every road a key tower opens, announced when the tower becomes the host's.
void announceOpened(const LevelDef& d, int key, Events* ev) {
    if (!ev) return;
    for (int r = 0; r < d.roadCount; ++r)
        if (d.roads[r].lock == key) ev->add(kEvOpen, r);
}

// The rival falls to the host: its strength and the camps it took become the host's.
void rivalJoins(State& st, Events* ev) {
    const i64 before = st.s;
    st.s = capped(st.s + st.rival);
    st.rival = 0;
    st.flags &= u8(~kRivalAlive);
    st.mine |= st.razed;
    st.razed = 0;
    if (ev) ev->add(kEvRivalJoin, 0, before, st.s);
}

// The end of a turn: the rival steps, then the raids due arrive (GAME_DESIGN 6.4, 6.5).
void endTurn(const LevelDef& d, State& st, Events* ev) {
    if (st.rivalAlive()) {
        if (st.rivalAt < d.routeLen) {
            int j = st.rivalAt + 1;
            while (j < d.routeLen && !isFree(st, d.route[j])) ++j;
            if (j < d.routeLen) {
                const int site = d.route[j];
                const i64 before = st.rival;
                st.rival = capped(st.rival + number(d, st, site));
                st.razed |= bit(site);
                st.rivalAt = u8(j);
                if (ev) ev->add(kEvRivalMove, site, before, st.rival);
            } else {
                st.rivalAt = d.routeLen;  // at Home's gates: it arrives at the end of the next turn
                if (ev) ev->add(kEvRivalGates, 0, st.rival);
            }
        } else if (st.s >= st.rival) {
            rivalJoins(st, ev);
        } else {
            if (ev) ev->add(kEvRivalWin, 0, st.s, st.rival);
            lose(st, kLossRival, ev);
            return;
        }
    }
    while (st.raidNext < d.raidCount && d.raids[st.raidNext].turn <= st.turn) {
        const Raid& r = d.raids[st.raidNext];
        if (st.s >= r.strength) {
            const i64 before = st.s;
            st.s = capped(st.s + r.strength);
            if (ev) ev->add(kEvRaidJoin, st.raidNext, before, st.s);
            ++st.raidNext;
        } else {
            if (ev) ev->add(kEvRaidWin, st.raidNext, st.s, r.strength);
            lose(st, kLossRaid, ev);
            return;
        }
    }
    if (d.keep() < 0 && d.raidCount > 0 && st.raidNext >= d.raidCount) win(st, ev);
}

bool pending(const LevelDef& d, const State& st) { return st.rivalAlive() || st.raidNext < d.raidCount; }

}  // namespace

int LevelDef::keep() const {
    for (int i = 0; i < siteCount; ++i)
        if (sites[i].kind == kKeep) return i;
    return -1;
}

bool isFree(const State& st, int site) { return !((st.mine | st.burnt | st.razed) & bit(site)); }

State start(const LevelDef& d) {
    State st;
    st.s = d.start;
    st.mine = bit(0);
    if (d.routeLen > 0 && d.rivalStart > 0) {
        st.rival = d.rivalStart;
        st.rivalAt = 0;
        st.razed = bit(d.route[0]);
        st.flags |= kRivalAlive;
    }
    return st;
}

i64 number(const LevelDef& d, const State& st, int i) {
    const Site& t = d.sites[i];
    if (t.kind == kShrine) return (t.until >= 0 && st.turn > t.until) ? t.aux : t.value;
    i64 v = i64(t.value) + i64(t.perTurn) * i64(st.turn);
    if (t.hi > 0 && v > t.hi) v = t.hi;
    if (v < t.lo) v = t.lo;
    if (t.kind == kKeep) {
        for (int j = 0; j < d.siteCount; ++j)
            if (d.sites[j].kind == kDepot && (st.mine & bit(j))) v -= d.sites[j].aux;
        if (v < 1) v = 1;
    }
    return v;
}

bool roadOpen(const LevelDef& d, const State& st, int r) {
    const u8 lock = d.roads[r].lock;
    return lock == kNoLock || (st.mine & bit(lock));
}

int rivalSite(const LevelDef& d, const State& st) {
    if (!st.rivalAlive() || st.rivalAt >= d.routeLen) return -1;
    return d.route[st.rivalAt];
}

u32 reachable(const LevelDef& d, const State& st) {
    u32 out = 0;
    const int rs = rivalSite(d, st);
    for (int r = 0; r < d.roadCount; ++r) {
        if (!roadOpen(d, st, r)) continue;
        const int a = d.roads[r].a, b = d.roads[r].b;
        if ((st.mine & bit(a)) && isFree(st, b)) out |= bit(b);
        if ((st.mine & bit(b)) && isFree(st, a)) out |= bit(a);
        if (rs >= 0 && (((st.mine & bit(a)) && b == rs) || ((st.mine & bit(b)) && a == rs))) out |= bit(kRivalMove);
    }
    if (st.rivalAlive() && st.rivalAt >= d.routeLen) out |= bit(kRivalMove);  // at the gates
    return out;
}

static bool conditionHolds(u8 kind, i64 s, i64 v) {
    switch (kind) {
        case kCamp: case kDepot: case kKey: case kKeep: case kRivalCamp: return s >= v;
        case kVillage: case kShrine: return true;
        case kPost: return s > v;
        case kMire: return s >= 2;
        default: return false;
    }
}

u32 takeable(const LevelDef& d, const State& st) {
    if (st.over()) return 0;
    const u32 reach = reachable(d, st);
    u32 out = 0;
    for (int i = 0; i < d.siteCount; ++i)
        if ((reach & bit(i)) && conditionHolds(d.sites[i].kind, st.s, number(d, st, i))) out |= bit(i);
    if ((reach & bit(kRivalMove)) && st.s >= st.rival) out |= bit(kRivalMove);
    return out;
}

int whyNot(const LevelDef& d, const State& st, int i, i64* need) {
    if (st.mine & bit(i)) return 4;
    if (st.burnt & bit(i)) return 3;
    if (st.razed & bit(i)) return 6;
    if (!(reachable(d, st) & bit(i))) {
        // A road to it exists from the host's land but is locked.
        for (int r = 0; r < d.roadCount; ++r) {
            const int a = d.roads[r].a, b = d.roads[r].b;
            if (((st.mine & bit(a)) && b == i) || ((st.mine & bit(b)) && a == i)) return 5;
        }
        return 1;
    }
    const i64 v = number(d, st, i);
    if (!conditionHolds(d.sites[i].kind, st.s, v)) {
        if (need) *need = d.sites[i].kind == kPost ? v + 1 : d.sites[i].kind == kMire ? 2 : v;
        return 2;
    }
    return 0;
}

i64 effectOf(const LevelDef& d, const State& st, int i) {
    if (i == kRivalMove) return capped(st.s + st.rival);
    const i64 v = number(d, st, i);
    switch (d.sites[i].kind) {
        case kCamp: case kDepot: case kKey: case kKeep: case kVillage: case kRivalCamp: return capped(st.s + v);
        case kShrine: return capped(st.s * v);
        case kPost: return st.s - v;
        case kMire: return (st.s + 1) / 2;
        default: return st.s;
    }
}

void take(const LevelDef& d, State& st, int i, Events* ev) {
    if (st.over()) return;
    const i64 before = st.s;
    if (i == kRivalMove) {
        if (!(takeable(d, st) & bit(kRivalMove))) return;
        const int rs = rivalSite(d, st);
        if (rs >= 0) st.razed |= bit(rs);  // it stands on a camp it took: that camp comes with it
        rivalJoins(st, ev);
    } else {
        if (!(takeable(d, st) & bit(i))) return;
        const Site& t = d.sites[i];
        st.s = effectOf(d, st, i);
        st.mine |= bit(i);
        if (ev) ev->add(kEvTake, i, before, st.s);
        if (t.fork) {
            for (int j = 0; j < d.siteCount; ++j)
                if (j != i && d.sites[j].fork == t.fork && isFree(st, j)) {
                    st.burnt |= bit(j);
                    if (ev) ev->add(kEvBurn, j);
                }
        }
        if (t.kind == kKey) announceOpened(d, i, ev);
        if (t.kind == kKeep) {
            win(st, ev);
            return;
        }
    }
    // The end of the turn, then turns pass by themselves while nothing can be taken and something is still coming.
    for (int guard = 0; guard < kMaxTurns; ++guard) {
        if (st.turn < 255) ++st.turn;
        if (ev && guard > 0) ev->add(kEvTurn, 0, st.turn);
        endTurn(d, st, ev);
        if (st.over()) return;
        if (takeable(d, st)) return;
        if (!pending(d, st)) {
            lose(st, kLossStranded, ev);
            return;
        }
    }
    lose(st, kLossStranded, ev);
}

int starsFor(i64 finalHost, i64 best, bool hinted, int star2Pct) {
    int s = 1;
    if (best > 0 && finalHost >= best) s = 3;
    else if (best > 0 && finalHost * 100 >= best * i64(star2Pct)) s = 2;
    if (hinted && s > 2) s = 2;
    return s;
}

}  // namespace pm::sim
