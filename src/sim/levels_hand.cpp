// The hand-made levels (GAME_DESIGN 8.1 and 5.2). Positions are map units on a 100 x pageH page, y down (the Keep near
// the top, Home near the bottom). Numbers are exact: docs/concept/check_levels.py and the tests hold the stated results.
#include "sim/levels_hand.h"

namespace pm::sim {

namespace {

const char* const kHints[kHintCount] = {
    "",
    "TAP THE CAMP TO TAKE IT",     // 1
    "GOLD: YOU CAN TAKE IT",       // 2
    "VILLAGES JOIN FOR FREE",      // 3
    "GROW FIRST, DOUBLE LAST",     // 4
    "A POST COSTS SOLDIERS",       // 5
    "TAKE ONE, THE OTHER BURNS",   // 6
    "IT GROWS EVERY TURN",         // 7
    "PICK ONE SIDE EACH ROW",      // 8
    "THE TOWER OPENS THE GATE",    // 9
    "THE MIRE HALVES YOUR HOST",   // 10
    "THEY DESERT EVERY TURN",      // 11
    "DEPOTS WEAKEN THE KEEP",      // 12
    "THE SHRINE FADES SOON",       // 13
    "BEAT THE WARLORD TO CAMPS",   // 14
    "BE READY WHEN RAIDS ARRIVE",  // 15
    "UNDO IS FREE: TRY AGAIN",     // 16
    "", "", "", "", "", "", "",
};

void finish(LevelBuilder& b, int start, int hint) {
    b.d.start = start;
    b.d.hint = u8(hint);
}

}  // namespace

const char* hintText(int hint) { return hint > 0 && hint < kHintCount ? kHints[hint] : ""; }

bool handLevel(int n, LevelDef& out) {
    LevelBuilder b;
    b.d.seed = u32(n * 7919 + 13);
    switch (n) {
        case 1: {  // a line: cannot be lost (final 43)
            const int h = b.site(kHome, 50, 146, 0), a = b.site(kCamp, 36, 114, 4), c = b.site(kCamp, 62, 80, 9),
                      k = b.site(kKeep, 48, 42, 20);
            b.road(h, a), b.road(a, c), b.road(c, k);
            finish(b, 10, 1);
            break;
        }
        case 2: {  // smallest first: B is too strong until A has joined (best 58)
            const int h = b.site(kHome, 50, 146, 0), a = b.site(kCamp, 26, 104, 6), c = b.site(kCamp, 74, 104, 14),
                      k = b.site(kKeep, 50, 44, 28);
            b.road(h, a), b.road(h, c), b.road(a, k), b.road(c, k);
            finish(b, 10, 2);
            break;
        }
        case 3: {  // a village (best 45)
            const int h = b.site(kHome, 58, 146, 0), v = b.site(kVillage, 24, 120, 8), c = b.site(kCamp, 62, 96, 10),
                      k = b.site(kKeep, 50, 44, 22);
            b.road(h, v), b.road(h, c), b.road(c, k);
            finish(b, 5, 3);
            break;
        }
        case 4: {  // the shrine: first it strands at 26, last it doubles 40 (best 80)
            const int h = b.site(kHome, 50, 148, 0), c5 = b.site(kCamp, 26, 116, 5), sh = b.site(kShrine, 74, 116, 2),
                      c9 = b.site(kCamp, 50, 84, 9), k = b.site(kKeep, 50, 40, 40);
            b.road(h, c5), b.road(h, sh), b.road(c5, c9), b.road(sh, c9), b.road(c9, k);
            finish(b, 6, 4);
            break;
        }
        case 5: {  // a post over the river (best 87)
            const int h = b.site(kHome, 50, 148, 0), c3 = b.site(kCamp, 20, 138, 3), c8 = b.site(kCamp, 74, 124, 8),
                      p = b.site(kPost, 62, 96, 6), v = b.site(kVillage, 30, 80, 10), c20 = b.site(kCamp, 38, 54, 20),
                      k = b.site(kKeep, 66, 34, 40);
            b.road(h, c3), b.road(h, c8), b.road(c8, p), b.road(p, v), b.road(v, c20), b.road(c20, k);
            finish(b, 12, 5);
            break;
        }
        case 6: {  // a fork: the village strands at 22 (best 61)
            const int h = b.site(kHome, 50, 148, 0), c6 = b.site(kCamp, 50, 116, 6), l = b.site(kCamp, 24, 84, 15),
                      r = b.site(kVillage, 76, 84, 6), k = b.site(kKeep, 50, 42, 30);
            b.road(h, c6), b.road(c6, l), b.road(c6, r), b.road(l, k), b.road(r, k);
            b.fork(l, 1), b.fork(r, 1);
            finish(b, 10, 6);
            break;
        }
        case 7: {  // Hourglass: two lobes joined by a bridge; the shrine is best kept for last
            const int h = b.site(kHome, 50, 150, 0), a = b.site(kCamp, 24, 132, 7), v = b.site(kVillage, 76, 132, 5),
                      c = b.site(kCamp, 50, 112, 15), p = b.site(kPost, 50, 90, 6), d = b.site(kCamp, 24, 70, 22),
                      s = b.site(kShrine, 76, 68, 2), f = b.site(kCamp, 50, 50, 40), k = b.site(kKeep, 50, 28, 75);
            b.road(h, a), b.road(h, v), b.road(a, c), b.road(v, c), b.road(c, p), b.road(p, d), b.road(p, s), b.road(d, f),
                b.road(s, f), b.road(f, k);
            finish(b, 9, 16);
            break;
        }
        case 8: {  // Tree: three branches from Home meet under the Keep
            const int h = b.site(kHome, 50, 150, 0), l1 = b.site(kCamp, 22, 128, 5), r1 = b.site(kVillage, 78, 128, 4),
                      l2 = b.site(kCamp, 14, 96, 12), r2 = b.site(kCamp, 86, 98, 16), m = b.site(kCamp, 50, 100, 25),
                      l3 = b.site(kVillage, 22, 64, 10), r3 = b.site(kShrine, 78, 66, 2), k = b.site(kKeep, 50, 32, 70);
            b.road(h, l1), b.road(h, r1), b.road(l1, l2), b.road(r1, r2), b.road(l1, m), b.road(r1, m), b.road(l2, l3),
                b.road(r2, r3), b.road(l3, k), b.road(r3, k), b.road(m, k);
            finish(b, 8, 0);
            break;
        }
        case 12: {  // the concept frame's level (GAME_DESIGN 2): 285 by the obvious line, 396 best
            const int h = b.site(kHome, 50, 150, 0), a = b.site(kCamp, 25, 132, 6), v = b.site(kVillage, 75, 130, 8),
                      c = b.site(kCamp, 23, 104, 14), d = b.site(kShrine, 76, 106, 2), e = b.site(kPost, 50, 86, 5),
                      f = b.site(kCamp, 20, 66, 30), g = b.site(kCamp, 80, 64, 45), i = b.site(kCamp, 50, 52, 12),
                      k = b.site(kKeep, 50, 28, 120);
            b.grow(i, 3, 1, 45);
            b.road(h, a), b.road(h, v), b.road(a, c), b.road(v, d), b.road(c, e), b.road(d, e), b.road(e, f), b.road(e, g),
                b.road(f, i), b.road(g, i), b.road(i, k);
            finish(b, 10, 7);
            break;
        }
        default:
            return false;
    }
    b.d.world = u8(1 + (n - 1) / 10 % 10);
    out = b.d;
    return true;
}

}  // namespace pm::sim
