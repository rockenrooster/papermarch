#pragma once
// The game: the level and its battle, the view, the screens, feedback and the save. App drives it.

#include "audio/mixer.h"
#include "core/args.h"
#include "game/feedback.h"
#include "game/field.h"
#include "game/input.h"
#include "game/save.h"
#include "game/ui.h"
#include "gfx/paper.h"
#include "sim/battle.h"

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

    void tick();                         // one fixed 1/60 s step: the battle, the bot, the flow test
    void update(float dt, float alpha);  // once a frame
    void frameShown(float intervalSeconds);
    void render(unsigned fbo, int w, int h);

    bool quitRequested() const { return quit_; }
    int exitCode() const { return exitCode_; }
    int stickers() const { return field_.stickerCount(); }
    int figures() const { return field_.figures(); }
    int confetti() const { return field_.particles(); }
    float lastTickSeconds() const { return lastTick_; }
    int drawCalls() const { return paper_.lastDrawCalls(); }
    audio::Mixer& mixer() { return mixer_; }
    int lintProblems() const { return lintProblems_; }
    bool lintRan() const { return lintRan_; }
    int hapticsSent() const { return hapticsSent_; }
    int hapticPeakPerSecond() const { return hapticPeak_; }

private:
    // Levels.
    void enterLevel(int n, bool turn);
    void loadLevel(int n);
    void restart();
    void finish(bool won);
    void saveNow();
    void applySettings();
    int musicLayers() const;
    void stepBattle(float steer, bool go);
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
    void drawUpdatePill(float y);
    void flowStep();
    bool press(const char* label);  // the flow test: a tap on a control by its label
    // Pictures (pictures.cpp).
    bool setupPicture(const char* name);
    void logSites();

    Args args_;
    GameEnv env_;
    Layout layout_;
    bool quit_ = false;
    int exitCode_ = 0;
    float lastTick_ = 0;

    // The level and its battle.
    int level_ = 1;
    const sim::LevelDef* def_ = nullptr;
    sim::Battle battle_;
    bool go_ = false;       // the player has touched the road: the march is on
    float steerX_ = 0;      // where the player steers the squad, road units
    int stars_ = 0, starsLanded_ = 0;
    bool newBest_ = false;
    float endTime_ = -1;    // seconds since the battle ended (-1: not yet)
    float levelTime_ = 0;
    // A level change waits for the next update (the old page is captured first).
    int pendingLevel_ = 0;
    bool pendingTurn_ = false;
    float turnT_ = -1;

    Field field_;
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

    enum class Mode { Play, Picture, Bench } mode_ = Mode::Play;
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
    int benchLevels_ = 0;
    int sitesLogged_ = -1;

    // Input: a touch on the road steers (a drag moves the squad); one that begins on a control belongs to the UI.
    i64 uiFinger_ = 0;
    bool uiFingerDown_ = false;
    i64 steerFinger_ = 0;
    bool steerDown_ = false;
    float downX_ = 0, steerAnchor_ = 0;
    bool keyLeft_ = false, keyRight_ = false;

    // The flow test.
    int flowStep_ = 0, flowWait_ = 0, flowFailures_ = 0, flowChecks_ = 0;
    void check(bool ok, const char* what);
};

}  // namespace pm
