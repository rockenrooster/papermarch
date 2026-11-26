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

void pauseGlyph(UiBatch& b, const Rect& r, const Layout& L) {
    const Rgba c = rgba(ui::kText);
    b.rect(r.cx() - L.px(6), r.cy(), L.px(6), L.px(20), L.px(3), c);
    b.rect(r.cx() + L.px(6), r.cy(), L.px(6), L.px(20), L.px(3), c);
}

Rect pauseRect(const Layout& L) {
    const float s = L.px(56);
    return {float(L.w) - L.safe.right - L.px(12) - s, L.top() + L.px(10), s, s};
}

const char* bossTitle(int world) {
    static const char* const names[kWorldCount] = {"OGRE CHIEF", "SAND OGRE", "FROST OGRE", "EMBER OGRE", "SALT OGRE",
                                                   "THORN OGRE", "BOG OGRE",   "CLOUD OGRE", "ASH OGRE",   "LANTERN OGRE"};
    return names[(maxv(world, 1) - 1) % kWorldCount];
}

}  // namespace

void Game::drawUi(float dt) {
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

void Game::drawTitle() {
    const Layout& L = layout_;
    const float a = titleFade_;
    const float cy = L.top() + L.px(190);
    uiBatch_.rect(float(L.w) * 0.5f + L.px(3), cy + L.px(7), L.px(340), L.px(120), L.px(24), rgba(col::kShadow, u8(80 * a)), {0, 0, 0, 0}, 0,
                  L.px(8));
    uiBatch_.rect(float(L.w) * 0.5f, cy, L.px(340), L.px(120), L.px(24), rgba(ui::kCard, u8(250 * a)), rgba(kSquadBlue, u8(255 * a)), L.px(3));
    uiBatch_.text("PAPERMARCH", float(L.w) * 0.5f, cy - L.px(18), L.px(46), rgba(kSquadBlue, u8(255 * a)), rgba(0x123070, u8(255 * a)), L.px(1.5f),
                  Align::Center);
    uiBatch_.text("GROW YOUR SQUAD. CRUSH THE HORDE.", float(L.w) * 0.5f, cy + L.px(28), L.px(15), rgba(ui::kText, u8(255 * a)), {0, 0, 0, 0}, 0,
                  Align::Center);
    drawUpdatePill(cy + L.px(76));
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
    char buf[64];
    // Top: the level chip (it opens LEVELS) with its saved stars, pause on the right.
    std::snprintf(buf, sizeof buf, "LEVEL %d", level_);
    const float top = L.top() + L.px(10);
    const float chipW = UiBatch::textWidth(buf, L.px(19)) + L.px(30);
    const float chipX = L.safe.left + L.px(12);
    if (ui_.button(buf, {chipX, top, chipW, L.px(56)}) && playing) {
        feedback_.uiTap();
        levelsFirst_ = 0;
        setScreen(Screen::Levels);
        return;
    }
    const int got = level_ - 1 < int(sizeof save_.stars) ? save_.stars[level_ - 1] : 0;
    for (int k = 0; k < 3; ++k)
        uiBatch_.star5(chipX + chipW + L.px(20) + float(k) * L.px(24), top + L.px(28), L.px(10.5f), rgba(k < got ? col::kGold : 0x5A4A40, 230),
                       rgba(col::kShadow, 120), L.px(1));
    const Rect pr = pauseRect(L);
    if (ui_.button("#PAUSE", pr, Ui::kRound) && playing) {
        feedback_.uiTap();
        setScreen(Screen::Pause);
        return;
    }
    pauseGlyph(uiBatch_, pr, L);
    // Under it: the road to the end (the squad's progress), or once the boss is out, its strength.
    const float barY = top + L.px(84);
    const float barW = minv(float(L.w) - L.safe.left - L.safe.right - L.px(60), L.px(420));
    const float x0 = float(L.w) * 0.5f - barW * 0.5f;
    const int bi = battle_.bossIndex();
    if (bi >= 0 && battle_.enemies[bi].z - battle_.z < 60.0f) {
        const float frac = clampv(float(battle_.enemies[bi].hp) / float(maxv<i32>(def_->bossHp, 1)), 0.0f, 1.0f);
        const float h = L.px(20);
        uiBatch_.rect(float(L.w) * 0.5f, barY, barW + L.px(6), h + L.px(6), h * 0.5f + L.px(3), rgba(0x2A0A10, 220));
        if (frac > 0) uiBatch_.rect(x0 + barW * frac * 0.5f, barY, barW * frac, h, h * 0.5f, rgba(kHordeRed), rgba(0xFF8A8A), L.px(1.5f));
        uiBatch_.text(bossTitle(def_->world), float(L.w) * 0.5f, barY, L.px(14), rgba(col::kWhite), rgba(0x2A0A10, 200), L.px(1.2f), Align::Center);
    } else {
        const float p = battle_.progress();
        const float h = L.px(12);
        uiBatch_.rect(float(L.w) * 0.5f, barY, barW + L.px(4), h + L.px(4), h * 0.5f + L.px(2), rgba(0x1A1410, 150));
        if (p > 0.005f) uiBatch_.rect(x0 + barW * p * 0.5f, barY, barW * p, h, h * 0.5f, rgba(kSquadBlue));
        uiBatch_.rect(x0 + barW * p, barY, L.px(18), L.px(18), L.px(9), rgba(col::kWhite), rgba(kSquadBlue), L.px(3));
        if (def_->bossHp > 0) uiBatch_.rect(x0 + barW, barY, L.px(22), L.px(22), L.px(11), rgba(kHordeRed), rgba(col::kWhite), L.px(2.5f));
        else uiBatch_.star5(x0 + barW, barY, L.px(12), rgba(col::kGold), rgba(col::kShadow, 140), L.px(1.5f));
    }
    // A level's hint line, once, under the bar.
    if (hintLineTime_ > 0 && hintShownFor_ > 0) {
        const float a = minv(1.0f, hintLineTime_ / 0.4f);
        const char* s = sim::hintText(hintShownFor_);
        const float y = barY + L.px(40);
        const float w = UiBatch::textWidth(s, L.px(17)) + L.px(32);
        uiBatch_.rect(float(L.w) * 0.5f, y, w, L.px(38), L.px(19), rgba(0x123070, u8(235 * a)), rgba(col::kWhite, u8(220 * a)), L.px(1.5f));
        uiBatch_.text(s, float(L.w) * 0.5f, y, L.px(17), rgba(col::kWhite, u8(255 * a)), {0, 0, 0, 0}, 0, Align::Center);
    }
    // Before the march: DRAG TO MARCH, a hand swinging under the squad.
    if (playing && !battle_.started && turnT_ < 0.0f && titleFade_ < 0.5f) {
        const float y = minv(L.bottom() - L.px(70), field_.squadScreenY() + L.px(96));
        const float pulse = 1.0f + 0.05f * std::sin(screenTime_ * 5.0f);
        uiBatch_.text("DRAG TO MARCH", float(L.w) * 0.5f, y, L.px(30) * pulse, rgba(col::kWhite), rgba(0x123070), L.px(3), Align::Center, true,
                      L.px(4));
        const float hx = float(L.w) * 0.5f + std::sin(screenTime_ * 2.6f) * L.px(90);
        const float hy = y + L.px(46);
        uiBatch_.rect(hx, hy + L.px(10), L.px(34), L.px(40), L.px(14), rgba(col::kWhite), rgba(0x123070), L.px(2.5f));
        uiBatch_.rect(hx - L.px(4), hy - L.px(14), L.px(12), L.px(28), L.px(6), rgba(col::kWhite), rgba(0x123070), L.px(2.5f));
        uiBatch_.rect(float(L.w) * 0.5f, hy + L.px(42), L.px(200), L.px(4), L.px(2), rgba(col::kWhite, 150));
    }
}

void Game::drawPause() {
    const Layout& L = layout_;
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(col::kShadow, 110));
    const Rect card = centered(L, float(L.h) * 0.48f, 300, 400);
    ui_.panel(card, 26);
    ui_.text("PAUSED", float(L.w) * 0.5f, card.y + L.px(46), 34, kSquadBlue);
    float y = card.y + L.px(118);
    if (ui_.button("RESUME", centered(L, y, 240, 64), Ui::kPrimary)) {
        feedback_.uiTap();
        setScreen(Screen::Play);
        return;
    }
    y += L.px(76);
    if (ui_.button("RESTART", centered(L, y, 220, 54))) {
        feedback_.uiTap();
        restart();
        return;
    }
    y += L.px(64);
    if (ui_.button("LEVELS", centered(L, y, 220, 54))) {
        feedback_.uiTap();
        levelsFirst_ = 0;
        setScreen(Screen::Levels);
        return;
    }
    y += L.px(64);
    if (ui_.button("SETTINGS", centered(L, y, 220, 54))) {
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
    const Rect card = centered(L, L.bottom() - L.px(255) + (1.0f - in) * L.px(40), 330, 322);
    ui_.panel(card, 26);
    const bool perfect = stars_ >= 3;
    ui_.text(perfect ? "PERFECT!" : "VICTORY!", float(L.w) * 0.5f, card.y + L.px(42), 36, perfect ? col::kGoldDark : kSquadBlue);
    for (int k = 0; k < 3; ++k) {
        const float landAt = 0.25f + 0.28f * float(k);
        const float x = float(L.w) * 0.5f + float(k - 1) * L.px(70);
        const float y = card.y + L.px(100) - (k == 1 ? L.px(10) : 0.0f);
        uiBatch_.star5(x, y, L.px(29), rgba(0xD8C8A8, u8(255 * in)), rgba(col::kShadow, u8(60 * in)), L.px(1.5f));
        if (k < stars_ && t >= landAt) {
            const float s = minv(1.0f, (t - landAt) / 0.18f);
            const float pop = s < 1.0f ? 1.35f - 0.35f * s : 1.0f;
            uiBatch_.star5(x, y, L.px(27) * pop, rgba(col::kGold), rgba(col::kGoldDark), L.px(2.5f));
            if (starsLanded_ <= k) {
                starsLanded_ = k + 1;
                feedback_.star(k, double(resultTime_));
            }
        }
    }
    char a[24], line[64];
    formatHost(battle_.squad, a, sizeof a);
    std::snprintf(line, sizeof line, "SOLDIERS LEFT %s", a);
    ui_.text(line, float(L.w) * 0.5f, card.y + L.px(154), 18, ui::kText);
    const bool last = level_ >= sim::levelCount();
    if (ui_.button(last ? "FINISH" : "NEXT", centered(L, card.y + L.px(214), 250, 66), Ui::kPrimary, 1.1f)) {
        feedback_.uiTap();
        if (last) setScreen(Screen::End);
        else enterLevel(level_ + 1, true);
        ui_.opacity = 1;
        return;
    }
    if (!perfect && ui_.button("RETRY", centered(L, card.y + L.px(282), 150, 48), Ui::kSecondary, 0.85f)) {
        feedback_.uiTap();
        restart();
    }
    ui_.opacity = 1;
}

void Game::drawLose() {
    const Layout& L = layout_;
    const float in = minv(1.0f, resultTime_ / 0.25f);
    ui_.opacity = in;
    const Rect card = centered(L, L.bottom() - L.px(225), 330, 250);
    ui_.panel(card, 26);
    ui_.text("DEFEATED", float(L.w) * 0.5f, card.y + L.px(42), 34, col::kScarletDark);
    const int pct = int(battle_.progress() * 100.0f + 0.5f);
    char line[48];
    std::snprintf(line, sizeof line, "YOU MARCHED %d%% OF THE ROAD", pct);
    ui_.text(line, float(L.w) * 0.5f, card.y + L.px(80), 15, ui::kMuted);
    if (ui_.button("RETRY", centered(L, card.y + L.px(144), 240, 64), Ui::kPrimary)) {
        feedback_.uiTap();
        restart();
        ui_.opacity = 1;
        return;
    }
    if (ui_.button("LEVELS", centered(L, card.y + L.px(212), 170, 48))) {
        feedback_.uiTap();
        levelsFirst_ = 0;
        setScreen(Screen::Levels);
    }
    ui_.opacity = 1;
}

void Game::drawSettings(float dt) {
    const Layout& L = layout_;
    const float usable = L.bottom() - L.top();
    const Rect card{L.safe.left + L.px(10), L.top() + L.px(8), float(L.w) - L.safe.left - L.safe.right - L.px(20), usable - L.px(16)};
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(col::kShadow, 110));
    ui_.panel(card, 24);
    float y = L.top() + L.px(48);
    ui_.text("SETTINGS", float(L.w) * 0.5f, y, 30, kSquadBlue);
    const bool roomy = usable / L.dp > 740;
    const float rowH = roomy ? 56.0f : 52.0f, gap = roomy ? 14.0f : 10.0f;
    y += L.px(56);
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
    const float backY = minv(L.bottom() - L.px(52), y + L.px(64));
    if (ui_.button("BACK", centered(L, backY, 220, 60), Ui::kPrimary)) {
        feedback_.uiTap();
        setScreen(settingsReturn_);
    }
}

