// The screens and the flow test. Simple on purpose (the owner, 2026-11-26): a level, its result, the next level; PAUSE,
// SETTINGS and LEVELS are one tap away and never in the way.
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "build_stamp.h"
#include "core/log.h"
#include "core/tuning.h"
#include "game/game.h"
#include "platform/platform.h"

namespace pm {

namespace {

constexpr u32 kSquadBlue = 0x2E7BF0, kHordeRed = 0xE0303A;

void pauseGlyph(UiBatch& b, const Rect& r, const Layout& L, bool pressed) {
    const Rgba c = rgba(pressed ? col::kWhite : ui::kInk);
    const float y = r.cy() + (pressed ? L.px(4) : 0.0f);
    b.rect(r.cx() - L.px(5.5f), y, L.px(6), L.px(18), L.px(2.5f), c);
    b.rect(r.cx() + L.px(5.5f), y, L.px(6), L.px(18), L.px(2.5f), c);
}

Rect pauseRect(const Layout& L) {
    const float s = L.px(48);
    return {float(L.w) - L.safe.right - L.px(12) - s, L.top() + L.px(10), s, s};
}

const char* bossTitle(int world) {
    static const char* const names[kWorldCount] = {"OGRE CHIEF", "SAND OGRE", "FROST OGRE", "EMBER OGRE", "SALT OGRE",
                                                   "THORN OGRE", "BOG OGRE",   "CLOUD OGRE", "ASH OGRE",   "LANTERN OGRE"};
    return names[(maxv(world, 1) - 1) % kWorldCount];
}

}  // namespace

void Game::findUiArt() {
    static const char* const names[] = {
        "ui.star_full", "ui.star_empty", "ui.shako", "ui.boss_head", "ui.skull", "ui.hand", "ui.trophy", "ui.logo", "ui.rays",
        "ui.ribbon_l", "ui.ribbon_m", "ui.ribbon_r", "ui.ribbon_red_l", "ui.ribbon_red_m", "ui.ribbon_red_r", "ui.ribbon_gold_l",
        "ui.ribbon_gold_m", "ui.ribbon_gold_r", "ui.wpn_musket", "ui.wpn_twin", "ui.wpn_scatter", "ui.wpn_lance", "ui.wpn_rocket",
        "ui.wpn_flame", "ui.pow_fieldgun" /* the cannon */, "ui.wpn_coil", "ui.wpn_mortar", "ui.wpn_frost",
        "ui.pow_none", "ui.pow_rapid", "ui.pow_power", "ui.pow_shield", "ui.pow_freeze", "ui.pow_airstrike", "ui.pow_magnet",
        "ui.pow_giants", "ui.pow_reinforce", "ui.pow_quick", "ui.pow_hourglass", "ui.pow_double", "ui.pow_fieldgun",
        "ui.mode_march", "ui.mode_boss", "ui.mode_gold", "ui.mode_rescue", "ui.mode_arsenal"};
    static_assert(sizeof names / sizeof names[0] == kUiArtCount, "a name for every UI sprite");
    for (int i = 0; i < kUiArtCount; ++i) uiArt_[i] = art_.ready() ? art_.findKind(names[i]) : -1;
}

bool Game::uiArt(int which, float x, float y, float hDp, Rgba tint, float angle, float hue) {
    const int a = which >= 0 && which < kUiArtCount ? uiArt_[which] : -1;
    if (a < 0 || !art_.ready()) return false;
    const SpriteFrame& f = art_.frameAt(a, 0, 0);
    const float h = layout_.px(hDp > 0 ? hDp : f.height), w = h * f.w / f.h;
    const float uv[5] = {f.u0, f.v0, f.u1, f.v1, f.layer};
    uiBatch_.sprite(uv, x, y, w, h, tint, angle, hue);
    return true;
}

// A title on a ribbon: its middle stretched under the text, its caps at the ends (each part placed by its joint, at
// the band's mid height: ART_DIRECTION 15.1); a pill when there is no art.
void Game::ribbon(const char* text, float cx, float cy, int color, float textDp) {
    const Layout& L = layout_;
    const float size = L.px(textDp);
    const float midW = UiBatch::textWidth(text, size) + L.px(24);
    const int base = color == 1 ? kUiRedL : (color == 2 ? kUiGoldL : kUiRibbonL);
    const int parts[3] = {uiArt_[base], uiArt_[base + 1], uiArt_[base + 2]};
    if (art_.ready() && parts[0] >= 0 && parts[1] >= 0 && parts[2] >= 0) {
        const Rgba white{255, 255, 255, u8(255 * ui_.opacity)};
        const SpriteFrame& m = art_.frameAt(parts[1], 0, 0);
        const float km = L.px(m.height) / m.h;
        const float uvm[5] = {m.u0, m.v0, m.u1, m.v1, m.layer};
        uiBatch_.sprite(uvm, cx, cy + (m.h * 0.5f - m.py) * km, midW, m.h * km, white);
        for (int side = 0; side < 2; ++side) {
            const SpriteFrame& c = art_.frameAt(parts[side ? 2 : 0], 0, 0);
            const float k = L.px(c.height) / c.h;
            const float jx = side ? cx + midW * 0.5f : cx - midW * 0.5f;
            const float uv[5] = {c.u0, c.v0, c.u1, c.v1, c.layer};
            uiBatch_.sprite(uv, jx + (c.w * 0.5f - c.px) * k, cy + (c.h * 0.5f - c.py) * k, c.w * k, c.h * k, white);
        }
    } else {
        const u32 fill = color == 1 ? ui::kRed : (color == 2 ? ui::kGold : ui::kCobalt);
        uiBatch_.rect(cx, cy, midW + L.px(24), L.px(40), L.px(12), rgba(fill, u8(255 * ui_.opacity)), rgba(ui::kInk, u8(255 * ui_.opacity)),
                      L.px(2.5f));
    }
    uiBatch_.text(text, cx, cy, size, rgba(col::kWhite, u8(255 * ui_.opacity)), rgba(ui::kInk, u8(255 * ui_.opacity)), L.px(2),
                  Align::Center);
}

float Game::bossHue() const {
    if (!art_.ready() || !def_) return 0;
    float h = std::fmod(art_.world(def_->world).bossHue, 360.0f);
    if (h < 0) h += 360.0f;
    return h / 360.0f;
}

u32 Game::bossTint() const { return art_.ready() && def_ ? art_.world(def_->world).bossTint : 0xFFFFFF; }

void Game::drawUi(float dt) {
    uiDt_ = dt;
    if (mode_ == Mode::Picture && pictureScreen_[0] == 0 && !args_.lint) {
        drawHud();
        return;
    }
    switch (screen_) {
        case Screen::Play: drawHud(); break;
        case Screen::Pause: drawHud(); drawPause(); break;
        case Screen::Win: drawHud(); drawWin(); break;
        case Screen::Lose: drawHud(); drawLose(); break;
        case Screen::Settings: drawSettings(dt); break;
        case Screen::Levels: drawLevels(); break;
        case Screen::End: drawEnd(); break;
    }
    if (titleFade_ > 0 && (screen_ == Screen::Play || screen_ == Screen::Pause)) drawTitle();
}

// The title on a cold start (ART_DIRECTION 15.4): the logo on a slow golden sunburst, the tagline on a red pill.
void Game::drawTitle() {
    const Layout& L = layout_;
    const float a = titleFade_, W = float(L.w);
    const float cy = maxv(L.top() + L.px(250), float(L.h) * 0.32f);  // clear of the HUD and the hint line
    uiArt(kUiRays, W * 0.5f, cy, 320, {255, 224, 122, u8(178 * a)}, screenTime_ * 0.1745f);
    // The logo 288 dp wide (or the screen less 48), bobbing; the tagline 12 dp under it.
    float logoH = L.px(60);
    const float bob = L.px(3) * std::sin(screenTime_ * 3.14159f);
    if (uiArt_[kUiLogo] >= 0 && art_.ready()) {
        const SpriteFrame& f = art_.frameAt(uiArt_[kUiLogo], 0, 0);
        const float wDp = minv(288.0f, W / L.dp - 48.0f);
        logoH = L.px(wDp * f.h / f.w);
        uiArt(kUiLogo, W * 0.5f, cy + bob, wDp * f.h / f.w, {255, 255, 255, u8(255 * a)});
    } else {
        uiBatch_.text("PAPERMARCH", W * 0.5f, cy + bob, L.px(46), rgba(ui::kCobalt, u8(255 * a)), rgba(ui::kInk, u8(255 * a)), L.px(2), Align::Center);
    }
    const char* tag = "GROW YOUR SQUAD. CRUSH THE HORDE.";
    const float tw = UiBatch::textWidth(tag, L.px(14)) + L.px(28);
    const float tagY = cy + logoH * 0.5f + L.px(12 + 15);
    uiBatch_.rect(W * 0.5f, tagY, tw, L.px(30), L.px(15), rgba(ui::kRed, u8(255 * a)), rgba(ui::kInk, u8(255 * a)), L.px(2));
    uiBatch_.text(tag, W * 0.5f, tagY, L.px(14), rgba(col::kWhite, u8(255 * a)), {0, 0, 0, 0}, 0, Align::Center);
    const float blink = 0.7f + 0.3f * std::sin(screenTime_ * 4.0f);  // between 100% and 40%
    uiBatch_.text("TAP TO MARCH", W * 0.5f, L.bottom() - L.px(120), L.px(22), rgba(col::kWhite, u8(255 * a * blink)), rgba(ui::kInk, u8(255 * a * blink)),
                  L.px(2), Align::Center);
    ui_.opacity = a;
    drawUpdatePill(tagY + L.px(52));
    ui_.opacity = 1;
}

// The updater (docs/RELEASE_PIPELINE.md): a GitHub build asks once a launch; while the latest release is newer, a gold
// pill downloads it in the game, checks it and opens Android's installer (the download's progress on the pill).
void Game::drawUpdatePill(float cy) {
    const Layout& L = layout_;
    if (!updateAsked_ && PM_BUILD_NUMBER > 0) {
        updateAsked_ = true;
        platform::checkForUpdate();
    }
    const int latest = updateOverride_ > 0 ? updateOverride_ : platform::latestBuild();
    if (latest > PM_BUILD_NUMBER && (PM_BUILD_NUMBER > 0 || updateOverride_ > 0)) {
        char buf[48];
        const float progress = platform::updateProgress();
        if (progress >= 0) std::snprintf(buf, sizeof buf, "DOWNLOADING %d%%", int(progress * 100.0f + 0.5f));
        else std::snprintf(buf, sizeof buf, "UPDATE: BUILD %d", latest);
        const float w = UiBatch::textWidth(buf, L.px(19)) + L.px(48);
        if (ui_.button(buf, {float(L.w) * 0.5f - w * 0.5f, cy, w, L.px(50)}, Ui::kPrimary, 0.8f) && progress < 0) {
            feedback_.uiTap();
            platform::startUpdate();
        }
    }
}

void Game::drawHud() {
    const Layout& L = layout_;
    if (!def_) return;
    const bool playing = screen_ == Screen::Play;
    const bool hard = def_->spike != 0;
    const float W = float(L.w);
    char buf[64];
    // Top left: the level chip (it opens LEVELS): cream, or on a hard level red with a skull, pulsing for 3 s.
    std::snprintf(buf, sizeof buf, "LEVEL %d", level_);
    const float top = L.top() + L.px(10);
    // Its icon: the skull of a hard level, else the mode's chip (gold rush, rescue, arsenal...), if any.
    const int mode = def_->mode < sim::kModeCount ? def_->mode : 0;
    const int chipArt = hard ? kUiSkull : (mode > sim::kModeBoss ? kUiMode + mode : -1);
    const float icon = chipArt >= 0 && uiArt_[chipArt] >= 0 ? L.px(30) : 0.0f;
    const float chipW = UiBatch::textWidth(buf, L.px(18)) + L.px(32) + icon;
    const float chipX = L.safe.left + L.px(12);
    const float pulse = hard && levelTime_ < 3.0f ? 1.0f + 0.05f * maxv(0.0f, std::sin(levelTime_ * 6.2831853f)) : 1.0f;
    const Rect chip{chipX, top, chipW * pulse, L.px(48) * pulse};
    const bool chipDown = ui_.pressing(chip, buf);
    if (ui_.button(buf, chip, hard ? Ui::kDanger : Ui::kSecondary, 18.0f / 22.0f * pulse, icon * 0.5f) && playing) {
        feedback_.uiTap();
        levelsFirst_ = 0;
        setScreen(Screen::Levels);
        return;
    }
    if (icon > 0) uiArt(chipArt, chipX + L.px(22) * pulse, chip.cy() + (chipDown ? L.px(4) : 0.0f), 26.0f * pulse);
    const int got = level_ - 1 < int(sizeof save_.stars) ? save_.stars[level_ - 1] : 0;
    for (int k = 0; k < 3; ++k) {
        const float sx = chipX + chip.w + L.px(22) + float(k) * L.px(28), sy = top + L.px(24);
        if (!uiArt(k < got ? kUiStarFull : kUiStarEmpty, sx, sy, 26))
            uiBatch_.star5(sx, sy, L.px(10.5f), rgba(k < got ? ui::kGold : 0x5A4A40, 230), rgba(ui::kInk, 160), L.px(1));
    }
    const Rect pr = pauseRect(L);
    const bool pauseDown = ui_.pressing(pr, "#PAUSE");
    if (ui_.button("#PAUSE", pr, Ui::kRound) && playing) {
        feedback_.uiTap();
        setScreen(Screen::Pause);
        return;
    }
    pauseGlyph(uiBatch_, pr, L, pauseDown);
    // Left of it: the weapon the squad carries once it is not the musket, popping when just taken.
    const int wpn = battle_.weapon < sim::kWeaponCount ? battle_.weapon : 0;
    if (wpn != shownWeapon_) {
        if (shownWeapon_ >= 0) weaponPop_ = 1;
        shownWeapon_ = wpn;
    }
    weaponPop_ = maxv(0.0f, weaponPop_ - uiDt_ * 3.0f);
    if (wpn != sim::kMusket && uiArt_[kUiWeapon + wpn] >= 0) {
        const float s = 1.0f + 0.35f * weaponPop_ * weaponPop_;
        const float cx = pr.x - L.px(34), cy = pr.cy();
        uiBatch_.rect(cx, cy, L.px(46) * s, L.px(46) * s, L.px(23) * s, rgba(ui::kCard), rgba(ui::kInk), L.px(2.5f));
        uiArt(kUiWeapon + wpn, cx, cy, 32.0f * s);
    }
    // Under it: the road to the end with the soldier marching along it, or once the ogre is in sight, its strength.
    const float barY = top + L.px(82);
    const float barW = minv(W - L.safe.left - L.safe.right - L.px(84), L.px(420));
    const float x0 = W * 0.5f - barW * 0.5f;
    const Rgba headTint = rgba(bossTint());
    const int bi = battle_.bossIndex();
    if (bi >= 0 && battle_.enemies[bi].z - battle_.z < 60.0f) {
        const float frac = clampv(float(battle_.enemies[bi].hp) / float(maxv<i32>(def_->bossHp * sim::kHp, 1)), 0.0f, 1.0f);
        bossLag_ = maxv(frac, minv(bossLag_, 1.0f) - uiDt_ * 0.35f);  // a gold strip trails what was just lost
        const float h = L.px(26);
        uiBatch_.rect(W * 0.5f, barY, barW, h, h * 0.5f, rgba(ui::kInk), rgba(col::kWhite, 90), L.px(1.5f));
        if (bossLag_ > frac) uiBatch_.rect(x0 + barW * bossLag_ * 0.5f, barY, barW * bossLag_, h, h * 0.5f, rgba(ui::kGold));
        if (frac > 0) {
            uiBatch_.rect(x0 + barW * frac * 0.5f, barY, barW * frac, h, h * 0.5f, rgba(ui::kRed));
            if (barW * frac > h) uiBatch_.rect(x0 + barW * frac * 0.5f, barY - h * 0.25f, barW * frac - h * 0.8f, L.px(3), L.px(1.5f), rgba(ui::kRedLight));
        }
        uiBatch_.text(bossTitle(def_->world), W * 0.5f, barY, L.px(16), rgba(col::kWhite), rgba(ui::kInk), L.px(1.5f), Align::Center);
        uiArt(kUiBossHead, x0, barY, 48, headTint, 0, bossHue());
    } else {
        bossLag_ = 1;
        const float p = battle_.progress();
        const float h = L.px(14);
        const u32 fill = hard ? ui::kRed : ui::kCobalt, light = hard ? ui::kRedLight : ui::kCobaltLight;
        uiBatch_.rect(W * 0.5f, barY, barW, h, h * 0.5f, rgba(ui::kInk, 153), rgba(col::kWhite, 90), L.px(1.5f));
        if (barW * p > h) {
            uiBatch_.rect(x0 + barW * p * 0.5f, barY, barW * p, h, h * 0.5f, rgba(fill));
            uiBatch_.rect(x0 + barW * p * 0.5f, barY - h * 0.2f, barW * p - h * 0.7f, L.px(3), L.px(1.5f), rgba(light));
        }
        const bool boss = def_->bossHp > 0;
        if (!(boss ? uiArt(kUiBossHead, x0 + barW, barY, 34, headTint, 0, bossHue()) : uiArt(kUiStarFull, x0 + barW, barY, 26))) {
            if (boss) uiBatch_.rect(x0 + barW, barY, L.px(22), L.px(22), L.px(11), rgba(kHordeRed), rgba(col::kWhite), L.px(2.5f));
            else uiBatch_.star5(x0 + barW, barY, L.px(12), rgba(ui::kGold), rgba(ui::kInk, 160), L.px(1.5f));
        }
        const bool marching = battle_.started && !battle_.won && !battle_.lost;
        const float bob = marching ? L.px(1.5f) * std::fabs(std::sin(levelTime_ * 9.4f)) : 0.0f;
        if (!uiArt(kUiShako, x0 + barW * p, barY - L.px(10) - bob, 40))
            uiBatch_.rect(x0 + barW * p, barY, L.px(18), L.px(18), L.px(9), rgba(col::kWhite), rgba(kSquadBlue), L.px(3));
    }
    // Under the bar: the power-ups running, each its icon on a cream disc in a cobalt ring draining clockwise.
    int run[sim::kPowerCount] = {}, nRun = 0;
    for (int p = sim::kRapid; p < sim::kPowerCount; ++p)
        if (battle_.powered(u8(p)) && battle_.powerFull[p] > 0.0f) run[nRun++] = p;
    for (int i = 0; i < nRun; ++i) {
        const int p = run[i];
        const float cx = W * 0.5f + (float(i) - 0.5f * float(nRun - 1)) * L.px(46), cy = barY + L.px(40);
        uiBatch_.rect(cx, cy, L.px(36), L.px(36), L.px(18), rgba(ui::kCard), rgba(ui::kInk), L.px(2));
        uiBatch_.ring(cx, cy, L.px(21), L.px(4.5f), battle_.powerLeft[p] / battle_.powerFull[p], rgba(ui::kCobalt));
        uiArt(kUiPower + p, cx, cy, 24);
    }
    // A level's hint line under the bar (a red pill on a hard level), under the power-ups if any.
    if (hintLineTime_ > 0 && hintShownFor_ > 0) {
        const float a = minv(1.0f, hintLineTime_ / 0.4f);
        const char* s = sim::hintText(hintShownFor_);
        const bool red = hintShownFor_ == sim::kHintHard;
        const float y = barY + L.px(nRun > 0 ? 88 : 44);
        if (def_->mode == sim::kModeGold && uiArt_[kUiGoldM] >= 0) {  // a bonus road: the gold ribbon (ART_DIRECTION 17)
            ribbon("BONUS ROAD", W * 0.5f, y + L.px(4), 2, 20);
        } else {
            const float w = UiBatch::textWidth(s, L.px(15)) + L.px(32);
            uiBatch_.rect(W * 0.5f, y, w, L.px(34), L.px(17), red ? rgba(ui::kRed, u8(240 * a)) : rgba(ui::kInk, u8(224 * a)),
                          rgba(col::kWhite, u8((red ? 160 : 64) * a)), L.px(1.5f));
            uiBatch_.text(s, W * 0.5f, y, L.px(15), rgba(col::kWhite, u8(255 * a)), {0, 0, 0, 0}, 0, Align::Center);
        }
    }
    // Before the march: a hand swinging under the squad, DRAG TO MARCH under it.
    if (playing && !battle_.started && turnT_ < 0.0f && titleFade_ < 0.5f) {
        const float y = minv(L.bottom() - L.px(96), field_.squadScreenY() + L.px(92));
        const float hx = W * 0.5f + std::sin(screenTime_ * 6.2831853f / 1.6f) * L.px(60);
        if (!uiArt(kUiHand, hx, y, 64)) {
            uiBatch_.rect(hx, y + L.px(10), L.px(34), L.px(40), L.px(14), rgba(col::kWhite), rgba(ui::kInk), L.px(2.5f));
            uiBatch_.rect(hx - L.px(4), y - L.px(14), L.px(12), L.px(28), L.px(6), rgba(col::kWhite), rgba(ui::kInk), L.px(2.5f));
        }
        uiBatch_.text("DRAG TO MARCH", W * 0.5f, y + L.px(54), L.px(20), rgba(col::kWhite), rgba(ui::kInk), L.px(2), Align::Center);
    }
}

void Game::drawPause() {
    const Layout& L = layout_;
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(ui::kInk, 115));
    const Rect card = centered(L, float(L.h) * 0.48f, 300, 364);
    ui_.panel(card, 22);
    ribbon("PAUSED", float(L.w) * 0.5f, card.y, 0);
    float y = card.y + L.px(76);
    if (ui_.button("RESUME", centered(L, y, 240, 56), Ui::kPrimary)) {
        feedback_.uiTap();
        setScreen(Screen::Play);
        return;
    }
    y += L.px(78);
    if (ui_.button("RESTART", centered(L, y, 220, 56))) {
        feedback_.uiTap();
        restart();
        return;
    }
    y += L.px(78);
    if (ui_.button("LEVELS", centered(L, y, 220, 56))) {
        feedback_.uiTap();
        levelsFirst_ = 0;
        setScreen(Screen::Levels);
        return;
    }
    y += L.px(78);
    if (ui_.button("SETTINGS", centered(L, y, 220, 56))) {
        feedback_.uiTap();
        settingsReturn_ = Screen::Pause;
        setScreen(Screen::Settings);
        return;
    }
    drawUpdatePill(card.y + card.h + L.px(18));
}

