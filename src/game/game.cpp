// The game: levels and their battles, input, the save, and the frame's assembly.
#include "game/game.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "build_stamp.h"
#include "core/clock.h"
#include "core/log.h"
#include "core/tuning.h"
#include "game/refresh.h"
#include "platform/platform.h"
#include "sim/bot.h"

namespace pm {

const char* screenName(Screen s) {
    switch (s) {
        case Screen::Play: return "PLAY";
        case Screen::Pause: return "PAUSE";
        case Screen::Win: return "WIN";
        case Screen::Lose: return "LOSE";
        case Screen::Settings: return "SETTINGS";
        case Screen::Levels: return "LEVELS";
        case Screen::End: return "END";
    }
    return "?";
}

bool Game::init(const Args& args, const GameEnv& env) {
    args_ = args;
    env_ = env;
    layout_ = {env.frameW, env.frameH, env.dpScale, env.safe};
    field_.init(layout_);

    // The save lives in the preferences path; pictures, tests and benchmarks never touch it.
    saveEnabled_ = !args.fresh && !args.picture[0] && std::strncmp(args.scenario, "bench_", 6) && !args.bot[0] && !args.flowTest;
    if (char* pref = SDL_GetPrefPath("papermarch", "papermarch")) {
        std::snprintf(savePath_, sizeof savePath_, "%ssave.txt", pref);
        SDL_free(pref);
    }
    if (saveEnabled_ && savePath_[0]) {
        platform::restoreFile(savePath_);  // a browser: the save kept in localStorage
        if (!loadSave(savePath_, save_)) PM_LOG("save: none or damaged at %s; defaults", savePath_);
        else PM_LOG("save: level %d, %d stars", save_.level, save_.totalStars());
    }
    if (args.reduceEffects) save_.reduceEffects = true;
    {
        const double t0 = nowSeconds();
        mixer_.init(48000);
        PM_LOG("audio: sound bank built in %.0f ms", (nowSeconds() - t0) * 1000.0);
    }
    feedback_.init(&mixer_);
    applySettings();
    if (save_.refresh60 && env.refreshHz > 61.0f && args.fps <= 0.0f) {
        platform::requestRefreshRate(60.0f);
        env_.refreshHz = 60.0f;
    }
    const int count = sim::levelCount();
    PM_LOG("levels: %d", count);
    if (count <= 0) return false;

    if (!std::strcmp(args.scenario, "sim_hash")) {
        // The determinism check on this device: every level played by the bot; the squads it ends with, hashed.
        u64 h = 1469598103934665603ULL;
        for (int n = 1; n <= count; ++n) {
            i32 left = 0;
            sim::botPlay(sim::level(n), &left);
            h = (h ^ u64(u32(left))) * 1099511628211ULL;
        }
        PM_TAG("PMSIM", "tablehash %016llx over %d levels", (unsigned long long)h, count);
        quit_ = true;
        return true;
    }
    if (args.picture[0]) {
        mode_ = Mode::Picture;
        coldStart_ = false;
        if (!setupPicture(args.picture)) {
            PM_ERR("unknown picture %s", args.picture);
            return false;
        }
        return true;
    }
    if (!std::strncmp(args.scenario, "bench_", 6)) {
        mode_ = Mode::Bench;
        coldStart_ = false;
        level_ = args.level > 0 ? args.level : (!std::strcmp(args.scenario, "bench_battle") ? 30 : 1);
        loadLevel(level_);
        useBot_ = std::strcmp(args.scenario, "bench_idle") != 0;
        return true;
    }
    mode_ = Mode::Play;
    level_ = clampv(args.level > 0 ? args.level : save_.level, 1, count);
    loadLevel(level_);
    if (args.bot[0]) {
        useBot_ = true;
        coldStart_ = false;
    }
    if (args.flowTest) {
        coldStart_ = false;
        flowStep_ = 1;
    }
    return true;
}

bool Game::initGraphics() {
    if (!paper_.init(env_.frameW, env_.frameH)) return false;
    graphicsReady_ = true;
    checkGl("Game::initGraphics");
    return true;
}

void Game::destroyGraphics() {
    paper_.destroy();
    graphicsReady_ = false;
}

void Game::resize(const GameEnv& env) {
    env_ = env;
    if (save_.refresh60 && args_.fps <= 0.0f) env_.refreshHz = minv(env_.refreshHz, 60.0f);
    layout_ = {env.frameW, env.frameH, env.dpScale, env.safe};
    field_.resize(layout_);
    if (graphicsReady_) paper_.resize(env.frameW, env.frameH);
}

void Game::applySettings() {
    mixer_.setEnabled(save_.sound, save_.music);
    feedback_.setEnabled(save_.sound, save_.haptics);
    field_.reduceEffects = save_.reduceEffects;
}

void Game::saveNow() {
    if (!saveEnabled_ || !savePath_[0]) return;
    if (!writeSave(savePath_, save_)) PM_WARN("save: could not write %s", savePath_);
    else platform::persistFile(savePath_);
}

// ---- Levels --------------------------------------------------------------------------------------------------------------

void Game::loadLevel(int n) {
    level_ = clampv(n, 1, sim::levelCount());
    def_ = &sim::level(level_);
    battle_.init(*def_);
    go_ = false;
    steerX_ = 0;
    steerDown_ = false;
    stars_ = starsLanded_ = 0;
    newBest_ = false;
    endTime_ = -1;
    levelTime_ = 0;
    levelFrames_ = levelLate_ = 0;
    field_.setLevel(*def_, level_);
    mixer_.setWorld(def_->world, def_->boss != 0);
    setScreen(Screen::Play);
    // A level's hint line, once.
    hintLineTime_ = 0;
    hintShownFor_ = 0;
    if (def_->hint > 0 && def_->hint < 32 && !(save_.hints & (1u << def_->hint))) {
        hintShownFor_ = def_->hint;
        hintLineTime_ = 5.0f;
        save_.hints |= 1u << def_->hint;
    }
    int enemies = 0;
    for (int w = 0; w < def_->waveCount; ++w) enemies += def_->waves[w].count;
    PM_TAG("PMLIFE", "level %d (world %d, start %d, %d gates, %d barrels, %d enemies, boss %d)", level_, def_->world, def_->start,
           def_->gateCount, def_->barrelCount, enemies, def_->bossHp);
}

void Game::enterLevel(int n, bool turn) {
    pendingLevel_ = clampv(n, 1, sim::levelCount());
    pendingTurn_ = turn && graphicsReady_;
}

void Game::restart() {
    battle_.init(*def_);
    go_ = false;
    steerX_ = 0;
    steerDown_ = false;
    stars_ = starsLanded_ = 0;
    newBest_ = false;
    endTime_ = -1;
    levelTime_ = 0;
    field_.setLevel(*def_, level_);
    setScreen(Screen::Play);
}

void Game::finish(bool won) {
    endTime_ = 0;
    if (won) {
        stars_ = sim::starsFor(*def_, battle_.squad);
        const int idx = level_ - 1;
        if (idx >= 0 && idx < int(sizeof save_.stars)) {
            newBest_ = stars_ > save_.stars[idx];
            if (newBest_) save_.stars[idx] = u8(stars_);
        }
        const int count = sim::levelCount();
        if (level_ >= count) save_.finished = true;
        save_.level = level_ < count ? level_ + 1 : count;
        saveNow();
        PM_TAG("PMLIFE", "won level %d with %d (peak %d; 2 stars at %d, 3 at %d): %d stars in %.1f s", level_, battle_.squad, battle_.peak,
               def_->star2, def_->star3, stars_, double(battle_.time));
    } else {
        PM_TAG("PMLIFE", "lost level %d at %.0f%% of the road (peak %d) in %.1f s", level_, double(battle_.progress() * 100.0f), battle_.peak,
               double(battle_.time));
    }
}

int Game::musicLayers() const {
    if (!def_) return 1;
    if (battle_.won) return 5;
    if (!battle_.started || battle_.lost) return 1;
    if (battle_.bossIndex() >= 0) return 5;
    if (battle_.aliveEnemies() > 0) return 4;
    return battle_.progress() > 0.45f ? 3 : 2;
}

void Game::stepBattle(float steer, bool go) {
    if (go && !battle_.started) {
        CueEvent c;
        c.cue = kCueStart;
        feedback_.cue(c, nowSeconds());
    }
    sim::BattleEvents ev;
    battle_.tick(steer, go, ev);
    field_.events(battle_, ev);
    if (ev.won || ev.lostAll) finish(ev.won);
}

// ---- Input -----------------------------------------------------------------------------------------------------------

void Game::pointer(const PointerEvent& e) {
    if (mode_ != Mode::Play) return;
    switch (e.type) {
        case PtrType::Down: {
            if (uiFingerDown_ || steerDown_) break;
            if (ui_.pointerDown(e.id, e.x, e.y)) {
                uiFinger_ = e.id;
                uiFingerDown_ = true;
                break;
            }
            if (coldStart_) coldStart_ = false;
            if (screen_ != Screen::Play || pendingLevel_ || turnT_ >= 0.0f || battle_.won || battle_.lost) break;
            steerFinger_ = e.id;
            steerDown_ = true;
            downX_ = e.x;
            steerAnchor_ = steerX_;
            go_ = true;
            break;
        }
        case PtrType::Move:
            if (uiFingerDown_ && e.id == uiFinger_) ui_.pointerMove(e.id, e.x, e.y);
            if (steerDown_ && e.id == steerFinger_)
                steerX_ = clampv(steerAnchor_ + (e.x - downX_) * field_.unitsPerPixel() * tune.steer, -sim::kRoadHalf, sim::kRoadHalf);
            break;
        case PtrType::Up:
            if (uiFingerDown_ && e.id == uiFinger_) {
                ui_.pointerUp(e.id, e.x, e.y);
                uiFingerDown_ = false;
            }
            if (steerDown_ && e.id == steerFinger_) steerDown_ = false;
            break;
    }
}

void Game::key(Key k, bool down) {
    if (k == Key::Left) keyLeft_ = down;
    if (k == Key::Right) keyRight_ = down;
    if (!down) return;
    if ((k == Key::Left || k == Key::Right || k == Key::Fire) && screen_ == Screen::Play) go_ = true;
    if (k == Key::Back) {
        if (mode_ != Mode::Play) {
            quit_ = true;
            return;
        }
        switch (screen_) {
            case Screen::Play: setScreen(Screen::Pause); break;
            case Screen::Pause: quit_ = true; break;  // back again leaves the game (it is saved)
            case Screen::Settings: setScreen(settingsReturn_); break;
            case Screen::Levels: setScreen(battle_.won || battle_.lost ? (battle_.won ? Screen::Win : Screen::Lose) : Screen::Play); break;
            case Screen::Win: enterLevel(level_ + 1, true); break;
            case Screen::Lose: restart(); break;
            case Screen::End: setScreen(Screen::Play); break;
        }
    } else if (k == Key::Pause && screen_ == Screen::Play) {
        setScreen(Screen::Pause);
    }
}

void Game::background() {
    PM_TAG("PMLIFE", "game: paused");
    if (mode_ == Mode::Play && screen_ == Screen::Play && battle_.started && !battle_.won && !battle_.lost) setScreen(Screen::Pause);
    uiFingerDown_ = steerDown_ = false;
    keyLeft_ = keyRight_ = false;
    saveNow();
}

void Game::foreground() { PM_TAG("PMLIFE", "game: resumed on %s", screenName(screen_)); }

// ---- The frame -------------------------------------------------------------------------------------------------------

void Game::tick() {
    const double t0 = nowSeconds();
    ++pictureTick_;
    if (flowStep_ > 0) flowStep();
    const bool live = def_ && screen_ == Screen::Play && !pendingLevel_ && turnT_ < 0.0f;
    if (live && !battle_.won && !battle_.lost) {
        if (useBot_ || mode_ != Mode::Play) {
            go_ = mode_ != Mode::Bench || std::strcmp(args_.scenario, "bench_idle") != 0;
            stepBattle(sim::botSteer(battle_), go_);
        } else {
            if (keyLeft_ != keyRight_) steerX_ = clampv(steerX_ + (keyRight_ ? 9.0f : -9.0f) * sim::kDt, -sim::kRoadHalf, sim::kRoadHalf);
            stepBattle(steerX_, go_);
        }
    } else if (live) {
        stepBattle(battle_.x, false);  // the battle is over: the view keeps its last step
    }
    // Bots and benchmarks go on by themselves.
    if ((useBot_ || mode_ == Mode::Bench) && def_ && !pendingLevel_ && turnT_ < 0.0f && flowStep_ == 0) {
        if (screen_ == Screen::Win && resultTime_ > 1.6f) {
            ++benchLevels_;
            const int next = level_ >= sim::levelCount() ? 1 : level_ + 1;
            enterLevel(!std::strcmp(args_.scenario, "bench_battle") ? level_ : next, true);
        } else if (screen_ == Screen::Lose && resultTime_ > 1.0f) {
            restart();
        }
    }
    lastTick_ = float(nowSeconds() - t0);
}

void Game::setScreen(Screen s) {
    if (s != screen_ && mode_ == Mode::Play) PM_TAG("PMLIFE", "screen %s -> %s", screenName(screen_), screenName(s));
    screen_ = s;
    screenTime_ = 0;
    resultTime_ = 0;
    confirmReset_ = false;
}

void Game::update(float dt, float alpha) {
    // A level change: the old page is captured as it was last drawn, then the new level set up.
    if (pendingLevel_ > 0) {
        const int n = pendingLevel_;
        pendingLevel_ = 0;
        if (pendingTurn_ && frame_.stickers && graphicsReady_) {
            paper_.snapshot(frame_);
            turnT_ = 0;
            feedback_.pageTurn();
        } else {
            turnT_ = -1;
        }
        loadLevel(n);
    }
    if (turnT_ >= 0.0f) {
        turnT_ += dt / maxv(0.05f, tune.turnSeconds * (save_.reduceEffects ? 0.5f : 1.0f));
        if (turnT_ >= 1.0f) turnT_ = -1;
    }
    paper_.setTurn(turnT_, save_.reduceEffects, col::kCream);
    screenTime_ += dt;
    resultTime_ += dt;
    levelTime_ += dt;
    hintLineTime_ = maxv(0.0f, hintLineTime_ - (go_ ? dt : 0.0f));
    if (!coldStart_) titleFade_ = maxv(0.0f, titleFade_ - dt * 3.0f);
    const bool frozen = screen_ == Screen::Pause || screen_ == Screen::Settings || screen_ == Screen::Levels;
    if (!frozen) field_.update(dt, battle_);
    // The battle's end: the panel after a beat.
    if (endTime_ >= 0.0f) {
        const float before = endTime_;
        endTime_ += dt;
        if (battle_.won && before < 0.35f && endTime_ >= 0.35f) field_.celebrate();
        if (screen_ == Screen::Play) {
            if (battle_.won && endTime_ >= tune.winPanelDelay) setScreen(Screen::Win);
            else if (battle_.lost && endTime_ >= 1.1f) setScreen(Screen::Lose);
        }
    }
    // The view's cues become sounds and haptics.
    {
        CueEvent cues[64];
        const int n = field_.takeCues(cues, 64);
        const double now = nowSeconds();
        for (int i = 0; i < n; ++i) feedback_.cue(cues[i], now);
        int hs[16];
        const int nh = feedback_.takeHaptics(hs, 16);
        for (int i = 0; i < nh; ++i) {
            platform::haptic(hs[i]);
            ++hapticsSent_;
            hapticRing_[hapticsSent_ % kHapticRing] = now;
            int inSecond = 0;
            for (double t : hapticRing_)
                if (now - t < 1.0) ++inSecond;
            hapticPeak_ = maxv(hapticPeak_, inSecond);
        }
    }
    mixer_.setLayers(save_.music ? musicLayers() : 0);
    // The UI, then the battle's frame.
    uiBatch_.clear();
    ui_.begin(&uiBatch_, layout_);
    drawUi(dt);
    ui_.end();
    if (args_.lint && mode_ == Mode::Picture) {
        lintRan_ = true;
        lintProblems_ = ui_.lint(layout_);
    }
    const bool moving = screen_ == Screen::Play && battle_.started && !battle_.won && !battle_.lost && turnT_ < 0.0f;
    if (def_) field_.build(frame_, labels_, battle_, moving ? alpha : 0.0f);
    frame_.ui = &uiBatch_;
    frame_.uiOpacity = 1.0f;
    if (args_.logSites) logSites();
}

void Game::frameShown(float intervalSeconds) {
    ++levelFrames_;
    if (lateFrame(intervalSeconds, env_.refreshHz)) ++levelLate_;
}

void Game::render(unsigned fbo, int w, int h) {
    if (!graphicsReady_) return;
    paper_.draw(frame_, fbo, w, h);
}

// The browser check (tools/web_check.mjs) drags from what this logs: where to touch to march, and NEXT.
void Game::logSites() {
    if (!def_ || turnT_ >= 0.0f) return;
    const int key = level_ * 100 + int(screen_) * 2 + (battle_.started ? 1 : 0);
    if (key == sitesLogged_) return;
    sitesLogged_ = key;
    char line[160];
    if (screen_ == Screen::Win) {
        const UiControl* c = ui_.find("NEXT");
        if (!c) {
            sitesLogged_ = -1;  // the panel is not up yet
            return;
        }
        std::snprintf(line, sizeof line, "PMSITES NEXT:%d,%d", int(c->r.cx()), int(c->r.cy()));
    } else if (screen_ == Screen::Play && !battle_.started) {
        std::snprintf(line, sizeof line, "PMSITES DRAG:%d,%d", int(field_.squadScreenX()), int(field_.squadScreenY()));
    } else {
        return;
    }
    PM_TAG("PMLIFE", "%s", line);
}

}  // namespace pm