// The level selector (GLOWTIDE's, 2026-11-24): every level, five a row and as many rows as fit (eight at most), each a
// button over its best stars, the current one gold; a level starts at once.
void Game::drawLevels() {
    const Layout& L = layout_;
    const float usable = L.bottom() - L.top();
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(col::kShadow, 110));
    ui_.panel({L.safe.left + L.px(8), L.top() + L.px(8), float(L.w) - L.safe.left - L.safe.right - L.px(16), usable - L.px(16)}, 24);
    const int count = sim::levelCount();
    const int rows = clampv(int((usable - L.px(218)) / L.px(66)), 3, 8), page = 5 * rows;
    if (levelsFirst_ <= 0) levelsFirst_ = (minv(level_, count) - 1) / page * page + 1;
    levelsFirst_ = clampv(levelsFirst_, 1, maxv(1, count - page + 1));
    const int last = minv(count, levelsFirst_ + page - 1);
    char buf[32];
    ui_.text("LEVELS", float(L.w) * 0.5f, L.top() + L.px(38), 30, kSquadBlue);
    std::snprintf(buf, sizeof buf, "%d-%d OF %d", levelsFirst_, last, count);
    ui_.text(buf, float(L.w) * 0.5f, L.top() + L.px(68), 15, ui::kMuted);
    const float gap = L.px(8);
    const float cellW = minv(L.px(76), (float(L.w) - L.safe.left - L.safe.right - L.px(48) - 4 * gap) / 5.0f);
    const float gridW = 5 * cellW + 4 * gap, x0 = float(L.w) * 0.5f - gridW * 0.5f;
    const float pitch = minv(L.px(84), (usable - L.px(218)) / float(rows));
    for (int lv = levelsFirst_; lv <= last; ++lv) {
        const int i = lv - levelsFirst_;
        const Rect r{x0 + float(i % 5) * (cellW + gap), L.top() + L.px(86) + float(i / 5) * pitch, cellW, L.px(48)};
        std::snprintf(buf, sizeof buf, "%d", lv);
        const bool here = lv == level_;
        if (ui_.button(buf, r, here ? Ui::kPrimary : Ui::kSecondary, here ? 19.0f / 24.0f : 1.0f)) {
            feedback_.uiTap();
            enterLevel(lv, true);
            return;
        }
        const int got = lv <= int(sizeof save_.stars) ? save_.stars[lv - 1] : 0;
        for (int k = 0; k < 3; ++k)
            uiBatch_.star5(r.cx() + float(k - 1) * L.px(14), r.y + r.h + L.px(10), L.px(6.5f), k < got ? rgba(col::kGold) : rgba(0xC8B494));
    }
    const float navW = (gridW - 3 * gap) / 4.0f;
    const char* const nav[4] = {"-100", "PREV", "NEXT", "+100"};
    const int step[4] = {-100, -page, page, 100};
    for (int k = 0; k < 4; ++k)
        if (ui_.button(nav[k], {x0 + float(k) * (navW + gap), L.bottom() - L.px(128), navW, L.px(48)})) {
            feedback_.uiTap();
            levelsFirst_ = clampv(levelsFirst_ + step[k], 1, maxv(1, count - page + 1));
        }
    if (ui_.button("BACK", centered(L, L.bottom() - L.px(44), 220, 56), Ui::kPrimary)) {
        feedback_.uiTap();
        setScreen(battle_.won ? Screen::Win : (battle_.lost ? Screen::Lose : Screen::Play));
    }
}