void Game::drawWin() {
    const Layout& L = layout_;
    const float t = resultTime_;
    const float in = minv(1.0f, t / 0.25f);
    ui_.opacity = in;
    const bool perfect = stars_ >= 3;
    // As tall as its content and 20 dp more (no RETRY on a perfect win), its foot where it always is.
    const float cardH = L.px(perfect ? 262.0f : 318.0f);
    const Rect card{float(L.w) * 0.5f - L.px(165), L.bottom() - L.px(91) - cardH + (1.0f - in) * L.px(40), L.px(330), cardH};
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(ui::kInk, u8(70 * in)));
    ui_.panel(card, 22);
    ribbon(perfect ? "PERFECT!" : "VICTORY!", float(L.w) * 0.5f, card.y, perfect ? 2 : 0, 26);
    // Three slots; each earned star slams in on a burst of rays, 0.25 s apart (ART_DIRECTION 15.4).
    for (int k = 0; k < 3; ++k) {
        const float landAt = 0.3f + 0.25f * float(k);
        const float x = float(L.w) * 0.5f + float(k - 1) * L.px(78);
        const float y = card.y + L.px(84) - (k == 1 ? L.px(10) : 0.0f);
        if (!uiArt(kUiStarEmpty, x, y, 72, {255, 255, 255, u8(255 * in)}))
            uiBatch_.star5(x, y, L.px(29), rgba(0xD8C8A8, u8(255 * in)), rgba(ui::kInk, u8(90 * in)), L.px(1.5f));
        if (k < stars_ && t >= landAt) {
            const float s = minv(1.0f, (t - landAt) / 0.18f);
            const float e = 1.0f - (1.0f - s) * (1.0f - s);  // ease out
            const float scale = 2.2f - 1.2f * e;
            uiArt(kUiRays, x, y, 120, {255, 200, 58, u8(200 * minv(1.0f, (t - landAt) * 3.0f))}, t * 0.5f + float(k));
            if (!uiArt(kUiStarFull, x, y, 72 * scale))
                uiBatch_.star5(x, y, L.px(27) * scale, rgba(ui::kGold), rgba(ui::kGoldDark), L.px(2.5f));
            if (starsLanded_ <= k) {
                starsLanded_ = k + 1;
                feedback_.star(k, double(resultTime_));
            }
        }
    }
    char a[24], line[64];
    formatHost(battle_.squad, a, sizeof a);
    std::snprintf(line, sizeof line, "SOLDIERS LEFT %s", a);
    ui_.text(line, float(L.w) * 0.5f, card.y + L.px(146), 18, ui::kText);
    const bool last = level_ >= sim::levelCount();
    if (ui_.button(last ? "FINISH" : "NEXT", centered(L, card.y + L.px(206), 250, 60), Ui::kPrimary, 1.1f)) {
        feedback_.uiTap();
        if (last) setScreen(Screen::End);
        else enterLevel(level_ + 1, true);
        ui_.opacity = 1;
        return;
    }
    if (!perfect && ui_.button("RETRY", centered(L, card.y + L.px(276), 160, 48), Ui::kSecondary, 0.85f)) {
        feedback_.uiTap();
        restart();
    }
    ui_.opacity = 1;
}

