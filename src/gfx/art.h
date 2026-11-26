#pragma once
// The art (docs/ART_DIRECTION.md, made by tools/art/): sprite atlases and their manifest (assets/art/sprites.txt),
// seamless ground textures and each world's look (worlds.txt). The files ship in the APK's assets, the browser's
// preloaded files and, on the desktop, are read from the repository. Loaded once after the GL context exists (the
// ground textures one world at a time). With no manifest `ready()` is false and the game draws its paper stickers.

#include "core/types.h"
#include "gfx/gl.h"

namespace pm {

struct SpriteFrame {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;  // in the atlas array
    float layer = 0;
    float w = 1, h = 1;                     // the rect, atlas pixels
    float px = 0, py = 0;                   // the pivot, pixels from the rect's top-left
    float height = 1;                       // the rect's height in road units
};

struct SpriteAnim {
    char name[48] = "";  // kind.anim
    float fps = 12;
    bool loop = true, additive = false;
    int frames = 0;
    int elevFirst = 0, elevCount = 0;
};

struct PropRule {
    int anim = -1;
    float weight = 1, scaleMin = 1, scaleMax = 1, xMin = 7, xMax = 12, spacing = 4;
};
struct EdgeRule {
    int anim = -1;
    float spacing = 4, x = 5.6f;
};
struct AmbientRule {
    int anim = -1;
    float perSecond = 0;
};

struct WorldArt {
    bool defined = false;
    char name[32] = "";
    char roadFile[48] = "", landFile[48] = "";
    float roadWidth = 11.2f, roadTile = 10, landTile = 8;
    u32 haze = 0xCFE8F0;
    float hazeStart = 40, hazeEnd = 140;
    u32 skyTop = 0x6FA8DC, skyHorizon = 0xCFE8F0;
    u32 light = 0xFFFFFF;
    float lightStrength = 0;
    float bloomThreshold = 0.75f, bloomStrength = 0;
    float vignette = 0;
    float contrast = 1, saturation = 1;
    char backdropFile[48] = "";
    float backdropHeight = 0.3f;  // a share of the screen's width
    float shadowDx = 0.5f, shadowDz = 0.6f, shadowAlpha = 0.35f;  // cast shadows: offset per unit of height, opacity
    u32 shadowColor = 0x1B2A55;
    float bossHue = 0;          // the shared boss recoloured: its hue turned (degrees), then tinted
    u32 bossTint = 0xFFFFFF;
    int bossAddon = -1;         // an anim drawn over the boss with its pivot and timing (a turban, a mantle), -1 none
    static constexpr int kMaxProps = 24, kMaxEdges = 6, kMaxAmbient = 6;
    PropRule props[kMaxProps];
    int propCount = 0;
    EdgeRule edges[kMaxEdges];
    int edgeCount = 0;
    AmbientRule ambient[kMaxAmbient];
    int ambientCount = 0;
};

class Art {
public:
    static constexpr int kWorlds = 10;
    // `dir` overrides where the files are (--art); "" looks in the platform's places. False when there is no manifest.
    bool load(const char* dir);
    void destroy();
    bool ready() const { return ready_; }
    const char* dir() const { return dir_; }

    int find(const char* name) const;      // "soldier.march" -> an anim, -1 if missing
    int findKind(const char* kind) const;  // an exact anim name, or the first anim of the kind ("w1.tree_a" -> its .still)
    int animCount() const { return animCount_; }
    const SpriteAnim& anim(int a) const { return anims_[a]; }
    // The frame nearest the view elevation (degrees) `t` seconds into the anim (looping or holding its last frame).
    const SpriteFrame& frame(int a, float elevDeg, float t) const;
    const SpriteFrame& frameAt(int a, float elevDeg, int index) const;
    // A named spot (a muzzle) in the frame's pixels; false if the anim has none.
    bool point(int a, float elevDeg, const char* name, float& x, float& y) const;

    const WorldArt& world(int n) const;  // 1..10
    // Makes this world current: its own atlases (props, its boss) into their layers and its ground textures, loaded
    // when the world changed. False: the world has no ground textures.
    bool useWorld(int n);
    GLuint sprites() const { return spriteTex_; }
    GLuint road() const { return roadTex_; }
    GLuint land() const { return landTex_; }
    GLuint backdrop() const { return backdropTex_; }  // 0: the world has none

private:
    struct Elev {
        int deg = 0;
        int frameFirst = 0;
        int pointCount = 0;
        char pointName[4][16] = {};
        float pointX[4] = {}, pointY[4] = {};
    };
    int elevIndex(const SpriteAnim& a, float elevDeg) const;
    bool loadSprites(const char* text);
    void loadWorlds(const char* text);
    GLuint loadGround(const char* file, int wrap);  // wrap: 0 repeat, 1 clamp across (the road), 2 clamp (the backdrop)

    // An atlas: global ones (world 0) stay in their layers; a world's own share the layers after them, loaded while
    // that world is played.
    struct Atlas {
        char file[64] = "";
        int world = 0, w = 0, h = 0, layer = 0;
    };
    bool upload(const Atlas& a);
    static constexpr int kMaxAtlases = 48;
    Atlas atlases_[kMaxAtlases];
    int atlasCount_ = 0, arrayW_ = 1, arrayH_ = 1, worldLoaded_ = 0;

    bool ready_ = false;
    char dir_[260] = "";
    static constexpr int kMaxAnims = 1024, kMaxElevs = 4096, kMaxFrames = 16384;
    SpriteAnim* anims_ = nullptr;
    Elev* elevs_ = nullptr;
    SpriteFrame* frames_ = nullptr;
    int animCount_ = 0, elevCount_ = 0, frameCount_ = 0;
    WorldArt worlds_[kWorlds];
    GLuint spriteTex_ = 0, roadTex_ = 0, landTex_ = 0, backdropTex_ = 0;
    char roadLoaded_[48] = "", landLoaded_[48] = "", backdropLoaded_[48] = "";
    float maxAniso_ = 0;
};

}  // namespace pm
