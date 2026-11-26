// The scene's drawing (GAME_DESIGN 9, TECH_SPEC 5): pop-ups, trees, figures, flags and confetti as stickers sorted far
// to near by group; tags as signed-distance text and rounded rectangles over everything.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "core/tuning.h"
#include "game/scene.h"
#include "sim/levels_hand.h"

namespace pm {

namespace {
inline u32 bit(int i) { return 1u << u32(i); }

float backEase(float t) {
    t = clampv(t, 0.0f, 1.0f);
    const float c1 = 1.70158f * 0.7f, c3 = c1 + 1.0f;
    const float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}
}  // namespace

void Scene::beginGroup(float key) {
    if (groupN_ > 0) groups_[groupN_ - 1].count = tmpN_ - groups_[groupN_ - 1].first;
    if (groupN_ >= kMaxGroups) return;
    groups_[groupN_++] = {key, tmpN_, 0};
}

void Scene::addSticker(const Sticker& in) {
    if (tmpN_ >= kMaxStickers || curRise_ <= 0.02f) return;
    Sticker s = in;
    if (curRise_ != 1.0f) {
        s.y = curBase_ + (s.y - curBase_) * curRise_;
        s.hh *= curRise_;
        s.lift *= curRise_;
    }
    if (curBurn_ > 0) {
        const float t = curBurn_ * 0.7f;
        for (u8* c : {s.c0, s.c1, s.c2}) {
            c[0] = u8(float(c[0]) + (110.0f - float(c[0])) * t);
            c[1] = u8(float(c[1]) + (96.0f - float(c[1])) * t);
            c[2] = u8(float(c[2]) + (84.0f - float(c[2])) * t);
        }
    }
    tmp_[tmpN_++] = s;
}

float Scene::riseAt(float y, bool keep) const {
    if (reduceEffects) return clampv(unfold_ / 0.2f, 0.0f, 1.0f);
    const float H = float(def_->pageH);
    const float delay = keep ? 0.55f : 0.45f * (1.0f - clampv((y - scroll_) / maxv(1.0f, fit_.viewH), 0.0f, 1.0f));
    (void)H;
    return backEase((unfold_ - 0.12f - delay) / 0.25f);
}

// ---- Pop-ups (GAME_DESIGN 5.1's last column) ---------------------------------------------------------------------------

void Scene::tent(float x, float y, float s, u32 light, u32 dark, u32 flag, float rise) {
    (void)rise;
    const float k = fit_.k;
    Sticker t = makeSticker(kShTri, sx(x), sy(y - 3.1f * s), 4.2f * s * k, 3.1f * s * k, light);
    setColor(t.c1, dark);
    t.fold = 1;
    t.lift = 1.7f * s * k;
    addSticker(t);
    Sticker door = makeSticker(kShTri, sx(x), sy(y - 1.5f * s), 0.95f * s * k, 1.5f * s * k, col::kInkDeep);
    addSticker(door);
    Sticker pole = makeSticker(kShBox, sx(x), sy(y - 7.0f * s), 0.16f * k, 1.2f * s * k, col::kWood);
    pole.lift = 2.0f * s * k;
    addSticker(pole);
    Sticker f = makeSticker(kShFlag, sx(x + 1.25f * s), sy(y - 7.7f * s), 1.15f * s * k, 0.48f * s * k, flag);
    f.p0 = time_ * 4.0f + x;
    setColor(f.c2, flag);
    f.lift = 2.2f * s * k;
    addSticker(f);
}

void Scene::house(float x, float y, float s, u32 wall, u32 wallDark, u32 roof, u32 roofDark, float rise) {
    (void)rise;
    const float k = fit_.k;
    Sticker w = makeSticker(kShBox, sx(x), sy(y - 1.5f * s), 2.4f * s * k, 1.5f * s * k, wall);
    setColor(w.c1, wallDark);
    w.fold = 1;
    w.lift = 1.0f * s * k;
    addSticker(w);
    Sticker r = makeSticker(kShTri, sx(x), sy(y - 4.6f * s), 3.2f * s * k, 1.7f * s * k, roof);
    setColor(r.c1, roofDark);
    r.fold = 1;
    r.lift = 1.6f * s * k;
    addSticker(r);
    Sticker door = makeSticker(kShBox, sx(x - 1.0f * s), sy(y - 0.8f * s), 0.42f * s * k, 0.8f * s * k, col::kWood);
    addSticker(door);
}

void Scene::tower(float x, float y, float w, float h, u32 light, u32 dark, u32 roof, u32 roofDark, bool crenel, float rise) {
    (void)rise;
    const float k = fit_.k;
    Sticker b = makeSticker(kShBox, sx(x), sy(y - h * 0.5f), w * 0.5f * k, h * 0.5f * k, light);
    setColor(b.c1, dark);
    b.fold = 1;
    b.lift = h * 0.28f * k;
    addSticker(b);
    if (crenel) {
        const int n = maxv(3, int(w / 2.2f) | 1);
        const float cw = w / float(n);
        for (int i = 0; i < n; i += 2) {
            const float cx = x - w * 0.5f + (float(i) + 0.5f) * cw;
            Sticker c = makeSticker(kShBox, sx(cx), sy(y - h - 0.8f), cw * 0.5f * k, 0.8f * k, cx < x ? light : dark);
            c.lift = h * 0.28f * k;
            addSticker(c);
        }
    }
    if (roof) {
        Sticker r = makeSticker(kShTri, sx(x), sy(y - h - 3.8f * (w / 7.6f) - 0.6f), (w * 0.5f + 0.8f) * k, 3.8f * (w / 7.6f) * k, roof);
        setColor(r.c1, roofDark);
        r.fold = 1;
        r.lift = h * 0.36f * k;
        addSticker(r);
    }
}

void Scene::flagAt(float x, float y, float h, float w, u32 c, u32 trim, float rise, float phase) {
    (void)rise;
    const float k = fit_.k;
    Sticker pole = makeSticker(kShBox, sx(x), sy(y - h * 0.5f), 0.18f * k, h * 0.5f * k, col::kWood);
    pole.lift = h * 0.3f * k;
    addSticker(pole);
    Sticker f = makeSticker(kShFlag, sx(x + w * 0.5f + 0.1f), sy(y - h + w * 0.32f), w * 0.5f * k, w * 0.3f * k, c);
    setColor(f.c2, trim);
    f.p0 = time_ * 4.5f + phase;
    f.lift = h * 0.34f * k;
    addSticker(f);
}

void Scene::treeAt(const Decor& d, float rise) {
    (void)rise;
    const WorldPalette& wp = worldPalette(def_->world);
    const float k = fit_.k, s = d.s;
    const bool accent = d.type == 1;
    const u32 dark = accent ? wp.tree2Dark : wp.treeDark, light = accent ? wp.tree2Light : wp.treeLight;
    if (d.type == 2) {  // a rock
        Sticker r = makeSticker(kShEllipse, sx(d.x), sy(d.y - 0.7f * s), 1.6f * s * k, 1.0f * s * k, 0xB8B0A4);
        setColor(r.c1, 0x8C8478);
        r.fold = 1;
        r.lift = 0.5f * k;
        addSticker(r);
        return;
    }
    Sticker trunk = makeSticker(kShBox, sx(d.x), sy(d.y - 0.6f * s), 0.38f * s * k, 0.65f * s * k, col::kWood);
    trunk.lift = 0.4f * k;
    switch (wp.trees) {
        case 1: case 4: {  // palms (accent: cacti on sand, bushes on islands)
            if (accent && wp.trees == 1) {
                Sticker c = makeSticker(kShCapsule, sx(d.x), sy(d.y - 2.6f * s), 2.6f * s * k, 0.75f * s * k, light);
                c.rot = 1.5708f;
                setColor(c.c1, dark);
                c.fold = 2;
                c.lift = 1.4f * s * k;
                addSticker(c);
                for (int side = -1; side <= 1; side += 2) {
                    Sticker a = makeSticker(kShCapsule, sx(d.x + side * 1.2f * s), sy(d.y - 3.2f * s), 1.0f * s * k, 0.5f * s * k, light);
                    a.rot = 1.5708f;
                    a.lift = 1.4f * s * k;
                    addSticker(a);
                }
                return;
            }
            Sticker t = makeSticker(kShBox, sx(d.x + 0.3f * s), sy(d.y - 2.4f * s), 0.32f * s * k, 2.4f * s * k, col::kWood);
            t.rot = 0.12f;
            t.lift = 1.2f * s * k;
            addSticker(t);
            for (int i = 0; i < 4; ++i) {
                const float a = -0.6f + 1.25f * float(i) - (i >= 2 ? 0.0f : 0.0f);
                Sticker l = makeSticker(kShEllipse, sx(d.x + 0.6f * s + std::cos(a + 2.4f) * 1.6f * s),
                                        sy(d.y - 4.9f * s + std::sin(a + 2.4f) * 0.5f * s), 2.0f * s * k, 0.6f * s * k,
                                        i % 2 ? dark : light);
                l.rot = a + 2.4f;
                l.lift = 2.2f * s * k;
                addSticker(l);
            }
            return;
        }
        case 3: case 5: case 7: case 9: {  // round crowns: maples, blossoms, bushes, plums with lanterns
            addSticker(trunk);
            const float hgt = wp.trees == 7 ? 0.0f : 1.0f;
            Sticker c = makeSticker(kShEllipse, sx(d.x), sy(d.y - (2.2f + 1.4f * hgt) * s), 2.4f * s * k, 2.2f * s * k, light);
            setColor(c.c1, dark);
            c.fold = 1;
            c.lift = (1.6f + hgt) * s * k;
            addSticker(c);
            if (wp.trees == 9 && accent) {  // a paper lantern hanging from the bough
                Sticker l = makeSticker(kShEllipse, sx(d.x + 1.5f * s), sy(d.y - 1.6f * s), 0.55f * s * k, 0.7f * s * k, 0xFF8A3D);
                setColor(l.c1, 0xD06018);
                l.fold = 1;
                l.lift = 1.0f * k;
                addSticker(l);
            }
            return;
        }
        case 6: {  // mushrooms (accent: reeds)
            if (accent) {
                for (int i = -1; i <= 1; ++i) {
                    Sticker r = makeSticker(kShBox, sx(d.x + float(i) * 0.7f * s), sy(d.y - (1.6f + 0.4f * float(i * i)) * s), 0.16f * s * k,
                                            (1.6f + 0.4f * float(i * i)) * s * k, i == 0 ? light : dark);
                    r.rot = float(i) * 0.18f;
                    r.lift = 0.9f * s * k;
                    addSticker(r);
                }
                return;
            }
            Sticker stem = makeSticker(kShBox, sx(d.x), sy(d.y - 1.0f * s), 0.55f * s * k, 1.0f * s * k, 0xF4EEDC);
            setColor(stem.c1, 0xD8CCB0);
            stem.fold = 1;
            stem.lift = 0.6f * s * k;
            addSticker(stem);
            Sticker cap = makeSticker(kShEllipse, sx(d.x), sy(d.y - 2.3f * s), 2.2f * s * k, 1.2f * s * k, light);
            setColor(cap.c1, dark);
            cap.fold = 1;
            cap.lift = 1.4f * s * k;
            addSticker(cap);
            return;
        }
        case 8: {  // ember trees: a flame of two tones
            addSticker(trunk);
            Sticker f = makeSticker(kShTri, sx(d.x), sy(d.y - 3.4f * s), 2.0f * s * k, 2.9f * s * k, light);
            setColor(f.c1, dark);
            f.fold = 1;
            f.lift = 1.8f * s * k;
            addSticker(f);
            return;
        }
        default: {  // pines, two folded cones (snow pines: a white tip)
            addSticker(trunk);
            for (int i = 0; i < 2; ++i) {
                const float hw = (2.0f + 0.8f * float(i)) * s, top = (7.8f - 2.6f * float(i)) * s, base = (3.4f - 2.4f * float(i)) * s;
                Sticker c = makeSticker(kShTri, sx(d.x), sy(d.y - (top + base) * 0.5f), hw * k, (top - base) * 0.5f * k, light);
                setColor(c.c1, dark);
                c.fold = 1;
                c.lift = 2.4f * s * k;
                addSticker(c);
            }
            if (wp.trees == 2) {
                Sticker tip = makeSticker(kShTri, sx(d.x), sy(d.y - 6.9f * s), 0.75f * s * k, 0.9f * s * k, 0xFFFFFF);
                setColor(tip.c1, 0xDDE8F2);
                tip.fold = 1;
                addSticker(tip);
            }
            return;
        }
    }
}

void Scene::popup(int i, float rise) {
    const sim::LevelDef& d = *def_;
    const sim::Site& t = d.sites[i];
    const WorldPalette& wp = worldPalette(d.world);
    const float x = float(t.x), y = float(t.y);
    const bool mine = (shownMine_ & bit(i)) != 0;
    const bool razed = (disp_.razed & bit(i)) != 0;
    const float fl = flipT_[i];
    const bool flipped = mine || fl >= 0.5f;
    const u32 tentLight = flipped ? col::kScarlet : (razed ? 0x1C1E3C : col::kInkLight);
    const u32 tentDark = flipped ? col::kScarletDark : (razed ? 0x101124 : col::kInk);
    const u32 flagC = flipped ? col::kGold : wp.accent;
    (void)rise;
    switch (t.kind) {
        case sim::kHome:
            tent(x - 6.2f, y - 1.6f, 1.05f, col::kScarlet, col::kScarletDark, col::kGold, rise);
            tent(x + 6.2f, y - 1.6f, 1.05f, col::kScarlet, col::kScarletDark, col::kGold, rise);
            break;
        case sim::kCamp:
        case sim::kRivalCamp: {
            const bool growing = t.perTurn != 0;
            tent(x - 4.5f, y - 3.0f, growing ? 1.12f : 1.0f, tentLight, tentDark, t.kind == sim::kRivalCamp ? wp.accentDark : flagC, rise);
            tent(x + 4.3f, y - 1.2f, 0.85f, tentLight, tentDark, flagC, rise);
            if (flipped) flagAt(x + 0.4f, y - 0.6f, 9.5f, 4.6f, col::kScarlet, col::kGold, rise, x);
            break;
        }
        case sim::kVillage: {
            const u32 roof1 = flipped ? col::kScarlet : wp.accent, roof1d = flipped ? col::kScarletDark : wp.accentDark;
            house(x - 3.3f, y - 2.6f, 1.0f, col::kIvory, col::kIvoryShade, roof1, roof1d, rise);
            house(x + 3.4f, y - 0.6f, 0.85f, col::kIvory, col::kIvoryShade, col::kGold, col::kGoldDark, rise);
            if (flipped) flagAt(x + 0.2f, y - 3.8f, 9.0f, 4.4f, col::kScarlet, col::kGold, rise, x);
            break;
        }
        case sim::kShrine: {
            const bool spent = flipped;
            const bool faded = t.until >= 0 && disp_.turn > t.until;
            const u32 light = spent || faded ? 0xBDB4CC : col::kVioletLight, dark = spent || faded ? 0x958AA8 : col::kViolet;
            Sticker base = makeSticker(kShBox, sx(x), sy(y - 1.0f), 4.6f * fit_.k, 1.0f * fit_.k, spent ? 0xC8B89A : col::kGold);
            base.p0 = 0.3f;
            base.lift = 0.5f * fit_.k;
            addSticker(base);
            Sticker ob = makeSticker(kShTri, sx(x), sy(y - 8.6f), 2.9f * fit_.k, 7.2f * fit_.k, light);
            setColor(ob.c1, dark);
            ob.fold = 1;
            ob.lift = 4.0f * fit_.k;
            addSticker(ob);
            if (!spent) {
                Sticker st = makeSticker(kShStar, sx(x), sy(y - 9.6f), 1.8f * fit_.k, 1.8f * fit_.k, col::kGold);
                st.rot = std::sin(time_ * 2.0f) * 0.12f;
                addSticker(st);
            }
            break;
        }
        case sim::kPost: {
            const u32 tl = flipped ? col::kScarlet : col::kInkLight, td = flipped ? col::kScarletDark : col::kInk;
            const u32 rf = flipped ? col::kGold : wp.accent, rfd = flipped ? col::kGoldDark : wp.accentDark;
            if (river_ && std::fabs(y - riverY_) < 3.0f) {  // a bridge across the river, a small tower each side
                Sticker deck = makeSticker(kShBox, sx(x), sy(y), 4.4f * fit_.k, 7.4f * fit_.k, col::kWoodLight);
                deck.p0 = 0.15f;
                deck.lift = 0.6f * fit_.k;
                addSticker(deck);
                for (int p = -3; p <= 3; ++p) {
                    Sticker plank = makeSticker(kShBox, sx(x), sy(y + float(p) * 2.1f), 4.2f * fit_.k, 0.14f * fit_.k, col::kWoodDark);
                    addSticker(plank);
                }
                tower(x - 6.0f, y + 2.6f, 3.4f, 6.2f, tl, td, rf, rfd, false, rise);
                tower(x + 6.0f, y + 2.6f, 3.4f, 6.2f, tl, td, rf, rfd, false, rise);
            } else {  // a gatehouse: two towers and an arch
                tower(x - 4.2f, y + 0.5f, 3.6f, 7.4f, tl, td, rf, rfd, true, rise);
                tower(x + 4.2f, y + 0.5f, 3.6f, 7.4f, tl, td, rf, rfd, true, rise);
                Sticker wall = makeSticker(kShBox, sx(x), sy(y - 2.6f), 2.6f * fit_.k, 2.4f * fit_.k, tl);
                setColor(wall.c1, td);
                wall.fold = 1;
                wall.lift = 1.6f * fit_.k;
                addSticker(wall);
                Sticker arch = makeSticker(kShArch, sx(x), sy(y - 1.4f), 1.3f * fit_.k, 1.5f * fit_.k, col::kInkDeep);
                addSticker(arch);
            }
            break;
        }
        case sim::kMire: {
            Sticker pool = makeSticker(kShEllipse, sx(x), sy(y + 0.4f), 5.6f * fit_.k, 2.8f * fit_.k, flipped ? 0x8A9A6A : 0x4E6A3A);
            addSticker(pool);
            Sticker sheen = makeSticker(kShEllipse, sx(x - 1.0f), sy(y), 2.4f * fit_.k, 0.8f * fit_.k, 0x7E9A5A);
            addSticker(sheen);
            for (int r = 0; r < 5; ++r) {
                const float rx = x - 5.0f + float(r) * 2.4f;
                Sticker reed = makeSticker(kShBox, sx(rx), sy(y - 2.0f - float(r % 2)), 0.18f * fit_.k, (2.0f + float(r % 2)) * fit_.k,
                                           r % 2 ? col::kMoss : col::kMossLight);
                reed.rot = (float(r) - 2.0f) * 0.1f;
                reed.lift = 0.8f * fit_.k;
                addSticker(reed);
            }
            Sticker sign = makeSticker(kShBox, sx(x + 3.2f), sy(y - 2.0f), 0.2f * fit_.k, 2.0f * fit_.k, col::kWood);
            sign.rot = 0.35f;
            sign.lift = 0.8f * fit_.k;
            addSticker(sign);
            Sticker board = makeSticker(kShBox, sx(x + 3.7f), sy(y - 3.6f), 1.4f * fit_.k, 0.5f * fit_.k, col::kWoodLight);
            board.rot = 0.35f;
            board.lift = 1.0f * fit_.k;
            addSticker(board);
            break;
        }
        case sim::kDepot: {
            for (int p = -3; p <= 3; ++p) {  // a stockade
                Sticker stake = makeSticker(kShBox, sx(x + float(p) * 1.3f), sy(y - 3.4f + float(p * p) * 0.12f), 0.55f * fit_.k,
                                            1.8f * fit_.k, flipped ? col::kScarlet : col::kWood);
                setColor(stake.c1, flipped ? col::kScarletDark : col::kWoodDark);
                stake.fold = 1;
                stake.p0 = 0.4f;
                stake.lift = 1.2f * fit_.k;
                addSticker(stake);
            }
            for (int c = 0; c < 3; ++c) {
                Sticker crate = makeSticker(kShBox, sx(x - 2.6f + float(c) * 2.4f), sy(y - 0.4f - float(c == 1) * 1.6f), 1.0f * fit_.k,
                                            1.0f * fit_.k, col::kWoodLight);
                setColor(crate.c1, col::kWoodDark);
                crate.fold = 1;
                crate.lift = 0.7f * fit_.k;
                addSticker(crate);
            }
            break;
        }
        case sim::kKey: {
            tower(x, y, 3.4f, 10.0f, tentLight, tentDark, flipped ? col::kGold : wp.accent, flipped ? col::kGoldDark : wp.accentDark,
                  false, rise);
            Sticker key = makeSticker(kShRing, sx(x), sy(y - 6.0f), 0.9f * fit_.k, 0.9f * fit_.k, col::kGold);
            key.p0 = 0.5f * fit_.k;
            addSticker(key);
            break;
        }
        case sim::kKeep: {
            const float s = 0.82f;
            const float fold = keepFold_ >= 0 ? keepFold_ : 0.0f;
            // The win: the Keep folds flat and stands again in the host's colours.
            const bool host = fold >= 0.5f;
            const float stand = keepFold_ >= 0 ? std::fabs(1.0f - 2.0f * fold) : 1.0f;
            const float saved = curRise_;
            curRise_ *= maxv(0.05f, stand);
            const u32 wl = host ? col::kIvory : col::kInkLight, wd = host ? col::kIvoryShade : col::kInk;
            const u32 rf = host ? col::kScarlet : wp.accent, rfd = host ? col::kScarletDark : wp.accentDark;
            const float boss = d.boss ? 1.15f : 1.0f;
            tower(x, y - 1.0f * s, 32.0f * s * boss, 11.6f * s, wl, wd, 0, 0, true, rise);
            tower(x - 14.6f * s * boss, y + 1.0f * s, 7.4f * s, 18.4f * s * boss, wl, wd, rf, rfd, false, rise);
            tower(x + 14.6f * s * boss, y + 1.0f * s, 7.4f * s, 18.4f * s * boss, wl, wd, rf, rfd, false, rise);
            if (d.boss) {
                tower(x - 7.5f * s, y - 0.5f * s, 5.4f * s, 15.0f * s, wl, wd, rf, rfd, false, rise);
                tower(x + 7.5f * s, y - 0.5f * s, 5.4f * s, 15.0f * s, wl, wd, rf, rfd, false, rise);
            }
            tower(x, y + 2.2f * s, 12.0f * s, 22.8f * s * boss, wl, wd, rf, rfd, false, rise);
            Sticker gate = makeSticker(kShArch, sx(x), sy(y - 2.4f * s), 2.8f * s * fit_.k, 3.6f * s * fit_.k, col::kInkDeep);
            addSticker(gate);
            for (int w = -1; w <= 1; w += 2) {
                Sticker win = makeSticker(kShBox, sx(x + float(w) * 14.6f * s * boss), sy(y - 11.0f * s * boss), 0.85f * s * fit_.k,
                                          1.6f * s * fit_.k, col::kGold);
                win.p0 = 0.9f;
                addSticker(win);
            }
            Sticker win = makeSticker(kShBox, sx(x), sy(y - 15.0f * s * boss), 1.05f * s * fit_.k, 2.0f * s * fit_.k, col::kGold);
            win.p0 = 0.9f;
            addSticker(win);
            flagAt(x, y - (22.8f * boss + 9.4f) * s, 7.0f, 5.6f, host ? col::kScarlet : wp.accent, col::kGold, rise, 0.7f);
            curRise_ = saved;
            break;
        }
        default: break;
    }
}

// ---- Figures ---------------------------------------------------------------------------------------------------------

void Scene::drawFigure(const Figure& f) {
    const WorldPalette& wp = worldPalette(def_->world);
    const float k = fit_.k;
    u32 body = col::kIvory, shade = col::kIvoryShade, scarf = col::kScarlet;
    if (f.team == 1) {
        const float fl = f.site >= 0 ? flipT_[f.site] : 0.0f;
        if (fl < 0.5f) {
            body = col::kInkLight;
            shade = col::kInk;
            scarf = wp.accent;
        }
    } else if (f.team == 2) {
        body = 0x2A2C48;
        shade = 0x16172C;
        scarf = wp.accent;
    }
    if (f.bearer) scarf = col::kGold;
    const float hop = std::fabs(std::sin(f.hop)) * 0.55f;
    const float s = f.bearer ? 1.25f : 1.0f;
    const float x = f.x, y = f.y - hop;
    const float wscale = maxv(0.08f, f.flip);
    if (f.kind == 1) {  // a rider
        Sticker h = makeSticker(kShHorse, sx(x), sy(y - 1.45f), 2.0f * k * wscale, 1.5f * k, f.team == 1 || f.team == 2 ? shade : 0xF2E6CE,
                                f.alpha);
        setColor(h.c1, f.team == 1 || f.team == 2 ? col::kInkDeep : col::kWood, f.alpha);
        setColor(h.c2, scarf, f.alpha);
        h.lift = 0.8f * k;
        addSticker(h);
    }
    const float scale = f.kind == 2 ? 2.4f : 1.0f;
    const float py = f.kind == 1 ? y - 2.2f : y;
    Sticker p = makeSticker(kShPawn, sx(x - (f.kind == 1 ? 0.2f : 0.0f)), sy(py - 1.9f * s * scale), 0.9f * s * scale * k * wscale,
                            1.9f * s * scale * k, body, f.alpha);
    setColor(p.c1, shade, f.alpha);
    setColor(p.c2, scarf, f.alpha);
    p.lift = 0.85f * s * scale * k;
    addSticker(p);
    if (f.kind == 2) {  // a giant carries a small banner
        Sticker b = makeSticker(kShFlag, sx(x + 2.6f), sy(py - 8.6f), 1.4f * k, 0.6f * k, scarf, f.alpha);
        b.p0 = time_ * 4.0f;
        setColor(b.c2, body, f.alpha);
        addSticker(b);
    }
    if (f.bearer) {  // the banner over the host
        const float h = 13.0f;
        Sticker pole = makeSticker(kShBox, sx(x + 1.3f), sy(y - h * 0.5f - 1.0f), 0.2f * k, h * 0.5f * k, col::kWood);
        pole.lift = 3.0f * k;
        addSticker(pole);
        Sticker fl = makeSticker(kShFlag, sx(x + 1.3f + 4.2f), sy(y - h + 1.2f), 4.2f * k, 1.9f * k, col::kScarlet);
        setColor(fl.c2, col::kGold);
        fl.p0 = time_ * 5.0f;
        fl.lift = 3.4f * k;
        addSticker(fl);
    }
}

// ---- Tags -----------------------------------------------------------------------------------------------------------

void Scene::tag(UiBatch& b, float x, float y, const char* text, u32 bg, u32 fg, u32 edge, float scale, bool lock, const char* badge,
                u32 badgeColor, float alpha) {
    const float size = L_.px(21.0f) * scale * tune.tagScale;
    const float tw = UiBatch::textWidth(text, size);
    const float lockW = lock ? size * 0.78f : 0.0f;
    const float h = size * 1.36f;
    const float w = maxv(h * 1.35f, tw + size * 0.95f + lockW);
    const float cy = y - h * 0.5f - size * 0.36f;
    const u8 a = u8(255 * clampv(alpha, 0.0f, 1.0f));
    b.rect(x + size * 0.06f, cy + size * 0.16f, w, h, h * 0.36f, rgba(col::kShadow, u8(70 * alpha)), {0, 0, 0, 0}, 0, size * 0.16f);
    b.rectRotated(x, y - size * 0.42f, size * 0.42f, size * 0.42f, size * 0.06f, 0.7854f, rgba(edge ? edge : bg, a));
    b.rect(x, cy, w, h, h * 0.36f, rgba(bg, a), rgba(edge, edge ? a : 0), edge ? size * 0.09f : 0);
    b.text(text, x + lockW * 0.5f, cy, size, rgba(fg, a), {0, 0, 0, 0}, 0, Align::Center);
    if (lock) {
        const float lx = x - w * 0.5f + size * 0.62f, ly = cy + size * 0.04f, s = size;
        b.rect(lx, ly - s * 0.17f, s * 0.36f, s * 0.42f, s * 0.18f, {0, 0, 0, 0}, rgba(fg, a), s * 0.08f);
        b.rect(lx, ly + s * 0.1f, s * 0.5f, s * 0.38f, s * 0.07f, rgba(fg, a));
    }
    if (badge && badge[0]) {
        const float bs = size * 0.6f;
        const float bw = UiBatch::textWidth(badge, bs) + bs * 0.8f, bh = bs * 1.35f;
        const float bx = x + w * 0.5f - bw * 0.2f, by = cy - h * 0.5f;
        b.rect(bx, by, bw, bh, bh * 0.5f, rgba(badgeColor, a), rgba(col::kWhite, a), size * 0.05f);
        b.text(badge, bx, by, bs, rgba(col::kWhite, a), {0, 0, 0, 0}, 0, Align::Center);
    }
}

void Scene::drawTags(UiBatch& b) {
    const sim::LevelDef& d = *def_;
    const WorldPalette& wp = worldPalette(d.world);
    const u32 take = winDone_ || winT_ >= 0 || loseT_ >= 0 ? 0u : sim::takeable(d, disp_);
    const u32 reach = sim::reachable(d, disp_);
    char buf[32], badge[16];
    for (int i = 1; i < d.siteCount; ++i) {
        const sim::Site& t = d.sites[i];
        if (!sim::isFree(disp_, i) || (shownMine_ & bit(i)) || flipT_[i] > 0.05f) continue;
        const float rise = riseAt(float(t.y), t.kind == sim::kKeep);
        if (rise < 0.6f) continue;
        const bool can = (take & bit(i)) != 0;
        const bool near = (reach & bit(i)) != 0;
        const i64 v = sim::number(d, disp_, i);
        float scale = near ? 1.0f : 0.78f;
        float lift = 12.5f;  // map units above the site
        u32 bg = col::kInk, fg = col::kIvory, edge = 0;
        bool lock = false;
        badge[0] = 0;
        switch (t.kind) {
            case sim::kCamp: case sim::kDepot: case sim::kKey: case sim::kRivalCamp:
                formatThousands(v, buf, sizeof buf);
                if (can) {
                    bg = col::kGold;
                    fg = col::kText;
                    edge = col::kWhite;
                } else {
                    lock = true;
                }
                if (t.kind == sim::kDepot) std::snprintf(badge, sizeof badge, "-%d", t.aux);
                break;
            case sim::kKeep: {
                formatThousands(v, buf, sizeof buf);
                bg = can ? col::kGold : col::kInk;
                fg = can ? col::kText : col::kGold;
                edge = col::kGold;
                lock = !can;
                scale = 1.32f;
                lift = 33.0f * (d.boss ? 1.15f : 1.0f);
                break;
            }
            case sim::kVillage:
                std::snprintf(buf, sizeof buf, "+%lld", (long long)v);
                bg = col::kIvory;
                fg = 0x178236;
                edge = can ? col::kGold : 0;
                lift = 9.0f;
                break;
            case sim::kShrine:
                std::snprintf(buf, sizeof buf, "\xC3\x97%lld", (long long)v);
                bg = col::kViolet;
                fg = col::kWhite;
                edge = can ? col::kGold : col::kWhite;
                lift = 15.0f;
                if (t.until >= 0 && disp_.turn <= t.until) std::snprintf(badge, sizeof badge, "%d", t.until - disp_.turn + 1);
                break;
            case sim::kPost:
                std::snprintf(buf, sizeof buf, "-%lld", (long long)v);
                bg = col::kIvory;
                fg = 0xD24A0A;
                edge = can ? col::kGold : 0;
                lift = 11.0f;
                break;
            case sim::kMire:
                std::snprintf(buf, sizeof buf, "\xC3\xB7" "2");
                bg = col::kMoss;
                fg = col::kWhite;
                edge = can ? col::kGold : 0;
                lift = 7.5f;
                break;
            default: continue;
        }
        if (t.perTurn > 0 && (t.hi == 0 || v < t.hi)) std::snprintf(badge, sizeof badge, "+%d", t.perTurn);
        if (t.perTurn < 0 && v > t.lo) std::snprintf(badge, sizeof badge, "%d", t.perTurn);
        float x = sx(float(t.x)), y = sy(float(t.y) - lift);
        if (t.kind == sim::kKeep && y < fit_.screenTop - L_.px(8)) {  // no room above: beside the castle
            x = sx(float(t.x) + 21.0f);
            y = sy(float(t.y) - 12.0f);
        }
        // A refused tap shakes the tag; a takeable one bobs.
        if (shake_[i] > 0) x += std::sin(shake_[i] * 80.0f) * L_.px(6) * (shake_[i] / 0.24f);
        if (can) y -= std::fabs(std::sin(time_ * 3.2f + float(i))) * L_.px(3);
        const float popScale = 1.0f + pop_[i] * 0.15f;
        tag(b, x, y, buf, bg, fg, edge, scale * popScale, lock, badge[0] ? badge : nullptr, t.perTurn < 0 ? col::kInk : wp.accent,
            near ? 1.0f : 0.88f);
        if (whyT_[i] > 0) {
            const float a = minv(1.0f, whyT_[i] / 0.3f);
            const float ws = L_.px(15);
            const float ww = UiBatch::textWidth(why_[i], ws) + L_.px(16);
            b.rect(x, y + L_.px(14), ww, L_.px(26), L_.px(13), rgba(col::kText, u8(220 * a)));
            b.text(why_[i], x, y + L_.px(14), ws, rgba(col::kIvory, u8(255 * a)), {0, 0, 0, 0}, 0, Align::Center);
        }
    }
    // The rival host's number, over its banner.
    if (disp_.rivalAlive() && rivalShown_ > 0) {
        formatThousands(i64(rivalShown_ + 0.5f), buf, sizeof buf);
        const bool can = (take & bit(sim::kRivalMove)) != 0;
        tag(b, sx(rivalX_), sy(rivalY_ - 13.0f), buf, can ? col::kGold : 0x1C1E3C, can ? col::kText : wp.accent, wp.accent, 1.1f, !can,
            nullptr, 0, 1.0f);
    }
    // Raids on their way: the number and the turns left.
    for (int r = disp_.raidNext; r < d.raidCount; ++r) {
        const sim::Raid& rd = d.raids[r];
        const int left = int(rd.turn) - int(disp_.turn);
        const sim::Site& e = d.sites[rd.entry];
        const sim::Site& h = d.sites[0];
        const float p = clampv(1.0f - float(left) / 6.0f, 0.0f, 0.9f);
        const float ex = e.x < 50 ? 4.0f : 96.0f;
        const float rx = ex + (float(h.x) - ex) * p, ry = float(e.y) + (float(h.y) - float(e.y)) * p;
        formatThousands(rd.strength, buf, sizeof buf);
        std::snprintf(badge, sizeof badge, "IN %d", maxv(1, left));
        tag(b, sx(rx), sy(ry - 8.0f), buf, 0x1C1E3C, wp.accent, wp.accent, 0.95f, false, badge, col::kScarlet, 1.0f);
    }
    // The host's own number over its banner.
    char hb[32];
    formatHost(i64(shownS_ + 0.5f), hb, sizeof hb);
    if (loseT_ < 0) tag(b, sx(bannerX_ + 1.3f), sy(bannerY_ - 15.6f), hb, col::kIvory, col::kScarlet, col::kScarlet, 1.18f, false, nullptr, 0, 1.0f);
}

// ---- The frame -----------------------------------------------------------------------------------------------------

void Scene::build(PaperFrame& f, UiBatch& labels) {
    const sim::LevelDef& d = *def_;
    const WorldPalette& wp = worldPalette(d.world);
    tmpN_ = 0;
    groupN_ = 0;
    // Gold dots along the roads from the host's land to each site it can take now.
    const u32 take = (winT_ >= 0 || loseT_ >= 0 || clipCount_ > 0) ? 0u : sim::takeable(d, disp_);
    beginGroup(-1e6f);
    curRise_ = 1;
    curBurn_ = 0;
    for (int r = 0; r < d.roadCount; ++r) {
        const int a = d.roads[r].a, bb = d.roads[r].b;
        const bool frontier = ((shownMine_ & bit(a)) && (take & bit(bb))) || ((shownMine_ & bit(bb)) && (take & bit(a)));
        if (!frontier) continue;
        const Road& R = roads_[r];
        for (int i = 3; i < R.n - 3; i += 3) {
            const float pulse = 0.8f + 0.2f * std::sin(time_ * 5.0f - float(i) * 0.6f);
            Sticker dot = makeSticker(kShEllipse, sx(R.px[i]), sy(R.py[i]), 0.6f * fit_.k * pulse, 0.6f * fit_.k * pulse, col::kGold);
            addSticker(dot);
        }
    }
    // Locked roads: a paper gate across the road at its middle, in the key tower's colour.
    for (int r = 0; r < d.roadCount; ++r) {
        if (d.roads[r].lock == sim::kNoLock || sim::roadOpen(d, disp_, r)) continue;
        const Road& R = roads_[r];
        const int m = R.n / 2;
        const float ang = std::atan2(R.py[m + 1] - R.py[m - 1], R.px[m + 1] - R.px[m - 1]) + 1.5708f;
        beginGroup(R.py[m]);
        curRise_ = riseAt(R.py[m], false);
        curBase_ = sy(R.py[m]);
        Sticker g = makeSticker(kShBox, sx(R.px[m]), sy(R.py[m] - 1.2f), 2.6f * fit_.k, 1.2f * fit_.k, col::kGold);
        setColor(g.c1, col::kGoldDark);
        g.fold = 2;
        g.rot = ang - 1.5708f;
        g.lift = 0.8f * fit_.k;
        addSticker(g);
    }
    for (int i = 0; i < decorCount_; ++i) {
        const Decor& o = decor_[i];
        if (o.y < scroll_ - 10.0f || o.y > scroll_ + fit_.viewH + 4.0f) continue;
        beginGroup(o.y);
        curRise_ = riseAt(o.y, false);
        curBase_ = sy(o.y);
        curBurn_ = 0;
        treeAt(o, curRise_);
    }
    for (int i = 0; i < d.siteCount; ++i) {
        const float y = float(d.sites[i].y);
        if (y < scroll_ - 30.0f || y > scroll_ + fit_.viewH + 12.0f) continue;
        beginGroup(y - (d.sites[i].kind == sim::kKeep ? 2.0f : 0.0f));
        curRise_ = riseAt(y, d.sites[i].kind == sim::kKeep) * (1.0f - 0.85f * burnT_[i]);
        curBase_ = sy(y);
        curBurn_ = burnT_[i];
        popup(i, curRise_);
    }
    curBurn_ = 0;
    for (int i = 0; i < figCount_; ++i) {
        const Figure& g = figs_[i];
        if (g.alpha <= 0.01f) continue;
        beginGroup(g.y + 0.01f * float(i % 7));
        curRise_ = g.team == 0 || g.team == 3 ? 1.0f : riseAt(g.y, false);
        curBase_ = sy(g.y);
        drawFigure(g);
    }
    // The rival's banner.
    if (disp_.rivalAlive() && rivalShown_ > 0) {
        beginGroup(rivalY_ + 0.5f);
        curRise_ = riseAt(rivalY_, false);
        curBase_ = sy(rivalY_);
        flagAt(rivalX_ + 1.0f, rivalY_, 12.0f, 5.0f, wp.accentDark, 0x1C1E3C, 1.0f, 1.3f);
    }
    if (groupN_ > 0) groups_[groupN_ - 1].count = tmpN_ - groups_[groupN_ - 1].first;
    // std::sort allocates nothing (std::stable_sort may): ties keep their order by `first`.
    std::sort(groups_, groups_ + groupN_,
              [](const Group& a, const Group& b) { return a.key < b.key || (a.key == b.key && a.first < b.first); });
    stickerN_ = 0;
    for (int g = 0; g < groupN_; ++g) {
        const Group& G = groups_[g];
        for (int i = 0; i < G.count && stickerN_ < kMaxStickers; ++i) stickers_[stickerN_++] = tmp_[G.first + i];
    }
    // Over everything but the tags: confetti and the hint's ring.
    overN_ = 0;
    for (int i = 0; i < confettiCount_ && overN_ < kMaxConfetti; ++i) {
        const Confetti& c = conf_[i];
        const float a = clampv((c.life - c.age) / 0.4f, 0.0f, 1.0f);
        const float turn = std::cos(c.rot * 1.7f);
        Sticker s = makeSticker(kShBox, sx(c.x), sy(c.y), 0.85f * fit_.k * maxv(0.15f, std::fabs(turn)), 0.38f * fit_.k, turn > 0 ? c.c0 : c.c1, a);
        s.rot = c.rot;
        s.p0 = 0.25f;
        over_[overN_++] = s;
    }
    if (hintSite_ >= 0 && hintSite_ < d.siteCount && overN_ < kMaxConfetti + 16) {
        const float pulse = 1.0f + 0.12f * std::sin(hintTime_ * 7.0f);
        Sticker r = makeSticker(kShRing, sx(float(d.sites[hintSite_].x)), sy(float(d.sites[hintSite_].y) - 3.0f), 8.0f * fit_.k * pulse,
                                8.0f * fit_.k * pulse, col::kGold, 0.95f);
        r.p0 = 0.9f * fit_.k;
        over_[overN_++] = r;
    }
    labels.clear();
    drawTags(labels);

    f.tableA = wp.tableA;
    f.tableB = wp.tableB;
    f.tableLine = wp.tableLine;
    f.pageX = fit_.x;
    f.pageY = fit_.y;
    f.pageW = 100.0f * fit_.k;
    f.pageH = fit_.viewH * fit_.k;
    const float H = float(d.pageH);
    f.pageV0 = 1.0f - scroll_ / H;
    f.pageV1 = 1.0f - (scroll_ + fit_.viewH) / H;
    f.grain = tune.grain;
    f.stickers = stickers_;
    f.stickerCount = stickerN_;
    f.shadowStrength = tune.shadowStrength;
    f.over = over_;
    f.overCount = overN_;
    f.labels = &labels;
    f.time = time_;
}

}  // namespace pm
