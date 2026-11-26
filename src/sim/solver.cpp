// The solver (TECH_SPEC 3.5): depth-first over moves with a memo keyed by everything in a state but the host. A state
// reached again with a host no larger than one already searched from it is pruned: by GAME_DESIGN 3.5 nothing better
// can come of it. Moves are tried largest resulting host first, so the pruning bites early.
#include <cstdlib>
#include <cstring>

#include "sim/level.h"

namespace pm::sim {

namespace {
constexpr u32 kTableBits = 18;
constexpr u32 kTableSize = 1u << kTableBits;
constexpr int kProbes = 32;

inline u64 mix(u64 x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}
}  // namespace

struct Solver::Entry {
    u64 k1, k2;
    i64 s;
    u32 gen, pad;
};

Solver::Solver() {
    table_ = static_cast<Entry*>(std::calloc(kTableSize, sizeof(Entry)));
}

Solver::~Solver() { std::free(table_); }

bool Solver::memo(const State& st) {
    const u64 k1 = u64(st.mine) | (u64(st.razed) << 32);
    const u64 k2 = u64(st.turn) | (u64(st.rivalAt) << 8) | (u64(st.raidNext) << 16) | (u64(st.flags) << 24) |
                   (u64(st.rival) << 32);
    u32 h = u32(mix(k1 ^ mix(k2)) & (kTableSize - 1));
    for (int p = 0; p < kProbes; ++p, h = (h + 1) & (kTableSize - 1)) {
        Entry& e = table_[h];
        if (e.gen != gen_) {
            e.k1 = k1;
            e.k2 = k2;
            e.s = st.s;
            e.gen = gen_;
            ++used_;
            return false;
        }
        if (e.k1 == k1 && e.k2 == k2) {
            if (e.s >= st.s) return true;
            e.s = st.s;
            return false;
        }
    }
    aborted_ = true;  // the neighbourhood is full: the answer may not be exact
    return false;
}

void Solver::dfs(const LevelDef& d, const State& st, int depth) {
    if (++expansions_ > limit_) {
        aborted_ = true;
        return;
    }
    if (memo(st)) return;
    const u32 moves = takeable(d, st);
    // Children, largest host first (insertion sort over at most 31).
    State child[31];
    int order[31];
    int n = 0;
    for (int m = 0; m <= kRivalMove; ++m) {
        if (!(moves & (1u << u32(m)))) continue;
        child[n] = st;
        take(d, child[n], m, nullptr);
        order[n] = m;
        int k = n++;
        while (k > 0 && child[k].s > child[k - 1].s) {
            const State ts = child[k];
            child[k] = child[k - 1];
            child[k - 1] = ts;
            const int to = order[k];
            order[k] = order[k - 1];
            order[k - 1] = to;
            --k;
        }
    }
    for (int k = 0; k < n && !aborted_; ++k) {
        if (depth == 0) root_ = order[k];
        const State& c = child[k];
        if (c.won()) {
            if (!out_.winnable || c.s > out_.best) {
                out_.winnable = true;
                out_.best = c.s;
                out_.first = root_;
            }
        } else if (!c.lost()) {
            dfs(d, c, depth + 1);
        }
    }
}

Solve Solver::run(const LevelDef& d, const State& from, int maxExpansions) {
    out_ = Solve();
    if (!table_) {
        out_.proven = false;
        return out_;
    }
    if (++gen_ == 0) {  // wrapped: clear the generations once every four billion runs
        std::memset(table_, 0, sizeof(Entry) * kTableSize);
        gen_ = 1;
    }
    used_ = 0;
    expansions_ = 0;
    limit_ = maxExpansions;
    aborted_ = false;
    root_ = -1;
    if (from.won()) {
        out_.winnable = true;
        out_.best = from.s;
    } else if (!from.lost()) {
        dfs(d, from, 0);
    }
    out_.proven = !aborted_;
    out_.expansions = expansions_;
    return out_;
}

}  // namespace pm::sim
