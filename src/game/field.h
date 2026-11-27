#pragma once
// The battle on screen (2026-11-26): a camera over the road; the squad, the gates, the barrels, the hordes and the boss
// as cut-paper stickers in perspective, far to near; their tags; bursts, falling figures and floating numbers; cues for
// the sounds. It reads the battle and never changes it.

#include "game/layout.h"
#include "gfx/art.h"
#include "gfx/paper.h"
#include "sim/battle.h"

namespace pm {

enum Cue : u8 {
    kCueShot, kCueHit, kCueKill, kCueGate, kCueGateBad, kCueGrow, kCueBarrelHit, kCueBarrel, kCueCrash, kCueWave, kCueBoss,
    kCueBossHit, kCueBossDown, kCueLoss, kCueWin, kCueLose, kCueStart,
    kCueWeapon,  // a weapon taken (step: its kind)
    kCueBlast,   // a rocket's burst
    kCuePower,   // a power-up taken (step: its kind)
    kCueShield,  // the shield absorbs a hit (step 1: it breaks)
    kCueBomb,    // an airstrike's bomb lands
    kCueCage,    // prisoners freed
    kCueArrow    // archers loose
};
struct CueEvent {
    u8 cue = 0;
    u8 weapon = 0;  // the squad's weapon (its shots sound like it)
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
    // The art (null: the paper stickers). Its sprites are found by name once here.
    void setArt(Art* art);
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
        u8 shape, kind;  // kind: 0 a bit of paper, 1 a puff, 2 a falling figure, 3 a spark, 4 a ring, 5 confetti,
                         // 6 a flipbook (the art's `anim`, played once over its life, after `vr` seconds of delay),
                         // 7 a drifting bit, 8 a figure shrinking away (the art)
        i16 anim;        // the art's anim (a bit's, a flipbook's, a dying figure's sprite), -1 none
    };
    // The art's sprites this view uses, found by name (-1: missing, the paper sticker instead).
    struct Ids {
        int soldier = -1, foot = -1, rider = -1, brute = -1, boss = -1, barrel = -1, banner = -1, cheer = -1;
        int bossOf[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};  // each world's own boss (boss_wN), else boss
        int muzzle = -1, tracer = -1, hit = -1, poofRed = -1, poofBlue = -1, debris = -1, explosion = -1, explosionGlow = -1;
        int bigboom = -1, bigboomGlow = -1, ring = -1, dust = -1, sparkle = -1;
        int gatePass[3] = {-1, -1, -1}, gatePost[3] = {-1, -1, -1}, gateSpikes = -1;  // add, multiply, negative
        int confetti[6] = {-1, -1, -1, -1, -1, -1};
        int wpn[sim::kWeaponCount] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};  // the gun over each soldier (rendered without)
        int wpnIcon[sim::kWeaponCount] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
        int shotFx[sim::kWeaponCount] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};  // each weapon's balls in flight
        int gatePostWpn = -1, crateWpn = -1, crateBreak = -1;
        int muzzleWide = -1, muzzleRing = -1, puffSmall = -1, hitCyan = -1, pierce = -1, smoke = -1;
        // Power-ups (ART_DIRECTION 16): carriers, icons and effects.
        int medal = -1, lightColumn = -1, crateSupply = -1, pickupFx = -1;
        int powIcon[sim::kPowerCount] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
        int tracerHot = -1, gunGlow = -1, shieldDome = -1, shieldCrack = -1, shieldBreak = -1, frostRing = -1, frostOverlay = -1,
            bomb = -1, shadowGrow = -1;
        // Step 3 (ART_DIRECTION 17): archers and arrows, mines, cages, moving gates' rail and wheeled posts, coins.
        int archer = -1, archerShoot = -1, arrow = -1, mine = -1, cage = -1, prisoner = -1, lightGreen = -1, cageOpen = -1, rail = -1,
            coin = -1;
        int postWheel[4] = {-1, -1, -1, -1};  // add, multiply, negative, weapon
        // Step 4: catapults and their boulders, the warnings on the road, giants, the boss's attack poses.
        int catapult = -1, catapultFire = -1, boulder = -1, warnCircle = -1, dirtBurst = -1, warnHalf = -1, waveRidge = -1,
            arrowGround = -1, giant = -1, bossSlam = -1, bossThrow = -1, bossRoar = -1, cageBack = -1;
        int addonPose[10][3] = {};  // each world's add-on in the boss's attack poses (wN.boss_addon.slam/.throw/.roar)
        // Step 5: shield bearers and their shields, bombers, drummers; the cannon at the squad's front (and the field gun);
        // the flamethrower's jet, the storm coil's bolts and their sparks, mortar shells and where they land; the magnet's
        // arc, quick march's lines, reinforcements parachuting in, scorch marks, the gold rim of a gate that counts twice.
        int shieldman = -1, shieldFront = -1, bomber = -1, drummer = -1, squadCannon = -1, flame = -1, zap = -1, lightning = -1,
            shell = -1, targetRing = -1, magnetArc = -1, speedLines = -1, soundRing = -1, recruitFly = -1, scorch = -1, goldRim = -1;
        int confettiCount = 0;
    };
    // Where a sprite went: its sticker, the pivot on screen, screen pixels per atlas pixel, the view's elevation. Between
    // two renderings it is two stickers (st the nearer rendering, st2 the other, fading).
    struct Placed {
        Sticker* st = nullptr;
        Sticker* st2 = nullptr;
        float sx = 0, sy = 0, k = 0, elev = 0, s = 0;
        const SpriteFrame* fr = nullptr;
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
    void drawPowers(const sim::Battle& b, UiBatch& labels);  // medallions, the shield's dome, bombs falling
    void drawHazards(const sim::Battle& b, UiBatch& labels, float step);  // mines, cages, arrows in flight
    void drawStrikes(const sim::Battle& b, float step);  // boulders' circles and flight, the club's half, the stomp's ridge
    void drawParticles();
    void label(UiBatch& b, const char* s, float x, float y, float size, u32 fill, u32 stroke);
    void burst(float x, float z, float y, int n, u32 c0, u32 c1, float speed, float size, u8 kind);
    void figureFalls(float x, float z, float size, u32 c0, u32 c1, u32 c2, float dir, int anim = -1);
    void floater(float x, float z, float y, const char* text, u32 fill, u32 stroke, float size, bool follow = false);
    void cue(u8 c, int step, float x);
    float rnd();
    // The art: a frame of `anim` standing at (x, z), `y` above the road, `t` seconds into it, `scale` times its size,
    // rotated about its pivot, tinted, its hue turned (degrees). Off screen or a full pool: st is null.
    Placed sprite(int anim, float x, float z, float y, float t, float scale, float alpha, float rot = 0, bool mirror = false,
                  float flash = 0, u32 tint = 0xFFFFFF, float hue = 0);
    // An icon (a UI sprite) in the labels over the field, centred at (x, y), h pixels tall, its width times wScale.
    void icon(UiBatch& b, int anim, float x, float y, float h, float alpha = 1.0f, float wScale = 1.0f);
    // A sprite at a screen spot (its pivot there), `size` road units tall per its height, for effects on a figure.
    Sticker* spriteAt(int anim, float key, float sx, float sy, float s, float t, float scale, float alpha, float rot = 0);
    void flipbook(int anim, float x, float z, float y, float scale, float rot = 0, float delay = 0);
    void dying(int anim, float x, float z, float knock, float seconds, float scale = 1);
    void confetti(float x, float z, float y, int n, float spread);
    // A thing's shadow cast on the road along the world's sun: `height` and `radius` (its footprint) in road units.
    void castShadow(float x, float z, float height, float radius);
    void sceneryArt(float zLo, float zHi);
    void ambientArt(float dt);
    void drawGround(PaperFrame& f);

    Layout L_;
    Art* art_ = nullptr;
    Ids ids_;
    float ambientClock_[WorldArt::kMaxAmbient] = {};
    float shDx_ = 0.5f, shDz_ = 0.6f, shAlpha_ = 0.35f;
    u32 shColor_ = 0x1B2A55;
    float gateUsedAt_[sim::kMaxGates] = {};  // when the squad took each gate (its collapse), -1 never
    float bornAt_[200] = {};                 // when each shown soldier appeared (the pop-in)
    int shownBefore_ = 0;
    int bossFrame_ = -1, bossTenth_ = 10;
    float hitStop = 0;  // seconds the view nearly stops (the boss's fall)
    float zoom_ = 0;    // the camera's zoom-out after the boss's fall, 0..1
    float edge_ = 0;    // a red flash at the screen's edges (a lost gate, a crash), 1..0
    Camera cam_;
    const sim::LevelDef* def_ = nullptr;
    int number_ = 1;
    float depthS_ = 16;  // the squad's depth from the camera
    float squadX_ = 0, squadY_ = 0;
    float viewZ_ = 0, viewX_ = 0;  // the squad as drawn (between steps)
    float lastZ_ = 0, lastX_ = 0, dz_ = 0, dx_ = 0;
    float shownSquad_ = 0, badgePop_ = 0;
    int lastShown_ = -1;  // the count last shown (the banner pops on every change)
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
    // Bolts drawn from point to point on the road for a moment: the storm coil's lightning, the magnet's arc.
    struct Bolt {
        float x0, z0, x1, z1, life, max;
        int anim;
    };
    static constexpr int kBolts = 32;
    Bolt bolts_[kBolts] = {};
    int boltHead_ = 0;
    float flameT_ = 0;  // the flamethrower's jet shows while this runs (it burns something)
    void bolt(int anim, float x0, float z0, float x1, float z1, float life);
    void drawBolts();
    // A sprite stretched from screen point a to b (its height along the line), `width` pixels across.
    void streak(int anim, float key, float ax, float ay, float bx, float by, float width, float t, float alpha);
    int partCount_ = 0, partHead_ = 0;
    static constexpr int kFloaters = 24;
    Floater floaters_[kFloaters];
    int floaterHead_ = 0;
    static constexpr int kMaxShown = 200;
    float flash_[kMaxShown] = {};  // a soldier's muzzle flash
    static constexpr int kCues = 64;
    CueEvent cues_[kCues];
    int cueN_ = 0;
    float cueClock_[24] = {};      // per cue: when it last sounded (rate limits)
    u8 weaponNow_ = 0;             // the squad's weapon as of the last step (its shots and its look)
    bool frozenNow_ = false;       // FREEZE is on (this frame), the squad at squadZ_
    float squadZ_ = 0;
    float gateStep_ = 0;           // this frame's time past the last step (moving gates slide smoothly)
    u8 bossAct_ = 0;               // the boss's attack pose (1 the club, 2 a throw, 3 a roar) and its clock
    float bossActT_ = 9;
    float smokeClock_ = 0;         // rockets drop a puff of smoke every 0.05 s
};

}  // namespace pm
