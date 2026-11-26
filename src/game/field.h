#pragma once
// The battle on screen (2026-11-26): a camera over the road; the squad, the gates, the barrels, the hordes and the boss
// as cut-paper stickers in perspective, far to near; their tags; bursts, falling figures and floating numbers; cues for
// the sounds. It reads the battle and never changes it.

#include "game/layout.h"
#include "gfx/paper.h"
#include "sim/battle.h"

namespace pm {

enum Cue : u8 {
    kCueShot, kCueHit, kCueKill, kCueGate, kCueGateBad, kCueGrow, kCueBarrelHit, kCueBarrel, kCueCrash, kCueWave, kCueBoss,
    kCueBossHit, kCueBossDown, kCueLoss, kCueWin, kCueLose, kCueStart
};
struct CueEvent {
    u8 cue = 0;
    int step = 0;   // the count ladder's step, a kill's kind
    float pan = 0;  // -1 left .. 1 right
};

// The camera: a pinhole `h` above the road, pitched down, looking up the road (+z).
struct Camera {
    float cx = 0, cy = 0, f = 1;  // principal point and focal length, pixels
    float h = 20, sn = 0.5f, cs = 0.87f;
    float z = 0;                  // the camera's foot on the road
    float tTop = 80, tBottom = 0; // the road seen at the top and the bottom of the screen, ahead of the foot
    // A point `y` above the road at (x, zw): its place on the screen and pixels per road unit there; false if behind.
    bool project(float x, float zw, float y, float& sx, float& sy, float& scale) const;
};

class Field {
public:
    void init(const Layout& L);
    void resize(const Layout& L);
    void setLevel(const sim::LevelDef& d, int number);
    void events(const sim::Battle& b, const sim::BattleEvents& ev);  // after every step
    void update(float dt, const sim::Battle& b);                       // every frame
    void build(PaperFrame& f, UiBatch& labels, const sim::Battle& b, float alpha);
    int takeCues(CueEvent* out, int max);
    void celebrate();  // confetti over everything

    float unitsPerPixel() const { return depthS_ / cam_.f; }  // at the squad (steering)
    float squadScreenY() const { return squadY_; }
    float squadScreenX() const { return squadX_; }
    float shownSquad() const { return shownSquad_; }
    int stickerCount() const { return out_; }
    int figures() const { return figures_; }
    int particles() const { return partCount_; }
    bool reduceEffects = false;
    float time = 0;

private:
    struct Item {
        float key;  // road z: far first
        u16 first, count;
    };
    struct Particle {
        float x, z, y, vx, vz, vy;
        float life, max, size, rot, vr;
        u32 c0, c1;
        u8 shape, kind;  // kind: 0 a bit of paper, 1 a puff, 2 a falling figure, 3 a spark, 4 a ring
    };
    struct Floater {
        float x, z, y, life;
        u32 fill, stroke;
        float size;
        bool follow;  // x and z are from the squad (it rises over the marching squad)
        char text[16];
    };

    void layoutCamera();
    // The draw list: stickers in a pool, items sorted far to near.
    Sticker* add(float key, int n);
    void shadow(float sx, float sy, float hw, float hh);
    void scenery(float zLo, float zHi);
    void drawSoldiers(const sim::Battle& b, float zs, float xs, UiBatch& labels);
    void drawEnemy(const sim::Enemy& e, float z);
    void drawBoss(const sim::Enemy& e, float z, UiBatch& labels);
    void drawGates(const sim::Battle& b, UiBatch& labels);
    void drawBarrels(const sim::Battle& b, UiBatch& labels);
    void drawParticles();
    void label(UiBatch& b, const char* s, float x, float y, float size, u32 fill, u32 stroke);
    void burst(float x, float z, float y, int n, u32 c0, u32 c1, float speed, float size, u8 kind);
    void figureFalls(float x, float z, float size, u32 c0, u32 c1, u32 c2, float dir);
    void floater(float x, float z, float y, const char* text, u32 fill, u32 stroke, float size, bool follow = false);
    void cue(u8 c, int step, float x);
    float rnd();

    Layout L_;
    Camera cam_;
    const sim::LevelDef* def_ = nullptr;
    int number_ = 1;
    float depthS_ = 16;  // the squad's depth from the camera
    float squadX_ = 0, squadY_ = 0;
    float viewZ_ = 0, viewX_ = 0;  // the squad as drawn (between steps)
    float lastZ_ = 0, lastX_ = 0, dz_ = 0, dx_ = 0;
    float shownSquad_ = 0, badgePop_ = 0;
    float shake_ = 0;
    float bossSeen_ = 0;
    float confettiT_ = -1;
    u32 rng_ = 0x9E3779B9u;
    int figures_ = 0;

    static constexpr int kPool = 6000;  // the renderer's limit (Paper::kMaxStickers)
    Sticker pool_[kPool];
    int poolN_ = 0;
    static constexpr int kItems = 4200;
    float detailScale_ = 30;  // enemies drawn bigger than this (px a unit) get a face and a spear
    Item items_[kItems];
    int itemN_ = 0;
    Sticker sorted_[kPool];
    int out_ = 0;
    static constexpr int kCasters = 2600;
    Sticker casters_[kCasters];
    int casterN_ = 0;

    static constexpr int kParticles = 700;
    Particle parts_[kParticles];
    int partCount_ = 0, partHead_ = 0;
    static constexpr int kFloaters = 24;
    Floater floaters_[kFloaters];
    int floaterHead_ = 0;
    static constexpr int kMaxShown = 200;
    float flash_[kMaxShown] = {};  // a soldier's muzzle flash
    static constexpr int kCues = 64;
    CueEvent cues_[kCues];
    int cueN_ = 0;
    float cueClock_[20] = {};      // per cue: when it last sounded (rate limits)
};

}  // namespace pm
