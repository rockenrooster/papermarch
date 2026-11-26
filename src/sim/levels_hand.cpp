// The hand-made levels (GAME_DESIGN 8.1): the first eight wave battles, each teaching one thing. The army starts at the
// bottom; rows of choices (in one or two lanes) go up the field; a wave comes after the rows that name one; the castle is
// last. The tests hold each level's stated best (src/tools/tests.cpp).
#include "sim/levels_hand.h"

namespace pm::sim {

namespace {

const char* const kHints[kHintCount] = {
    "",
    "TAP A CARD, THEN BEAT THE WAVE",   // 1
    "PICK THE BIGGER ARMY",             // 2
    "\xC3\x97" "2 DOUBLES YOUR WHOLE ARMY",  // 3
    "BE STRONG WHEN THE WAVE HITS",     // 4
    "BEAT A CAMP: THEY JOIN YOU",       // 5
    "A TOLL COSTS SOLDIERS",            // 6
    "YOUR LANE DECIDES WHAT'S NEXT",    // 7
    "SAVE \xC3\x97" "2 FOR A BIG ARMY", // 8
    "THREE LANES: PLAN AHEAD",          // 9
    "THE SWAMP HALVES YOUR ARMY",       // 10
    "EVERY LANE MEETS HERE",            // 11
    "\xC3\x97" "3 TRIPLES YOUR ARMY",   // 12
    "TWO WAVES AT ONCE",                // 13
    "THE KEY OPENS A NEW TRACK",        // 14
    "SUPPLY CAMPS WEAKEN THE CASTLE",   // 15
    "UNDO IS FREE: TRY AGAIN",          // 16
    "", "", "", "", "", "", "",
};

struct Opt {
    u8 kind;
    i32 value;
};

// A wave battle's grid: Home at the bottom, `rows` rows of `lanes` choices (a row of several is a fork), the castle at the
// top. Tracks: straight up each lane; `cross` also links every lane to every lane between rows (bit i: after row i).
struct Grid {
    LevelBuilder b;
    int ids[8][3] = {};
    int lanes = 1, rows = 1, keep = 0, home = 0;
    Grid(int lanes_, int rows_, u32 cross, const Opt (*opts)[3], int castle) : lanes(lanes_), rows(rows_) {
        const int H = 96 + rows * 40;
        b.d.kind = kWaves;
        b.d.pageH = i16(H);
        home = b.site(kHome, 50, H - 16, 0);
        for (int i = 0; i < rows; ++i)
            for (int k = 0; k < lanes; ++k) {
                const int x = lanes == 1 ? 50 : (k == 0 ? 30 : 70);
                ids[i][k] = b.site(opts[i][k].kind, x, H - 50 - i * 40, opts[i][k].value);
                if (lanes > 1) b.fork(ids[i][k], i + 1);
            }
        keep = b.site(kKeep, 50, 30, castle);
        for (int k = 0; k < lanes; ++k) b.road(home, ids[0][k]);
        for (int i = 0; i + 1 < rows; ++i)
            for (int a = 0; a < lanes; ++a)
                for (int c = 0; c < lanes; ++c)
                    if (a == c || (cross & (1u << i))) b.road(ids[i][a], ids[i + 1][c]);
        for (int k = 0; k < lanes; ++k) b.road(ids[rows - 1][k], keep);
    }
    // A wave after row `row` (0-based): it arrives at the end of that turn.
    void wave(int row, int strength) { b.raid(row + 1, ids[row][0], strength); }
};

}  // namespace

const char* hintText(int hint) { return hint > 0 && hint < kHintCount ? kHints[hint] : ""; }

bool handLevel(int n, LevelDef& out) {
    switch (n) {
        case 1: {  // one lane: recruit, beat the wave, fight a camp, beat the wave, take the castle (cannot be lost; 88)
            const Opt o[2][3] = {{{kVillage, 6}}, {{kCamp, 12}}};
            Grid g(1, 2, 0, o, 30);
            g.wave(0, 10);
            g.wave(1, 20);
            g.b.d.start = 10;
            g.b.d.hint = 1;
            out = g.b.d;
            break;
        }
        case 2: {  // the smaller recruit loses to the first wave
            const Opt o[2][3] = {{{kVillage, 8}, {kVillage, 15}}, {{kVillage, 10}, {kVillage, 25}}};
            Grid g(2, 2, 1, o, 60);
            g.wave(0, 20);
            g.wave(1, 40);
            g.b.d.start = 10;
            g.b.d.hint = 2;
            out = g.b.d;
            break;
        }
        case 3: {  // x2 against +, three times
            const Opt o[3][3] = {{{kVillage, 10}, {kShrine, 2}}, {{kShrine, 2}, {kVillage, 30}}, {{kVillage, 40}, {kShrine, 2}}};
            Grid g(2, 3, 3, o, 200);
            g.wave(0, 15);
            g.wave(1, 50);
            g.wave(2, 120);
            g.b.d.start = 12;
            g.b.d.hint = 3;
            out = g.b.d;
            break;
        }
        case 4: {  // doubling first leaves the army one short of the wave
            const Opt o[3][3] = {{{kShrine, 2}, {kVillage, 12}}, {{kShrine, 2}, {kVillage, 40}}, {{kVillage, 60}, {kShrine, 2}}};
            Grid g(2, 3, 3, o, 300);
            g.wave(0, 21);
            g.wave(1, 70);
            g.wave(2, 200);
            g.b.d.start = 10;
            g.b.d.hint = 4;
            out = g.b.d;
            break;
        }
        case 5: {  // camps: beat them and they join; a camp too strong stays locked
            const Opt o[3][3] = {{{kCamp, 12}, {kVillage, 8}}, {{kCamp, 50}, {kVillage, 20}}, {{kCamp, 90}, {kShrine, 2}}};
            Grid g(2, 3, 3, o, 250);
            g.wave(0, 20);
            g.wave(1, 45);
            g.wave(2, 150);
            g.b.d.start = 15;
            g.b.d.hint = 5;
            out = g.b.d;
            break;
        }
        case 6: {  // a toll every lane must pay, between two choices
            LevelBuilder b;
            const int H = 96 + 3 * 40;
            b.d.kind = kWaves;
            b.d.pageH = i16(H);
            const int h = b.site(kHome, 50, H - 16, 0);
            const int a0 = b.site(kVillage, 30, H - 50, 10), a1 = b.site(kCamp, 70, H - 50, 8);
            const int t = b.site(kPost, 50, H - 90, 6);
            const int c0 = b.site(kShrine, 30, H - 130, 2), c1 = b.site(kVillage, 70, H - 130, 25);
            const int k = b.site(kKeep, 50, 30, 80);
            b.fork(a0, 1), b.fork(a1, 1), b.fork(c0, 3), b.fork(c1, 3);
            b.road(h, a0), b.road(h, a1), b.road(a0, t), b.road(a1, t), b.road(t, c0), b.road(t, c1), b.road(c0, k), b.road(c1, k);
            b.raid(1, a0, 15);
            b.raid(3, c0, 40);
            b.d.start = 12;
            b.d.hint = 6;
            out = b.d;
            break;
        }
        case 7: {  // no crossings: the bigger recruit now leads up the poorer lane
            const Opt o[3][3] = {{{kVillage, 10}, {kVillage, 30}}, {{kVillage, 10}, {kVillage, 5}}, {{kShrine, 3}, {kVillage, 5}}};
            Grid g(2, 3, 0, o, 60);
            g.wave(0, 15);
            g.wave(2, 40);
            g.b.d.start = 10;
            g.b.d.hint = 7;
            out = g.b.d;
            break;
        }
        case 8: {  // doubling now beats recruiting now, but the other lane doubles a bigger army later
            const Opt o[3][3] = {{{kShrine, 2}, {kVillage, 15}}, {{kVillage, 20}, {kVillage, 20}}, {{kVillage, 30}, {kShrine, 2}}};
            Grid g(2, 3, 0, o, 100);
            g.wave(0, 15);
            g.wave(2, 50);
            g.b.d.start = 20;
            g.b.d.hint = 8;
            out = g.b.d;
            break;
        }
        default:
            return false;
    }
    out.seed = u32(n * 7919 + 13);
    out.world = 1;
    return true;
}

}  // namespace pm::sim