void Game::drawLose() {
    const Layout& L = layout_;
    const float in = minv(1.0f, resultTime_ / 0.25f);
    ui_.opacity = in;
    const Rect card = centered(L, L.bottom() - L.px(228), 330, 262);
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(ui::kInk, u8(70 * in)));
    ui_.panel(card, 22);
    ribbon("DEFEATED", float(L.w) * 0.5f, card.y, 1, 26);
    const float p = battle_.progress();
    const int pct = int(p * 100.0f + 0.5f);
    char line[48];
    std::snprintf(line, sizeof line, "YOU MARCHED %d%% OF THE ROAD", pct);
    ui_.text(line, float(L.w) * 0.5f, card.y + L.px(54), 15, ui::kMuted);
    {  // a small road with how far the squad got
        const float rw = L.px(220), rh = L.px(12), rx = float(L.w) * 0.5f - rw * 0.5f, ry = card.y + L.px(82);
        uiBatch_.rect(float(L.w) * 0.5f, ry, rw, rh, rh * 0.5f, rgba(ui::kInk, u8(153 * in)));
        if (rw * p > rh) uiBatch_.rect(rx + rw * p * 0.5f, ry, rw * p, rh, rh * 0.5f, rgba(ui::kRed, u8(255 * in)));
        uiArt(kUiShako, rx + rw * p, ry - L.px(6), 28, {255, 255, 255, u8(255 * in)});
    }
    if (ui_.button("RETRY", centered(L, card.y + L.px(146), 240, 60), Ui::kPrimary)) {
        feedback_.uiTap();
        restart();
        ui_.opacity = 1;
        return;
    }
    if (ui_.button("LEVELS", centered(L, card.y + L.px(216), 180, 50))) {
        feedback_.uiTap();
        levelsFirst_ = 0;
        setScreen(Screen::Levels);
    }
    ui_.opacity = 1;
}

