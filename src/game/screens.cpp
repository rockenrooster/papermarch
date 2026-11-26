// The screens (GAME_DESIGN 11) and the flow test (TECH_SPEC 9.4). Simple on purpose (the owner, 2026-11-26): a level,
// its result, the next level; PAUSE, SETTINGS and LEVELS are one tap away and never in the way.
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "build_stamp.h"
#include "core/log.h"
#include "core/tuning.h"
#include "game/game.h"
#include "platform/platform.h"
#include "sim/levels_hand.h"

namespace pm {

namespace {
inline u32 bit(int i) { return 1u << u32(i); }

void pauseGlyph(UiBatch& b, const Rect& r, const Layout& L, bool pressed) {
    const Rgba c = rgba(pressed ? col::kIvory : ui::kText);
    b.rect(r.cx() - L.px(6), r.cy(), L.px(6), L.px(20), L.px(3), c);
    b.rect(r.cx() + L.px(6), r.cy(), L.px(6), L.px(20), L.px(3), c);
}

Rect pauseRect(const Layout& L) {
    const float s = L.px(56);
    return {float(L.w) - L.safe.right - L.px(12) - s, L.top() + L.px(10), s, s};
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
    const float cy = L.top() + L.px(150);
    uiBatch_.rect(float(L.w) * 0.5f + L.px(3), cy + L.px(7), L.px(330), L.px(112), L.px(24), rgba(col::kShadow, u8(80 * a)), {0, 0, 0, 0}, 0,
                  L.px(8));
    uiBatch_.rect(float(L.w) * 0.5f, cy, L.px(330), L.px(112), L.px(24), rgba(ui::kCard, u8(250 * a)), rgba(col::kScarlet, u8(255 * a)), L.px(3));
    uiBatch_.text("PAPERMARCH", float(L.w) * 0.5f, cy - L.px(14), L.px(46), rgba(col::kScarlet, u8(255 * a)), {0, 0, 0, 0}, 0, Align::Center);
    uiBatch_.text("TAP A CARD, BEAT THE WAVES", float(L.w) * 0.5f, cy + L.px(30), L.px(15), rgba(ui::kText, u8(255 * a)), {0, 0, 0, 0}, 0,
                  Align::Center);
    drawUpdatePill(cy + L.px(70));
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
    // Top: the level chip (it opens LEVELS) with the level's saved stars, the objective, pause.
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
        uiBatch_.star5(chipX + chipW + L.px(20) + float(k) * L.px(24), top + L.px(28), L.px(10.5f),
                       rgba(k < got ? col::kGold : 0x8A6244), rgba(col::kShadow, 90), L.px(1));
    const Rect pr = pauseRect(L);
    const char* obj = def_->boss ? nullptr : sim::objective(*def_);
    char bossLine[48];
    if (def_->boss) {
        std::snprintf(bossLine, sizeof bossLine, "BOSS: %s", bossName(def_->world));
        obj = bossLine;
    }
    {
        const float size = L.px(14);
        const float w = UiBatch::textWidth(obj, size) + L.px(26);
        const float left = chipX + chipW + L.px(94);
        const float x = maxv(left + w * 0.5f, minv(pr.x - L.px(10) - w * 0.5f, float(L.w) * 0.5f + L.px(70)));
        if (x - w * 0.5f >= left - L.px(2)) {
            uiBatch_.rect(x, top + L.px(28), w, L.px(34), L.px(17), rgba(col::kInk, 235));
            uiBatch_.text(obj, x, top + L.px(28), size, rgba(col::kGold), {0, 0, 0, 0}, 0, Align::Center);
        }
    }
    if (ui_.button("#PAUSE", pr, Ui::kRound) && playing) {
        feedback_.uiTap();
        setScreen(Screen::Pause);
        return;
    }
    pauseGlyph(uiBatch_, pr, L, false);
    // Under the top row: the waves (GAME_DESIGN 11): beaten ones pale, the next one scarlet, the rest ink; the castle last.
    float lineY = top + L.px(80);
    if (def_->raidCount > 0) {
        const sim::State& sh = scene_.shown();
        const float size = L.px(13), h = L.px(28), gap = L.px(5);
        char chips[sim::kMaxRaids + 1][24];
        float widths[sim::kMaxRaids + 1], total = 0;
        const int n = def_->raidCount;
        for (int r = 0; r <= n; ++r) {
            if (r < n) formatHost(def_->raids[r].strength, chips[r], sizeof chips[r]);
            else if (def_->keep() >= 0) {
                char v[16];
                formatHost(sim::number(*def_, sh, def_->keep()), v, sizeof v);
                std::snprintf(chips[r], sizeof chips[r], "CASTLE %s", v);
            } else {
                chips[r][0] = 0;
            }
            widths[r] = chips[r][0] ? UiBatch::textWidth(chips[r], size) + L.px(18) : 0;
            total += widths[r] + (chips[r][0] ? gap : 0);
        }
        const float label = UiBatch::textWidth("WAVES", size) + L.px(8);
        total += label;
        const float avail = float(L.w) - L.safe.left - L.safe.right - L.px(24);
        const float k = total > avail ? avail / total : 1.0f;
        float x = float(L.w) * 0.5f - total * k * 0.5f;
        uiBatch_.text("WAVES", x, lineY, size * k, rgba(col::kIvory), {0, 0, 0, 0}, 0, Align::Left);
        x += label * k;
        for (int r = 0; r <= n; ++r) {
            if (!chips[r][0]) continue;
            const float w = widths[r] * k;
            const bool done = r < sh.raidNext, next = r == sh.raidNext;
            const u32 bg = r == n ? col::kInk : done ? 0xD8CCB4 : next ? col::kScarlet : col::kInk;
            const u32 fg = r == n ? col::kGold : done ? 0x9A8A74 : next ? col::kWhite : col::kGold;
            const float pulse = next ? 1.0f + 0.06f * std::sin(screenTime_ * 6.0f) : 1.0f;
            uiBatch_.rect(x + w * 0.5f, lineY, w * pulse, h * k * pulse, h * k * 0.5f, rgba(bg, 240), rgba(col::kShadow, 60), L.px(1));
            uiBatch_.text(chips[r], x + w * 0.5f, lineY, size * k, rgba(fg), {0, 0, 0, 0}, 0, Align::Center);
            if (done) uiBatch_.rect(x + w * 0.5f, lineY, w - L.px(10), L.px(2), L.px(1), rgba(0x9A8A74));
            x += w + gap * k;
        }
        lineY += L.px(40);
    }
    if (deadEnd_ && !scene_.busy() && !st_.over()) {
        const char* s = "NO WAY THROUGH FROM HERE";
        const float w = UiBatch::textWidth(s, L.px(16)) + L.px(30);
        uiBatch_.rect(float(L.w) * 0.5f, lineY, w, L.px(36), L.px(18), rgba(col::kScarlet, 240));
        uiBatch_.text(s, float(L.w) * 0.5f, lineY, L.px(16), rgba(col::kWhite), {0, 0, 0, 0}, 0, Align::Center);
    } else if (hintLineTime_ > 0 && hintShownFor_ > 0) {
        const float a = minv(1.0f, hintLineTime_ / 0.4f);
        const char* s = sim::hintText(hintShownFor_);
        const float w = UiBatch::textWidth(s, L.px(16)) + L.px(30);
        uiBatch_.rect(float(L.w) * 0.5f, lineY, w, L.px(36), L.px(18), rgba(col::kInk, u8(235 * a)), rgba(col::kGold, u8(200 * a)), L.px(1.5f));
        uiBatch_.text(s, float(L.w) * 0.5f, lineY, L.px(16), rgba(col::kWhite, u8(255 * a)), {0, 0, 0, 0}, 0, Align::Center);
    }
    // Bottom: UNDO, the host plate, HINT.
    const float rowH = L.px(64), y = L.bottom() - L.px(14) - rowH;
    const float sideW = L.px(104);
    const Rect undoR{L.safe.left + L.px(12), y, sideW, rowH};
    const Rect hintR{float(L.w) - L.safe.right - L.px(12) - sideW, y, sideW, rowH};
    if (deadEnd_ && !scene_.busy()) {  // UNDO pulses gold
        const float p = 0.5f + 0.5f * std::sin(screenTime_ * 6.0f);
        uiBatch_.rect(undoR.cx(), undoR.cy(), undoR.w + L.px(10), undoR.h + L.px(10), L.px(22), {0, 0, 0, 0}, rgba(col::kGold, u8(255 * p)),
                      L.px(4));
    }
    if (ui_.button("UNDO", undoR) && (playing || screen_ == Screen::Lose)) {
        feedback_.uiTap();
        undo();
    }
    if (ui_.button("HINT", hintR, Ui::kPrimary, 0.82f) && playing) {
        feedback_.uiTap();
        useHint();
    }
    const float plateW = minv(L.px(184), hintR.x - (undoR.x + undoR.w) - L.px(24));
    const Rect plate{float(L.w) * 0.5f - plateW * 0.5f, y - L.px(10), plateW, rowH + L.px(20)};
    uiBatch_.rect(plate.cx() + L.px(2), plate.cy() + L.px(5), plate.w, plate.h, L.px(22), rgba(col::kShadow, 80), {0, 0, 0, 0}, 0, L.px(5));
    uiBatch_.rect(plate.cx(), plate.cy(), plate.w, plate.h, L.px(22), rgba(col::kIvory), rgba(col::kScarlet), L.px(3.5f));
    uiBatch_.text("YOUR ARMY", plate.cx(), plate.y + L.px(18), L.px(13), rgba(ui::kText), {0, 0, 0, 0}, 0, Align::Center);
    char host[32];
    formatHost(i64(scene_.shownHost() + 0.5f), host, sizeof host);
    float hs = L.px(40);
    while (UiBatch::textWidth(host, hs) > plate.w - L.px(16) && hs > L.px(18)) hs *= 0.92f;
    uiBatch_.text(host, plate.cx(), plate.y + plate.h * 0.62f, hs, rgba(col::kScarlet), {0, 0, 0, 0}, 0, Align::Center);
    ui_.note(plate, "#HOST");
}

void Game::drawPause() {
    const Layout& L = layout_;
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(col::kShadow, 110));
    const Rect card = centered(L, float(L.h) * 0.48f, 300, 400);
    ui_.panel(card, 26);
    ui_.text("PAUSED", float(L.w) * 0.5f, card.y + L.px(46), 34, col::kScarlet);
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
    const float appear = tune.winPanelDelay * 0.35f;
    if (resultTime_ < appear) return;
    const float t = resultTime_ - appear;
    const float in = minv(1.0f, t / 0.25f);
    ui_.opacity = in;
    const Rect card = centered(L, L.bottom() - L.px(235) + (1.0f - in) * L.px(40), 330, 300);
    ui_.panel(card, 26);
    const bool perfect = stars_ >= 3;
    ui_.text(perfect ? "PERFECT!" : "MAP TAKEN!", float(L.w) * 0.5f, card.y + L.px(40), 32, perfect ? col::kGoldDark : col::kScarlet);
    for (int k = 0; k < 3; ++k) {
        const float landAt = 0.25f + 0.28f * float(k);
        const float x = float(L.w) * 0.5f + float(k - 1) * L.px(70);
        const float y = card.y + L.px(96) - (k == 1 ? L.px(10) : 0.0f);
        uiBatch_.star5(x, y, L.px(29), rgba(0xD8C8A8, u8(255 * in)), rgba(col::kShadow, u8(60 * in)), L.px(1.5f));
        if (k < stars_ && t >= landAt) {
            const float s = minv(1.0f, (t - landAt) / 0.18f);
            const float pop = s < 1.0f ? 1.35f - 0.35f * s : 1.0f;
            uiBatch_.star5(x, y, L.px(27) * pop, rgba(col::kGold), rgba(col::kGoldDark), L.px(2.5f));
            if (starsLanded_ <= k) {
                starsLanded_ = k + 1;
                feedback_.star(k, double(resultTime_));
                if (k == stars_ - 1 && perfect) scene_.celebrate(true);
            }
        }
    }
    char a[24], b[24], line[64];
    formatHost(st_.s, a, sizeof a);
    formatHost(def_->best, b, sizeof b);
    std::snprintf(line, sizeof line, "ARMY %s   BEST %s", a, b);
    ui_.text(line, float(L.w) * 0.5f, card.y + L.px(150), 17, ui::kText);
    const bool last = level_ >= sim::levelCount();
    if (ui_.button(last ? "FINISH" : "NEXT", centered(L, card.y + L.px(206), 250, 66), Ui::kPrimary, 1.1f)) {
        feedback_.uiTap();
        if (last) setScreen(Screen::End);
        else enterLevel(level_ + 1, true);
        ui_.opacity = 1;
        return;
    }
    if (!perfect && ui_.button("RETRY", centered(L, card.y + L.px(266), 150, 48), Ui::kSecondary, 0.85f)) {
        feedback_.uiTap();
        restart();
    }
    ui_.opacity = 1;
}

void Game::drawLose() {
    const Layout& L = layout_;
    if (resultTime_ < 0.2f) return;
    const float in = minv(1.0f, (resultTime_ - 0.2f) / 0.25f);
    ui_.opacity = in;
    const Rect card = centered(L, L.bottom() - L.px(215), 330, 250);
    ui_.panel(card, 26);
    const char* why = st_.reason == sim::kLossRaid ? "THE WAVE BROKE THROUGH"
                      : st_.reason == sim::kLossRival ? "THE WARLORD WON"
                                                      : "TOO WEAK TO GO ON";
    ui_.text(why, float(L.w) * 0.5f, card.y + L.px(40), 20, col::kScarletDark);
    ui_.text("UNDO A MOVE OR TRY AGAIN", float(L.w) * 0.5f, card.y + L.px(74), 14, ui::kMuted);
    if (ui_.button("UNDO", centered(L, card.y + L.px(136), 240, 64), Ui::kPrimary)) {
        feedback_.uiTap();
        undo();
        ui_.opacity = 1;
        return;
    }
    if (ui_.button("RETRY", centered(L, card.y + L.px(206), 170, 52))) {
        feedback_.uiTap();
        restart();
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
    ui_.text("SETTINGS", float(L.w) * 0.5f, y, 30, col::kScarlet);
    const bool roomy = usable / L.dp > 740;
    const float rowH = roomy ? 54.0f : 50.0f, gap = roomy ? 12.0f : 8.0f;
    y += L.px(54);
    bool changed = false;
    struct Sw {
        const char* name;
        bool* v;
    } sws[] = {{"SOUND", &save_.sound}, {"MUSIC", &save_.music}, {"HAPTICS", &save_.haptics}, {"FAST MOVES", &save_.fast},
               {"REDUCE EFFECTS", &save_.reduceEffects}};
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
            save_.fast = keep.fast;
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
    ui_.text("LEVELS", float(L.w) * 0.5f, L.top() + L.px(38), 30, col::kScarlet);
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
        setScreen(Screen::Play);
    }
}

void Game::drawEnd() {
    const Layout& L = layout_;
    uiBatch_.rect(float(L.w) * 0.5f, float(L.h) * 0.5f, float(L.w), float(L.h), 0, rgba(col::kShadow, 120));
    const Rect card = centered(L, float(L.h) * 0.5f, 330, 360);
    ui_.panel(card, 26);
    char buf[48];
    std::snprintf(buf, sizeof buf, "ALL %d MAPS", sim::levelCount());
    ui_.text(buf, float(L.w) * 0.5f, card.y + L.px(48), 30, col::kScarlet);
    ui_.text("TAKEN!", float(L.w) * 0.5f, card.y + L.px(86), 30, col::kScarlet);
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

// ---- The flow test (TECH_SPEC 9.4) ----------------------------------------------------------------------------------

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
    auto settled = [&] { return !scene_.busy() && pendingLevel_ == 0 && turnT_ < 0.0f; };
    switch (flowStep_) {
        case 1:
            if (flowWait_ < 10) break;
            check(screen_ == Screen::Play && level_ == 1, "starts playing level 1");
            next();
            break;
        case 2: {  // a tap on a gold site takes it
            const u32 moves = sim::takeable(*def_, st_);
            int site = -1;
            for (int i = 1; i < def_->siteCount && site < 0; ++i)
                if (moves & bit(i)) site = i;
            float x = 0, y = 0;
            scene_.siteScreen(site, x, y);
            pointer({PtrType::Down, 5, x, y, 0, 0});
            pointer({PtrType::Up, 5, x, y, 0, 0});
            check(undoCount_ == 1, "a tap on a gold camp takes it");
            next();
            break;
        }
        case 3:  // the bot finishes the level
            useBot_ = true;
            std::snprintf(args_.bot, sizeof args_.bot, "best");
            if (screen_ == Screen::Win) {
                useBot_ = false;
                check(stars_ == 3, "the best line wins three stars");
                next();
            }
            timeout(60 * 30, "level 1 is won within 30 s");
            break;
        case 4:
            if (resultTime_ < 1.5f) break;
            check(press("NEXT"), "the win panel has NEXT");
            next();
            break;
        case 5:
            if (!settled()) break;
            check(level_ == 2 && screen_ == Screen::Play, "NEXT plays level 2");
            next();
            break;
        case 6:
            if (flowWait_ < 3) break;
            check(press("LEVEL 2"), "the level chip is a button");
            next();
            break;
        case 7:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Levels, "the level chip opens LEVELS");
            check(press("4"), "LEVELS shows level 4");
            next();
            break;
        case 8: {
            if (!settled() || flowWait_ < 3) break;
            check(level_ == 4 && screen_ == Screen::Play, "a level's button starts it");
            // Level 4: doubling first leaves the army one short of the first wave.
            check(makeMove(1), "x2 can be taken first");
            next();
            break;
        }
        case 9:
            if (screen_ == Screen::Lose) {
                check(st_.reason == sim::kLossRaid, "the wave breaks through a weak army");
                next();
            }
            timeout(60 * 10, "a lost wave shows LOSE");
            break;
        case 10:
            if (resultTime_ < 0.8f) break;
            check(press("UNDO"), "LOSE offers UNDO");
            next();
            break;
        case 11:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Play && undoCount_ == 0 && !st_.over() && !deadEnd_, "UNDO takes the move back");
            next();
            break;
        case 12:
            check(press("HINT"), "HINT is a button");
            next();
            break;
        case 13:
            if (flowWait_ < 3) break;
            check(hinted_, "HINT marks the level hinted");
            next();
            break;
        case 14:
            useBot_ = true;
            next();
            break;
        case 15:
            if (screen_ == Screen::Win) {
                useBot_ = false;
                check(stars_ <= 2, "a hint caps the stars at two");
                next();
            }
            timeout(60 * 30, "level 4 is won after a hint");
            break;
        case 16:
            if (resultTime_ < 1.5f) break;
            check(press("RETRY"), "under three stars the win panel offers RETRY");
            next();
            break;
        case 17:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Play && level_ == 4 && undoCount_ == 0 && !hinted_, "RETRY restarts the level");
            check(press("#PAUSE"), "the pause button");
            next();
            break;
        case 18:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Pause, "pause opens PAUSE");
            check(press("SETTINGS"), "PAUSE has SETTINGS");
            next();
            break;
        case 19: {
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Settings, "SETTINGS is open");
            const bool before = save_.sound;
            press("SOUND");
            flowChecks_ += 0;
            next();
            (void)before;
            break;
        }
        case 20:
            if (flowWait_ < 3) break;
            check(!save_.sound, "SOUND switches the sound off");
            press("SOUND");
            next();
            break;
        case 21:
            if (flowWait_ < 3) break;
            check(save_.sound, "and on again");
            check(press("BACK"), "SETTINGS has BACK");
            next();
            break;
        case 22:
            if (flowWait_ < 3) break;
            check(screen_ == Screen::Pause, "BACK returns to PAUSE");
            check(press("RESUME"), "PAUSE has RESUME");
            next();
            break;
        case 23:
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