void Game::drawEnd() {
    const Layout& L = layout_;
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(col::kShadow, 120));
    const Rect card = centered(L, float(L.h) * 0.5f, 330, 360);
    ui_.panel(card, 26);
    char buf[48];
    std::snprintf(buf, sizeof buf, "ALL %d LEVELS", sim::levelCount());
    ui_.text(buf, float(L.w) * 0.5f, card.y + L.px(48), 30, kSquadBlue);
    ui_.text("CONQUERED!", float(L.w) * 0.5f, card.y + L.px(86), 30, kSquadBlue);
    for (int k = 0; k < 3; ++k) uiBatch_.star5(float(L.w) * 0.5f + float(k - 1) * L.px(54), card.y + L.px(136), L.px(20), rgba(col::kGold));
    std::snprintf(buf, sizeof buf, "STARS %d OF %d", save_.totalStars(), 3 * sim::levelCount());
    ui_.text(buf, float(L.w) * 0.5f, card.y + L.px(180), 18, ui::kText);
    if (ui_.button("PLAY AGAIN FROM 1", centered(L, card.y + L.px(246), 280, 62), Ui::kPrimary, 0.8f)) {
        feedback_.uiTap();
        enterLevel(1, true);
        return;
    }
    if (ui_.button("LEVELS", centered(L, card.y + L.px(316), 200, 52))) {
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