void Game::drawSettings(float dt) {
    const Layout& L = layout_;
    const float usable = L.bottom() - L.top();
    const bool roomy = usable / L.dp > 740;
    const float rowH = roomy ? 56.0f : 52.0f, gap = roomy ? 14.0f : 10.0f;
    // As tall as what it holds (five rows, the credits, BACK) and 20 dp more.
    const float cardH = minv(usable - L.px(40), L.px(5.0f * (rowH + gap) + 252.0f));
    const Rect card{L.safe.left + L.px(10), L.top() + L.px(30), float(L.w) - L.safe.left - L.safe.right - L.px(20), cardH};
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(ui::kInk, 115));
    ui_.panel(card, 22);
    ribbon("SETTINGS", float(L.w) * 0.5f, card.y, 0);
    float y = card.y + L.px(68);
    bool changed = false;
    struct Sw {
        const char* name;
        bool* v;
    } sws[] = {{"SOUND", &save_.sound}, {"MUSIC", &save_.music}, {"HAPTICS", &save_.haptics}, {"REDUCE EFFECTS", &save_.reduceEffects}};
    for (Sw& s : sws) {
        if (ui_.toggle(s.name, centered(L, y, 320, rowH), *s.v)) changed = true;
        y += L.px(rowH + gap);
    }
    if (changed) {
        feedback_.uiTap();
        applySettings();
        saveNow();
    }
    y += L.px(2);
    if (!confirmReset_) {
        if (ui_.holdButton("HOLD TO RESET PROGRESS", centered(L, y, 320, rowH), 1.0f, dt, holdProgress_)) confirmReset_ = true;
    } else {
        if (ui_.button("CONFIRM RESET", {float(L.w) * 0.5f - L.px(164), y - L.px(rowH * 0.5f), L.px(184), L.px(rowH)})) {
            const SaveData keep = save_;
            save_ = SaveData();
            save_.sound = keep.sound;
            save_.music = keep.music;
            save_.haptics = keep.haptics;
            save_.reduceEffects = keep.reduceEffects;
            saveNow();
            confirmReset_ = false;
            enterLevel(1, true);
            return;
        }
        if (ui_.button("CANCEL", {float(L.w) * 0.5f + L.px(30), y - L.px(rowH * 0.5f), L.px(134), L.px(rowH)})) confirmReset_ = false;
    }
    y += L.px(rowH + gap + 6);
    char ver[64];
    if (PM_BUILD_NUMBER > 0) std::snprintf(ver, sizeof ver, "VERSION %s BUILD %d (%s)", PM_VERSION, PM_BUILD_NUMBER, PM_COMMIT);
    else std::snprintf(ver, sizeof ver, "VERSION %s (%s)", PM_VERSION, PM_COMMIT);
    for (char* c = ver; *c; ++c) *c = char(std::toupper(static_cast<unsigned char>(*c)));  // the font is capitals only
    ui_.text(ver, float(L.w) * 0.5f, y, 14, ui::kMuted);
    y += L.px(22);
    ui_.text("FONT: ROBOTO, APACHE 2.0", float(L.w) * 0.5f, y, 13, ui::kMuted);
    y += L.px(18);
    ui_.text("SDL 3: ZLIB LICENCE", float(L.w) * 0.5f, y, 13, ui::kMuted);
    y += L.px(18);
    ui_.text("PAPER, SOUND AND MUSIC MADE IN CODE", float(L.w) * 0.5f, y, 13, ui::kMuted);
    const float backY = minv(card.y + card.h - L.px(54), y + L.px(64));
    if (ui_.button("BACK", centered(L, backY, 220, 56))) {
        feedback_.uiTap();
        setScreen(settingsReturn_);
    }
}

