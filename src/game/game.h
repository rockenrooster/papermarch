#pragma once
// The game (TECH_SPEC 8): the level, its state and undo stack, the scene, the screens, feedback. App drives it.

#include "audio/mixer.h"
#include "core/args.h"
#include "game/feedback.h"
#include "game/input.h"
#include "game/save.h"
#include "game/scene.h"
#include "game/ui.h"
#include "gfx/paper.h"
#include "sim/level.h"

namespace pm {

struct GameEnv {
    int frameW = 0, frameH = 0;
    float dpScale = 1;
    SafeArea safe;
    bool android = false;
    float refreshHz = 60;
};

enum class Screen { Play, Pause, Win, Lose, Settings, Levels, End };
const char* screenName(Screen s);

class Game {
public:
    bool init(const Args& args, const GameEnv& env);
    bool initGraphics();
    void destroyGraphics();
    void resize(const GameEnv& env);

    void pointer(const PointerEvent& e);
    void key(Key k, bool down);
    void background();
    void foreground();

    void tick();                         // one fixed 1/60 s step: bots, the flow test, scripted pictures
    void update(float dt, float alpha);  // once a frame
    void frameShown(float intervalSeconds);
    void render(unsigned fbo, int w, int h);

    bool quitRequested() const { return quit_; }
    int exitCode() const { return exitCode_; }
    int stickers() const { return scene_.stickerCount(); }
    int figures() const { return scene_.figures(); }
    int confetti() const { return scene_.confetti(); }
    float lastTickSeconds() const { return lastTick_; }
    int drawCalls() const { return paper_.lastDrawCalls(); }
    audio::Mixer& mixer() { return mixer_; }
    int lintProblems() const { return lintProblems_; }
    bool lintRan() const { return lintRan_; }
    int hapticsSent() const { return hapticsSent_; }
    int hapticPeakPerSecond() const { return hapticPeak_; }

private:
    // Levels and moves.
    void enterLevel(int n, bool turn);
    void loadLevel(int n);
    bool makeMove(int move);
    void undo();
    void restart();
    void afterMove();
    void useHint();
    void saveNow();
    void applySettings();
    int musicLayers() const;
    int botMove();
    // Screens (screens.cpp).
    void setScreen(Screen s);
    void drawUi(float dt);
    void drawHud();
    void drawPause();
    void drawWin();
    void drawLose();
    void drawSettings(float dt);
    void drawLevels();
    void drawEnd();
    void drawTitle();
    void flowStep();
    bool press(const char* label);  // the flow test: a tap on a control by its label
    // Pictures and benchmarks (pictures.cpp).
    bool setupPicture(const char* name);
    void applyScriptedMoves(const char* list);

    Args args_;
    GameEnv env_;
    Layout layout_;
    bool quit_ = false;
    int exitCode_ = 0;
    float lastTick_ = 0;

    // The level.
    int level_ = 1;
    const sim::LevelDef* def_ = nullptr;
    sim::State st_;
    static constexpr int kUndo = 96;
    sim::State undo_[kUndo];
    int undoCount_ = 0;
    sim::Solver* solver_ = nullptr;
    sim::Solve solve_;
    bool hinted_ = false;
    bool deadEnd_ = false;
    int stars_ = 0, starsLanded_ = 0;
    bool newBest_ = false;
    float levelTime_ = 0;
    // A level change waits for the next update (the old page is captured first, then the new one baked).
    int pendingLevel_ = 0;
    bool pendingTurn_ = false;
    float turnT_ = -1;

    Scene scene_;
    Paper paper_;
    PaperFrame frame_;
    UiBatch labels_, uiBatch_;
    Ui ui_;
    bool graphicsReady_ = false;
    audio::Mixer mixer_;
    Feedback feedback_;
    SaveData save_;
    char savePath_[512] = "";
    bool saveEnabled_ = true;

    enum class Mode { Play, Picture, Bench, Hash } mode_ = Mode::Play;
    Screen screen_ = Screen::Play;
    Screen settingsReturn_ = Screen::Play;
    float screenTime_ = 0;
    float resultTime_ = 0;
    int levelsFirst_ = 0;
    bool coldStart_ = true;
    float titleFade_ = 1;
    float holdProgress_ = 0;
    bool confirmReset_ = false;
    int lintProblems_ = 0;
    bool lintRan_ = false;
    int pictureTick_ = 0;
    char pictureScreen_[16] = "";
    int updateOverride_ = 0;
    bool updateAsked_ = false;
    int hintShownFor_ = 0;
    float hintLineTime_ = 0;
    int levelFrames_ = 0, levelLate_ = 0;
    static constexpr int kHapticRing = 24;
    double hapticRing_[kHapticRing] = {};
    int hapticsSent_ = 0, hapticPeak_ = 0;
    bool useBot_ = false;
    u32 botRng_ = 99;
    float botWait_ = 0;
    int benchLevels_ = 0;
    int sitesLogged_ = -1;
    void logSites();

    // Input: a touch on the page takes a site; a touch that begins on a control belongs to the UI.
    i64 uiFinger_ = 0;
    bool uiFingerDown_ = false;
    i64 pageFinger_ = 0;
    bool pageFingerDown_ = false;
    float downX_ = 0, downY_ = 0;

    // The flow test (TECH_SPEC 9.4).
    int flowStep_ = 0, flowWait_ = 0, flowFailures_ = 0, flowChecks_ = 0;
    void check(bool ok, const char* what);
};

}  // namespace pm
