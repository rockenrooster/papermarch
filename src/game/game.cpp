// The game (TECH_SPEC 8): levels, moves, undo, hints, the save, input, and the frame's assembly.
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
#include "sim/levels_hand.h"

namespace pm {

namespace {
inline u32 bit(int i) { return 1u << u32(i); }
}  // namespace

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
    scene_.init(layout_);

    // The save lives in the preferences path; pictures, tests and benchmarks never touch it.
    saveEnabled_ = !args.fresh && !args.picture[0] && std::strncmp(args.scenario, "bench_", 6) && !args.bot[0];
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
        mixer_.init(48000);  // synthesizes every sound (P4: measured on the Moto)
        PM_LOG("audio: sound bank built in %.0f ms", (nowSeconds() - t0) * 1000.0);
    }
    feedback_.init(&mixer_);
    applySettings();
    if (save_.refresh60 && env.refreshHz > 61.0f && args.fps <= 0.0f) {
        platform::requestRefreshRate(60.0f);
        env_.refreshHz = 60.0f;
    }
    solver_ = new sim::Solver();
    const int count = sim::levelCount();
    PM_LOG("levels: %d", count);
    if (count <= 0) return false;

    if (!std::strcmp(args.scenario, "sim_hash")) {
        // The determinism hash on this device (TECH_SPEC 3.6): every level's best, the same as the PC's.
        mode_ = Mode::Hash;
        u64 h = 1469598103934665603ULL;
        for (int n = 1; n <= count; ++n) {
            const sim::LevelDef& d = sim::level(n);
            const sim::Solve s = solver_->run(d, sim::start(d));
            h = (h ^ u64(s.best)) * 1099511628211ULL;
            h = (h ^ u64(s.first + 1)) * 1099511628211ULL;
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
        level_ = args.level > 0 ? args.level : (!std::strcmp(args.scenario, "bench_battle") ? 12 : 1);
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
    if (args.moves[0]) applyScriptedMoves(args.moves);
    if (args.flowTest) {
        coldStart_ = false;
        flowStep_ = 1;
    }
    return true;
}

bool Game::initGraphics() {
    if (!paper_.init(env_.frameW, env_.frameH)) return false;
    graphicsReady_ = true;
    if (def_) scene_.bake(paper_);
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
    scene_.resize(layout_);
    if (graphicsReady_) {
        paper_.resize(env.frameW, env.frameH);
        scene_.bake(paper_);
    }
}

void Game::applySettings() {
    mixer_.setEnabled(save_.sound, save_.music);
    feedback_.setEnabled(save_.sound, save_.haptics);
    scene_.reduceEffects = save_.reduceEffects;
}

void Game::saveNow() {
    if (!saveEnabled_ || !savePath_[0]) return;
    if (!writeSave(savePath_, save_)) PM_WARN("save: could not write %s", savePath_);
    else platform::persistFile(savePath_);
}

// ---- Levels and moves ------------------------------------------------------------------------------------------------

void Game::loadLevel(int n) {
    level_ = clampv(n, 1, sim::levelCount());
    def_ = &sim::level(level_);
    st_ = sim::start(*def_);
    undoCount_ = 0;
    hinted_ = false;
    deadEnd_ = false;
    stars_ = starsLanded_ = 0;
    newBest_ = false;
    levelTime_ = 0;
    levelFrames_ = levelLate_ = 0;
    scene_.levelNumber = level_;
    scene_.setLevel(*def_, level_, st_);
    mixer_.setWorld(def_->world, def_->boss != 0);
    afterMove();
    setScreen(Screen::Play);
    // A level's hint line, once (GAME_DESIGN 8.1).
    hintLineTime_ = 0;
    hintShownFor_ = 0;
    if (def_->hint > 0 && def_->hint < 32 && !(save_.hints & (1u << def_->hint))) {
        hintShownFor_ = def_->hint;
        hintLineTime_ = 4.5f;
        save_.hints |= 1u << def_->hint;
    }
    PM_TAG("PMLIFE", "level %d (%s, world %d, %d sites, best %lld)", level_, sim::objective(*def_), def_->world, def_->siteCount,
           (long long)def_->best);
}

void Game::enterLevel(int n, bool turn) {
    pendingLevel_ = clampv(n, 1, sim::levelCount());
    pendingTurn_ = turn && graphicsReady_;
}

bool Game::makeMove(int move) {
    if (!def_ || st_.over()) return false;
    if (!(sim::takeable(*def_, st_) & bit(move))) return false;
    if (undoCount_ < kUndo) undo_[undoCount_++] = st_;
    sim::Events ev;
    sim::take(*def_, st_, move, &ev);
    scene_.play(st_, ev);
    scene_.clearHint();
    if (st_.won()) {
        stars_ = sim::starsFor(st_.s, def_->best, hinted_, int(tune.star2Pct));
        const int idx = level_ - 1;
        if (idx >= 0 && idx < int(sizeof save_.stars)) {
            newBest_ = stars_ > save_.stars[idx];
            if (newBest_) save_.stars[idx] = u8(stars_);
        }
        const int count = sim::levelCount();
        if (level_ >= count) save_.finished = true;
        save_.level = level_ < count ? maxv(save_.level, level_ + 1) : count;
        if (save_.level > level_ + 1) save_.level = level_ + 1;  // the next level to play is the one after this one
        saveNow();
        PM_TAG("PMLIFE", "won level %d: host %lld of best %lld, %d stars", level_, (long long)st_.s, (long long)def_->best, stars_);
    } else if (st_.lost()) {
        PM_TAG("PMLIFE", "lost level %d (reason %d)", level_, st_.reason);
    }
    afterMove();
    return true;
}

void Game::undo() {
    if (undoCount_ <= 0) return;
    st_ = undo_[--undoCount_];
    scene_.snap(st_);
    setScreen(Screen::Play);
    afterMove();
}

void Game::restart() {
    st_ = sim::start(*def_);
    undoCount_ = 0;
    hinted_ = false;
    scene_.snap(st_);
    setScreen(Screen::Play);
    afterMove();
}

void Game::afterMove() {
    deadEnd_ = false;
    if (!def_ || st_.over()) return;
    solve_ = solver_->run(*def_, st_);
    deadEnd_ = tune.deadNudge > 0 && solve_.proven && !solve_.winnable;
}

void Game::useHint() {
    if (!def_ || st_.over()) return;
    if (solve_.first < 0) return;
    hinted_ = true;
    if (solve_.first == sim::kRivalMove) {
        const int rs = sim::rivalSite(*def_, st_);
        scene_.ringHint(rs >= 0 ? rs : 0);
    } else {
        scene_.ringHint(solve_.first);
    }
}

int Game::musicLayers() const {
    if (!def_) return 1;
    if (st_.won()) return 5;
    const sim::State& s = scene_.shown();
    i64 goal = 0;
    const int k = def_->keep();
    if (k >= 0) goal = sim::number(*def_, s, k);
    else if (s.raidNext < def_->raidCount) goal = def_->raids[s.raidNext].strength;
    if (goal <= 0) return 5;
    const float r = scene_.shownHost() / float(goal);
    return 1 + (r >= tune.layer1) + (r >= tune.layer2) + (r >= tune.layer3) + (r >= tune.layer4);
}

int Game::botMove() {
    const u32 moves = sim::takeable(*def_, st_);
    if (!moves) return -1;
    if (!std::strcmp(args_.bot, "random")) {
        int list[32], n = 0;
        for (int m = 0; m <= sim::kRivalMove; ++m)
            if (moves & bit(m)) list[n++] = m;
        botRng_ = botRng_ * 1664525u + 1013904223u;
        return list[(botRng_ >> 8) % u32(n)];
    }
    if (!std::strcmp(args_.bot, "greedy")) {
        // The weakest camp first, else anything free.
        int best = -1;
        i64 bestV = 0;
        for (int m = 0; m < def_->siteCount; ++m) {
            if (!(moves & bit(m))) continue;
            const u8 k = def_->sites[m].kind;
            const bool camp = k == sim::kCamp || k == sim::kDepot || k == sim::kKey || k == sim::kKeep;
            const i64 v = camp ? sim::number(*def_, st_, m) : -1;
            if (best < 0 || v < bestV) {
                best = m;
                bestV = v;
            }
        }
        return best >= 0 ? best : sim::kRivalMove;
    }
    if (solve_.first >= 0) return solve_.first;
    for (int m = 0; m <= sim::kRivalMove; ++m)
        if (moves & bit(m)) return m;
    return -1;
}

// ---- Input -----------------------------------------------------------------------------------------------------------

void Game::pointer(const PointerEvent& e) {
    if (mode_ != Mode::Play) return;
    switch (e.type) {
        case PtrType::Down: {
            if (uiFingerDown_ || pageFingerDown_) break;
            if (ui_.pointerDown(e.id, e.x, e.y)) {
                uiFinger_ = e.id;
                uiFingerDown_ = true;
                break;
            }
            pageFinger_ = e.id;
            pageFingerDown_ = true;
            downX_ = e.x;
            downY_ = e.y;
            if (coldStart_) coldStart_ = false;
            if (screen_ != Screen::Play || pendingLevel_ || turnT_ >= 0.0f) break;
            const int site = scene_.hit(e.x, e.y);
            if (site < 0) break;
            if (scene_.movesQueued() >= 2) break;
            if (sim::takeable(*def_, st_) & bit(site)) {
                makeMove(site);
            } else if (site != sim::kRivalMove) {
                i64 need = 0;
                const int why = sim::whyNot(*def_, st_, site, &need);
                char msg[24];
                if (why == 2) {
                    char n[16];
                    formatThousands(need, n, sizeof n);
                    std::snprintf(msg, sizeof msg, "NEED %s", n);
                } else if (why == 5) {
                    std::snprintf(msg, sizeof msg, "ROAD LOCKED");
                } else if (why == 3) {
                    std::snprintf(msg, sizeof msg, "BURNT");
                } else if (why == 6) {
                    std::snprintf(msg, sizeof msg, "THE WARLORD'S");
                } else {
                    std::snprintf(msg, sizeof msg, "TOO FAR");
                }
                if (why != 4) scene_.refuse(site, msg);
            }
            break;
        }
        case PtrType::Move:
            if (uiFingerDown_ && e.id == uiFinger_) ui_.pointerMove(e.id, e.x, e.y);
            break;
        case PtrType::Up:
            if (uiFingerDown_ && e.id == uiFinger_) {
                ui_.pointerUp(e.id, e.x, e.y);
                uiFingerDown_ = false;
            }
            if (pageFingerDown_ && e.id == pageFinger_) pageFingerDown_ = false;
            break;
    }
}

void Game::key(Key k, bool down) {
    if (!down) return;
    if (k == Key::Back) {
        if (mode_ != Mode::Play) {
            quit_ = true;
            return;
        }
        switch (screen_) {
            case Screen::Play: setScreen(Screen::Pause); break;
            case Screen::Pause: quit_ = true; break;  // back again leaves the game (it is saved)
            case Screen::Settings: setScreen(settingsReturn_); break;
            case Screen::Levels: setScreen(Screen::Play); break;
            case Screen::Win: enterLevel(level_ + 1, true); break;
            case Screen::Lose: undo(); break;
            case Screen::End: setScreen(Screen::Play); break;
        }
    } else if (k == Key::Pause && screen_ == Screen::Play) {
        setScreen(Screen::Pause);
    }
}

void Game::background() {
    PM_TAG("PMLIFE", "game: paused");
    if (mode_ == Mode::Play && screen_ == Screen::Play) setScreen(Screen::Pause);
    uiFingerDown_ = pageFingerDown_ = false;
    saveNow();
}

void Game::foreground() { PM_TAG("PMLIFE", "game: resumed on %s", screenName(screen_)); }

// ---- The frame -------------------------------------------------------------------------------------------------------

void Game::tick() {
    const double t0 = nowSeconds();
    ++pictureTick_;
    if (flowStep_ > 0) flowStep();
    if ((useBot_ || mode_ == Mode::Bench) && def_ && !pendingLevel_ && turnT_ < 0.0f) {
        botWait_ -= 1.0f / 60.0f;
        if (screen_ == Screen::Play && !st_.over() && scene_.movesQueued() == 0 && botWait_ <= 0) {
            const int m = botMove();
            if (m >= 0) makeMove(m);
            botWait_ = mode_ == Mode::Bench ? 0.05f : 0.15f;
        } else if (screen_ == Screen::Win && resultTime_ > 1.6f) {
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
    (void)alpha;
    // A level change: the old page is captured as it was last drawn, then the new one is set up and baked.
    if (pendingLevel_ > 0) {
        const int n = pendingLevel_;
        pendingLevel_ = 0;
        if (pendingTurn_ && frame_.stickers) {
            paper_.snapshot(frame_);
            turnT_ = 0;
            feedback_.pageTurn();
        } else {
            turnT_ = -1;
        }
        loadLevel(n);
        if (graphicsReady_) scene_.bake(paper_);
        scene_.startUnfold(turnT_ >= 0 ? tune.turnSeconds * 0.45f : 0.0f);
    }
    if (turnT_ >= 0.0f) {
        turnT_ += dt / maxv(0.05f, tune.turnSeconds * (save_.reduceEffects ? 0.5f : 1.0f));
        if (turnT_ >= 1.0f) turnT_ = -1;
    }
    paper_.setTurn(turnT_, save_.reduceEffects, worldPalette(def_ ? def_->world : 1).page);
    screenTime_ += dt;
    resultTime_ += dt;
    levelTime_ += dt;
    hintLineTime_ = maxv(0.0f, hintLineTime_ - dt);
    if (!coldStart_) titleFade_ = maxv(0.0f, titleFade_ - dt * 3.0f);
    scene_.update(dt, save_.fast);
    // The scene's cues become sounds and haptics.
    {
        CueEvent cues[64];
        const int n = scene_.takeCues(cues, 64);
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
    // The result screens come when the scene has played the end.
    if (screen_ == Screen::Play && def_) {
        if (st_.won() && scene_.winPlayed()) setScreen(Screen::Win);
        else if (st_.lost() && scene_.losePlayed()) setScreen(Screen::Lose);
    }
    // The UI, then the scene's frame.
    uiBatch_.clear();
    ui_.begin(&uiBatch_, layout_);
    drawUi(dt);
    ui_.end();
    if (args_.lint && mode_ == Mode::Picture) {
        lintRan_ = true;
        lintProblems_ = ui_.lint(layout_);
    }
    if (def_) scene_.build(frame_, labels_);
    frame_.ui = &uiBatch_;
    if (args_.logSites) logSites();
    frame_.uiOpacity = 1.0f;
}

void Game::frameShown(float intervalSeconds) {
    ++levelFrames_;
    if (lateFrame(intervalSeconds, env_.refreshHz)) ++levelLate_;
}

void Game::render(unsigned fbo, int w, int h) {
    if (!graphicsReady_) return;
    if (scene_.needsBake()) scene_.bake(paper_);
    paper_.draw(frame_, fbo, w, h);
}

// The browser check (tools/web_check.mjs) taps what this logs: the sites the host can take now, in pixels, and NEXT.
void Game::logSites() {
    if (!def_ || scene_.busy() || turnT_ >= 0.0f) return;
    const int key = level_ * 1000 + undoCount_ * 4 + int(screen_);
    if (key == sitesLogged_) return;
    sitesLogged_ = key;
    char line[600];
    int o = std::snprintf(line, sizeof line, "PMSITES");
    if (screen_ == Screen::Win) {
        if (const UiControl* c = ui_.find("NEXT")) o += std::snprintf(line + o, sizeof line - size_t(o), " NEXT:%d,%d", int(c->r.cx()), int(c->r.cy()));
        else sitesLogged_ = -1;  // the panel is not up yet
    } else if (screen_ == Screen::Play) {
        const u32 moves = sim::takeable(*def_, st_);
        for (int i = 1; i < def_->siteCount && o < int(sizeof line) - 24; ++i) {
            if (!(moves & bit(i))) continue;
            float x = 0, y = 0;
            scene_.siteScreen(i, x, y);
            o += std::snprintf(line + o, sizeof line - size_t(o), " %d:%d,%d", i, int(x), int(y));
        }
    }
    PM_TAG("PMLIFE", "%s", line);
}

void Game::applyScriptedMoves(const char* list) {
    const char* p = list;
    while (*p) {
        char* end = nullptr;
        const long m = std::strtol(p, &end, 10);
        if (end == p) break;
        if (!makeMove(int(m))) PM_WARN("moves: %ld is not a move here", m);
        p = *end == ',' ? end + 1 : end;
    }
}

}  // namespace pm
