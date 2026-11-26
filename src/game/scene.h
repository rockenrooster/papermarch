#pragma once
// The scene (TECH_SPEC 5, 6): a level's page and everything on it, drawn from the rules' state. The simulation is
// always the truth; the scene shows a recent past and catches up, one clip at a time (march, fight, flip, count).

#include "core/rng.h"
#include "game/layout.h"
#include "gfx/paper.h"
#include "sim/level.h"

namespace pm {

// What the scene asks the game to play (sounds, haptics), in order; the game drains them each frame.
enum Cue : u8 {
    kCueTap, kCueStep, kCueClash, kCueFlip, kCueCount, kCueCountDown, kCueVillage, kCueShrine, kCuePost, kCueMire, kCueBurn,
    kCueOpen, kCueRival, kCueRaid, kCueWin, kCueLose, kCueRefuse, kCueTurn, kCueUnfold, kCueStar, kCueKeepFall
};
struct CueEvent {
    u8 cue = 0;
    i16 step = 0;   // the count ladder's step, a site's x on screen for panning (-1..1 as step / 100)
    float pan = 0;
};

class Scene {
public:
    static constexpr int kMaxFigures = 520;
    static constexpr int kMaxConfetti = 600;
    static constexpr int kMaxClips = 96;
    static constexpr int kMaxDecor = 400;

    void init(const Layout& L);
    void resize(const Layout& L);
    // A new level: builds the dressing; the page must be baked (bake()) before it is drawn.
    void setLevel(const sim::LevelDef& d, int number, const sim::State& st);
    bool bake(Paper& paper);  // false if the bake failed
    bool needsBake() const { return needBake_; }
    // Snap the shown state to `st` (undo, restart): every clip is dropped.
    void snap(const sim::State& st);
    // A move made in the rules: `ev` are its events; its clips are appended.
    void play(const sim::State& after, const sim::Events& ev);
    void update(float dt, bool fast);
    void build(PaperFrame& f, UiBatch& labels);
    // A touch: the site (or sim::kRivalMove) under (x, y) in pixels, or -1.
    int hit(float x, float y) const;
    void refuse(int site, const char* why);
    void ringHint(int site) { hintSite_ = site; hintTime_ = 0; }
    void clearHint() { hintSite_ = -1; }
    void startUnfold(float lead = 0) { unfold_ = -lead; }
    void confettiBurst(float x, float y, int n, bool gold, float speed = 1.0f);  // map units
    void celebrate(bool perfect);

    bool busy() const { return clipCount_ > 0; }
    int movesQueued() const { return movesQueued_; }
    float shownHost() const { return shownS_; }
    const sim::State& shown() const { return disp_; }
    bool winPlayed() const { return winDone_; }
    bool losePlayed() const { return loseDone_; }
    int figures() const { return figCount_; }
    int confetti() const { return confettiCount_; }
    int stickerCount() const { return stickerN_; }
    int takeCues(CueEvent* out, int max);
    // Where a site is on the screen (pixels), for the hint ring and the flow test.
    void siteScreen(int site, float& x, float& y) const;
    float pageX() const { return fit_.x; }
    float pageK() const { return fit_.k; }
    bool reduceEffects = false;
    int levelNumber = 1;

private:
    struct Figure {
        float x = 0, y = 0;     // map units
        float tx = 0, ty = 0;   // target
        float hop = 0;          // phase
        float flip = 1;         // 0..1: the paper turn (scales width), 1 = settled
        float alpha = 1;
        float vx = 0, vy = 0;   // scattered (lose)
        u8 kind = 0;            // 0 pawn, 1 rider, 2 giant
        u8 team = 0;            // 0 host, 1 enemy, 2 rival, 3 joiner (enemy turned host, running in), 4 villager
        i16 site = -1;          // a garrison's site
        u8 bearer = 0;
        u8 alive = 1;
        float ox = 0, oy = 0;   // a wave figure's place in its block
    };
    struct Confetti {
        float x, y, vx, vy, rot, spin, life, age;
        u32 c0, c1;
    };
    enum ClipType : u8 {
        kClipMarch, kClipArrive, kClipCount, kClipBurn, kClipOpen, kClipTurn, kClipRivalMove, kClipRivalJoin, kClipRaid,
        kClipWin, kClipLose, kClipSettle, kClipEndMove
    };
    struct Clip {
        u8 type = 0;
        i16 site = -1;
        float dur = 0;
        i64 a = 0, b = 0;
        int move = 0;           // which queued move it belongs to
    };
    struct Road {
        float px[24], py[24];   // the road's curve, map units
        int n = 0;
    };
    struct Decor {
        float x, y, s;
        u8 type;                // 0 tree, 1 accent tree, 2 rock, 3 flowers, 4 reeds
        u8 seed;
    };