// The level selector (GLOWTIDE's, 2026-11-24): every level, five a row and as many rows as fit (eight at most), each a
// button over its best stars, the current one gold; a level starts at once.
void Game::drawLevels() {
    const Layout& L = layout_;
    const float usable = L.bottom() - L.top();
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(ui::kInk, 115));
    const Rect card{L.safe.left + L.px(8), L.top() + L.px(30), float(L.w) - L.safe.left - L.safe.right - L.px(16), usable - L.px(38)};
    ui_.panel(card, 22);
    const int count = sim::levelCount();
    const float gridTop = card.y + L.px(76), gridBottom = L.bottom() - L.px(150);
    const int rows = clampv(int((gridBottom - gridTop) / L.px(76)), 3, 8), page = 5 * rows;
    if (levelsFirst_ <= 0) levelsFirst_ = (minv(level_, count) - 1) / page * page + 1;
    levelsFirst_ = clampv(levelsFirst_, 1, maxv(1, count - page + 1));
    const int last = minv(count, levelsFirst_ + page - 1);
    char buf[48];
    // The page's world on the ribbon (WORLD 3 - FROSTVALE), the range under it.
    {
        const int world = sim::level(levelsFirst_).world;
        const char* name = art_.ready() && art_.world(world).defined && art_.world(world).name[0] ? art_.world(world).name : worldPalette(world).name;
        std::snprintf(buf, sizeof buf, "WORLD %d - %s", world, name);
        for (char* c = buf; *c; ++c) *c = char(std::toupper(static_cast<unsigned char>(*c)));
        ribbon(buf, float(L.w) * 0.5f, card.y, 0, 20);
    }
    std::snprintf(buf, sizeof buf, "LEVELS %d-%d OF %d", levelsFirst_, last, count);
    ui_.text(buf, float(L.w) * 0.5f, card.y + L.px(46), 15, ui::kMuted);
    const float gap = L.px(10);
    const float cellW = minv(L.px(64), (float(L.w) - L.safe.left - L.safe.right - L.px(48) - 4 * gap) / 5.0f);
    const float gridW = 5 * cellW + 4 * gap, x0 = float(L.w) * 0.5f - gridW * 0.5f;
    const float pitch = (gridBottom - gridTop) / float(rows);
    const int next = clampv(save_.level, 1, count);
    for (int lv = levelsFirst_; lv <= last; ++lv) {
        const int i = lv - levelsFirst_;
        const int got = lv <= int(sizeof save_.stars) ? save_.stars[lv - 1] : 0;
        const bool here = lv == next;
        const float bob = here ? L.px(2) * std::sin(screenTime_ * 4.0f) : 0.0f;
        const Rect r{x0 + float(i % 5) * (cellW + gap), gridTop + float(i / 5) * pitch + bob, cellW, L.px(64)};
        std::snprintf(buf, sizeof buf, "%d", lv);
        const bool down = ui_.pressing(r, buf);
        const Ui::Style style = here ? Ui::kPrimary : (got > 0 ? Ui::kSecondary : Ui::kMuted);
        // Its number above its three stars; a hard level a red band and a skull, a boss level the ogre's head.
        if (ui_.button(buf, r, style, 20.0f / 22.0f, 0, -L.px(9))) {
            feedback_.uiTap();
            enterLevel(lv, true);
            return;
        }
        const float sink = down ? L.px(4) : 0.0f;
        const sim::LevelDef& d = sim::level(lv);
        if (d.spike && !down) uiBatch_.rect(r.cx(), r.y + L.px(8), r.w - L.px(5), L.px(12), L.px(6), rgba(ui::kRed, 220));
        if (d.spike) uiArt(kUiSkull, r.x + r.w - L.px(9), r.y + L.px(9) + sink, 14);
        else if (d.bossHp > 0 && art_.ready())  // over the cell's top right corner by 6 dp
            uiArt(kUiBossHead, r.x + r.w - L.px(6), r.y + L.px(6) + sink, 24, rgba(art_.world(d.world).bossTint), 0,
                  std::fmod(std::fmod(art_.world(d.world).bossHue, 360.0f) + 360.0f, 360.0f) / 360.0f);
        for (int k = 0; k < 3; ++k) {
            const float sx = r.cx() + float(k - 1) * L.px(15), sy = r.y + r.h - L.px(14) + sink;
            if (!uiArt(k < got ? kUiStarFull : kUiStarEmpty, sx, sy, 14))
                uiBatch_.star5(sx, sy, L.px(6.5f), k < got ? rgba(ui::kGold) : rgba(0xC8B494));
        }
    }
    const float navW = (gridW - 3 * gap) / 4.0f;
    const char* const nav[4] = {"-10", "PREV", "NEXT", "+10"};  // ten levels: a world
    const int step[4] = {-10, -page, page, 10};
    for (int k = 0; k < 4; ++k)
        if (ui_.button(nav[k], {x0 + float(k) * (navW + gap), L.bottom() - L.px(130), navW, L.px(48)}, Ui::kSecondary, 0.85f)) {
            feedback_.uiTap();
            levelsFirst_ = clampv(levelsFirst_ + step[k], 1, maxv(1, count - page + 1));
        }
    if (ui_.button("BACK", centered(L, L.bottom() - L.px(46), 220, 52))) {
        feedback_.uiTap();
        setScreen(battle_.won ? Screen::Win : (battle_.lost ? Screen::Lose : Screen::Play));
    }
}

void Game::drawEnd() {
    const Layout& L = layout_;
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(ui::kInk, 125));
    const float W = float(L.w);
    const Rect card = centered(L, float(L.h) * 0.56f, 330, 300);
    // The trophy on a gold sunburst over the card, the gold ribbon on its edge (ART_DIRECTION 15.4).
    uiArt(kUiRays, W * 0.5f, card.y - L.px(70), 320, {255, 200, 58, 230}, screenTime_ * 0.1745f);
    ui_.panel(card, 22);
    if (!uiArt(kUiTrophy, W * 0.5f, card.y - L.px(78), 160)) {
        for (int k = 0; k < 3; ++k) uiBatch_.star5(W * 0.5f + float(k - 1) * L.px(54), card.y - L.px(60), L.px(20), rgba(ui::kGold));
    }
    char buf[48];
    std::snprintf(buf, sizeof buf, "ALL %d LEVELS CONQUERED!", sim::levelCount());
    ribbon(buf, W * 0.5f, card.y, 2, 18);
    std::snprintf(buf, sizeof buf, "STARS %d OF %d", save_.totalStars(), 3 * sim::levelCount());
    const float tw = UiBatch::textWidth(buf, L.px(18));
    uiArt(kUiStarFull, W * 0.5f - tw * 0.5f - L.px(18), card.y + L.px(70), 26);
    ui_.text(buf, W * 0.5f + L.px(4), card.y + L.px(70), 18, ui::kText);
    if (ui_.button("PLAY AGAIN FROM 1", centered(L, card.y + L.px(160), 280, 60), Ui::kPrimary, 0.8f)) {
        feedback_.uiTap();
        enterLevel(1, true);
        return;
    }
    if (ui_.button("LEVELS", centered(L, card.y + L.px(238), 200, 52))) {
        feedback_.uiTap();
        levelsFirst_ = 0;
        setScreen(Screen::Levels);
    }
}