    void layoutPage();
    void buildRoads();
    void buildDecor();
    void buildBake();
    void startClip();
    void finishClip();
    void applyClipStart(const Clip& c);
    void applyClipEnd(const Clip& c);
    void cue(u8 c, int step = 0, float pan = 0);
    void spawnGarrisons();
    void garrisonOf(int site, int count, u8 team);
    void hostTargets(float dt);
    void stepFigures(float dt);
    void stepConfetti(float dt);
    bool pathTo(int site, float* xs, float* ys, int& n) const;
    float panOf(float mapX) const { return clampv((mapX - 50.0f) / 50.0f, -1.0f, 1.0f); }
    // Drawing.
    float sx(float x) const { return fit_.x + x * fit_.k; }
    float sy(float y) const { return fit_.y + (y - scroll_) * fit_.k; }
    void addSticker(const Sticker& s);
    void popup(int site, float rise);
    void drawFigure(const Figure& f);
    void drawTags(UiBatch& b);
    void tag(UiBatch& b, float x, float y, const char* text, u32 bg, u32 fg, u32 edge, float scale, bool lock, const char* badge,
             u32 badgeColor, float alpha);
    void tent(float x, float y, float s, u32 light, u32 dark, u32 flag, float rise);
    void house(float x, float y, float s, u32 wall, u32 wallDark, u32 roof, u32 roofDark, float rise);
    void tower(float x, float y, float w, float h, u32 light, u32 dark, u32 roof, u32 roofDark, bool crenel, float rise);
    void flagAt(float x, float y, float h, float w, u32 c, u32 trim, float rise, float phase);
    void treeAt(const Decor& d, float rise);

    Layout L_;
    PageFit fit_;
    const sim::LevelDef* def_ = nullptr;
    sim::LevelDef defCopy_;
    sim::State disp_;          // what the tags show
    sim::State target_;        // the rules' latest state
    float scroll_ = 0, scrollTarget_ = 0;
    bool needBake_ = true;
    float time_ = 0;
    float unfold_ = 10;        // seconds since the page appeared
    // Roads, dressing.
    Road roads_[sim::kMaxRoads];
    Decor decor_[kMaxDecor];
    int decorCount_ = 0;
    bool river_ = false;
    float riverY_ = 0, riverAmp_ = 0, riverPhase_ = 0;
    bool islands_ = false;
    bool waves_ = false;       // a wave battle (GAME_DESIGN 6): lanes up a field, waves between the rows
    float waveFlip_[sim::kMaxRaids] = {};
    int waveClip_ = -1;        // the raid whose clip is playing
    float waveClipT_ = 0;      // its progress 0..1
    void waveSpot(int raid, float& x, float& y) const;    // where a wave waits for its turn
    void waveCenter(int raid, float& x, float& y) const;  // where it is now (marching in its clip)
    void spawnWave(int raid);
    Sticker bakeLand_[160], bakeTerr_[200], bakeRoads_[1400], bakeWater_[400];
    int bakeLandN_ = 0, bakeTerrN_ = 0, bakeRoadsN_ = 0, bakeWaterN_ = 0;
    // Figures and the banner.
    Figure figs_[kMaxFigures];
    int figCount_ = 0;
    float bannerX_ = 0, bannerY_ = 0, bannerDirX_ = 0, bannerDirY_ = -1;
    int bannerSite_ = 0;
    float trailX_[256], trailY_[256];
    int trailHead_ = 0, trailLen_ = 0;
    bool marching_ = false;
    float shownS_ = 0;         // the host plate's number
    float rivalShown_ = 0;
    float rivalX_ = 0, rivalY_ = 0;
    // Clips.
    Clip clips_[kMaxClips];
    int clipHead_ = 0, clipCount_ = 0;
    float clipT_ = 0;
    bool clipStarted_ = false;
    int movesQueued_ = 0;
    sim::State moveAfter_[4];
    int moveIndex_ = 0, moveSerial_ = 0;
    float marchX_[96], marchY_[96], marchLen_[96];
    int marchN_ = 0;
    // Per-site animation.
    float flipT_[sim::kMaxSites] = {};   // 0..1 the camp's flip to the host's colours
    float burnT_[sim::kMaxSites] = {};   // 0..1 folding flat
    float pop_[sim::kMaxSites] = {};     // tag pop
    float shake_[sim::kMaxSites] = {};
    char why_[sim::kMaxSites][24] = {};
    float whyT_[sim::kMaxSites] = {};
    u32 shownMine_ = 1;
    int hintSite_ = -1;
    float hintTime_ = 0;
    float keepFold_ = -1;      // the win: the Keep folds and stands again
    bool winDone_ = false, loseDone_ = false;
    float winT_ = -1, loseT_ = -1;
    // Confetti.
    Confetti conf_[kMaxConfetti];
    int confettiCount_ = 0;
    Rng rng_;
    // The frame's stickers, built in groups (a pop-up, a tree, a figure) and sorted far to near by group.
    static constexpr int kMaxStickers = 5600;
    static constexpr int kMaxGroups = 2400;
    struct Group {
        float key;
        int first, count;
    };
    void beginGroup(float key);
    float riseAt(float y, bool keep) const;  // the unfold: 0 flat .. 1 standing (with a little overshoot)
    float curRise_ = 1, curBase_ = 0, curBurn_ = 0;  // the group's rise, its base line (pixels), how burnt
    Sticker stickers_[kMaxStickers], tmp_[kMaxStickers];
    Group groups_[kMaxGroups];
    int stickerN_ = 0, tmpN_ = 0, groupN_ = 0;
    Sticker over_[kMaxConfetti + 16];
    int overN_ = 0;
    // Cues.
    CueEvent cues_[64];
    int cueCount_ = 0;
    float countAcc_ = 0;
    int countStep_ = 0;
};

}  // namespace pm