// ---- The flow test ---------------------------------------------------------------------------------------------------

void Game::check(bool ok, const char* what) {
    ++flowChecks_;
    if (!ok) ++flowFailures_;
    PM_TAG("FLOW", "%s %s", ok ? "ok  " : "FAIL", what);
}

bool Game::press(const char* label) {
    const UiControl* c = ui_.find(label);
    if (!c) return false;
    const float x = c->r.cx(), y = c->r.cy();
    ui_.pointerDown(77, x, y);
    ui_.pointerUp(77, x, y);
    return true;
}

void Game::flowStep() {
    ++flowWait_;
    auto next = [&] {
        ++flowStep_;
        flowWait_ = 0;
    };
    auto timeout = [&](int ticks, const char* what) {
        if (flowWait_ > ticks) {
            check(false, what);
            flowStep_ = 100;
        }
    };
    auto settled = [&] { return pendingLevel_ == 0 && turnT_ < 0.0f; };
    switch (flowStep_) {
        case 1:
            if (flowWait_ < 10) break;
            check(screen_ == Screen::Play && level_ == 1 && !battle_.started, "starts on level 1, waiting for a touch");
            next();
            break;
        case 2: {  // a drag on the road starts the march and steers the squad
            const float x = field_.squadScreenX(), y = field_.squadScreenY();
            pointer({PtrType::Down, 5, x, y, 0, 0});
            pointer({PtrType::Move, 5, x + layout_.px(60), y, layout_.px(60), 0});
            next();
            break;
        }
        case 3:
            if (flowWait_ < 30) break;
            pointer({PtrType::Up, 5, field_.squadScreenX(), field_.squadScreenY(), 0, 0});
            check(battle_.started && battle_.z > 0.5f, "a touch on the road starts the march");
            check(steerX_ > 0.5f && battle_.x > 0.3f, "dragging right steers the squad right");
            next();
            break;
        case 4:  // the bot finishes the level
            useBot_ = true;
            if (screen_ == Screen::Win) {
                useBot_ = false;
                check(stars_ >= 1, "level 1 is won with stars");
                next();
            }
            timeout(60 * 90, "level 1 is won within 90 s");
            break;
        case 5:
            if (resultTime_ < 1.0f) break;
            check(press("NEXT"), "the win panel has NEXT");
            next();
            break;
        case 6:
            if (!settled() || flowWait_ < 3) break;
            check(level_ == 2 && screen_ == Screen::Play && !battle_.started, "NEXT plays level 2");
            next();
            break;
        case 7:
            if (flowWait_ < 3) break;
            check(press("LEVEL 2"), "the level chip is a button");
            next();
            break;
        case 8:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Levels, "the level chip opens LEVELS");
            check(press("4"), "LEVELS shows level 4");
            next();
            break;
        case 9:
            if (!settled() || flowWait_ < 3) break;
            check(level_ == 4 && screen_ == Screen::Play, "a level's button starts it");
            // A squad of one, steered past every gate, cannot hold the road.
            battle_.squad = 1;
            go_ = true;
            steerX_ = 4.9f;
            next();
            break;
        case 10:
            if (battle_.squad > 1 && !battle_.lost) battle_.squad = 1;
            if (screen_ == Screen::Lose) {
                check(battle_.lost, "a squad of one is beaten");
                next();
            }
            timeout(60 * 90, "a beaten squad shows DEFEATED");
            break;
        case 11:
            if (resultTime_ < 0.6f) break;
            check(press("RETRY"), "DEFEATED offers RETRY");
            next();
            break;
        case 12:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Play && level_ == 4 && !battle_.started && battle_.squad == def_->start, "RETRY restarts the level");
            check(press("#PAUSE"), "the pause button");
            next();
            break;
        case 13:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Pause, "pause opens PAUSE");
            check(press("SETTINGS"), "PAUSE has SETTINGS");
            next();
            break;
        case 14:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Settings, "SETTINGS is open");
            press("SOUND");
            next();
            break;
        case 15:
            if (flowWait_ < 3) break;
            check(!save_.sound, "SOUND switches the sound off");
            press("SOUND");
            next();
            break;
        case 16:
            if (flowWait_ < 3) break;
            check(save_.sound, "and on again");
            check(press("BACK"), "SETTINGS has BACK");
            next();
            break;
        case 17:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Pause, "BACK returns to PAUSE");
            check(press("RESUME"), "PAUSE has RESUME");
            next();
            break;
        case 18:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Play, "RESUME plays on");
            next();
            break;
        default:
            PM_TAG("FLOW", "%d checks, %d failed", flowChecks_, flowFailures_);
            exitCode_ = flowFailures_ ? 6 : 0;
            quit_ = true;
            flowStep_ = 0;
            break;
    }
}

}  // namespace pm
