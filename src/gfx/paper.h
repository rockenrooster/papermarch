#pragma once
// The paper renderer (TECH_SPEC 5): the wooden table, the page baked once a level, every standing thing as an
// instanced cut-paper "sticker" with one union shadow, the tags and the UI (signed-distance text), and the page turn.

#include "gfx/gl.h"
#include "gfx/target.h"
#include "gfx/text.h"

namespace pm {

enum Shape : u8 {
    kShBox, kShEllipse, kShTri, kShPawn, kShFlag, kShStar, kShRing, kShCapsule, kShBlob, kShHorse, kShDiamond, kShArch,
    kShSprite,  // a frame of the art's atlases: soft = layer; p0, p1, p2, fold = u0, v0, u1, v1; c0 tint (a opacity),
                // c1 a flash added (a its amount), c2.a 1 = drawn additively; a negative hw mirrors it
    kShGate     // a gate's animated panel (sticker.frag gatePanel): p0 kind (0 +, 1 x, 2 -), p1, p2 its width and height
                // in road units, fold the hit flash; c0 colour, c1 highlight, c2.r the taken flash, c2.a 1 while moving
};

// One sticker (shaders/sticker.*): a shape on a quad, pixels, top-left origin. Colours are 0xRRGGBB plus alpha.
struct Sticker {
    float x, y, hw, hh;       // centre and half size
    float rot, shape, soft, p0;
    float p1, p2, fold, lift; // fold: 0 flat, 1 a vertical fold (c1 on the right), 2 a horizontal one; lift: its shadow
    u8 c0[4], c1[4], c2[4];
};
static_assert(sizeof(Sticker) == 60, "sticker instance");

inline void setColor(u8* c, u32 rgb, float alpha = 1.0f) {
    c[0] = u8(rgb >> 16);
    c[1] = u8(rgb >> 8);
    c[2] = u8(rgb);
    c[3] = u8(clampv(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
}
Sticker makeSticker(Shape shape, float x, float y, float hw, float hh, u32 c0, float alpha = 1.0f);

// The road under the battle (shaders/ground.frag): drawn instead of the table and the page when `on`.
struct GroundParams {
    bool on = false;
    float cam0[4] = {};  // principal point x, y (px), focal length (px), camera height
    float cam1[4] = {};  // sin, cos of the pitch, the camera's z, the finish line's z (< 0: none)
    u32 stoneA = 0xC9BFAE, stoneB = 0xA89C8A, mortar = 0x7D7264, curb = 0xE8E0D0, grassA = 0x7CC655, grassB = 0x5FAE45,
        haze = 0xCFE8F0;
    float roadHalf = 5, speckle = 0.5f, hazeStart = 40;
    // The art's textures (gfx/art.h) instead of the procedural stones and grass, when both are set.
    GLuint roadTex = 0, landTex = 0;
    float roadWidth = 11.2f, roadTile = 10, landTile = 8, hazeEnd = 140;
    u32 skyTop = 0x6FA8DC, skyHorizon = 0xCFE8F0;
    GLuint backdropTex = 0;                                       // a strip on the horizon (premultiplied)
    float backdropHeight = 0, backdropShift = 0, backdropHaze = 0.35f;  // its height (px), its sideways shift (u)
};

// After the world, before the tags: bloom, a colour grade and a vignette (off: drawn straight to the target).
struct PostParams {
    bool on = false;
    float threshold = 0.75f, bloom = 0;
    u32 light = 0xFFFFFF;
    float lightStrength = 0, vignette = 0;
    float contrast = 1, saturation = 1, hudTop = 0.1f;
    u32 edgeColor = 0xFF2030;
    float edge = 0;  // a flash at the screen's edges, 0..1
};

struct PaperFrame {
    GroundParams ground;
    PostParams post;
    GLuint spriteTex = 0;  // the art's atlas array (kShSprite stickers)
    const Sticker* shadows = nullptr;   // if set, these cast the union shadow instead of the stickers (feet on a road)
    int shadowCount = 0;
    u32 tableA = 0x70482C, tableB = 0x45281B, tableLine = 0x3C2417;
    float planks = 6;
    float pageX = 0, pageY = 0, pageW = 0, pageH = 0;    // the page on screen, pixels
    float pageV0 = 1, pageV1 = 0;                         // the baked page's rows shown (v up)
    float grain = 0.04f;
    const Sticker* stickers = nullptr;  // far to near; those with a lift throw a shadow
    int stickerCount = 0;
    float shadowStrength = 0.42f;
    u32 shadowColor = 0x26140A;
    const Sticker* over = nullptr;      // confetti and other things above everything but the tags
    int overCount = 0;
    const UiBatch* labels = nullptr;    // the tags
    float labelOpacity = 1;
    const UiBatch* ui = nullptr;
    float uiOpacity = 1;
    float time = 0;
};

class Paper {
public:
    bool init(int w, int h);
    void destroy();
    void resize(int w, int h);
    int maxTextureSize() const { return maxTex_; }

    // The page bake (TECH_SPEC 5.3): each layer throws one union shadow (its stickers' lifts) on what lies under it,
    // then is drawn. The page texture is pw x ph pixels.
    struct BakeLayer {
        const Sticker* s;
        int n;
        float shadowAlpha;  // 0: no shadow
    };
    bool bakePage(int pw, int ph, const BakeLayer* layers, int layerCount);
    bool pageReady() const { return page_.fbo != 0; }

    void draw(const PaperFrame& f, GLuint fbo, int w, int h);
    // The page turn: draws `f` (without the table) into the snapshot; then while turnT is in [0, 1) every draw lays the
    // old page over the new one, curling it away (or, `slide`, fading it: reduce effects).
    void snapshot(const PaperFrame& f);
    void setTurn(float t, bool slide, u32 backColor) {
        turnT_ = t;
        turnSlide_ = slide;
        turnBack_ = backColor;
    }
    int lastDrawCalls() const { return drawCalls_; }

private:
    void setScreen(int w, int h, float time = 0);
    void drawScene(const PaperFrame& f, GLuint fbo, int w, int h, bool table);
    void drawWorld(const PaperFrame& f, GLuint fbo, int w, int h, bool table);
    void postProcess(const PaperFrame& f, GLuint fbo, int w, int h);
    void fullscreen();
    void drawStickers(GLuint vao, GLuint vbo, const Sticker* s, int n, bool upload, float mode, float extraSoft, float alpha,
                      float liftScale);
    void drawBatch(const UiBatch& b, float opacity);
    void drawTurn(int w, int h);
    bool makeR8(Target& t, int w, int h);

    int w_ = 0, h_ = 0, maxTex_ = 2048;
    GLuint ubo_ = 0;
    void drawGround(const GroundParams& g);
    GLuint pSticker_ = 0, pPage_ = 0, pTable_ = 0, pShadow_ = 0, pCurl_ = 0, pText_ = 0, pGround_ = 0, pBright_ = 0, pBlur_ = 0,
           pPost_ = 0;
    GLint uStickerMode_ = -1, uStickerShift_ = -1, uPageRect_ = -1, uPageUV_ = -1, uPageParams_ = -1, uTableA_ = -1, uTableB_ = -1,
          uTableLine_ = -1, uShadow_ = -1, uCurl_ = -1, uSpan_ = -1, uBack_ = -1, uTextSpread_ = -1, uTextOpacity_ = -1,
          uBright_ = -1, uBlurDir_ = -1, uPost_ = -1, uPostLight_ = -1, uPostGrade_ = -1, uPostEdge_ = -1;
    static constexpr int kGroundUniforms = 13;
    GLint uGround_[kGroundUniforms] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    GLuint sprites_ = 0, noSprites_ = 0;  // the frame's atlas array; a 1x1 clear one when there is none
    GLuint blank2D_ = 0;                  // a 1x1 clear texture for samplers with nothing to show
    Target sceneRT_, bloomA_, bloomB_;
    static constexpr int kMaxStickers = 6000;
    GLuint vaoMain_ = 0, vboMain_ = 0, vaoOver_ = 0, vboOver_ = 0, vaoOne_ = 0, vboOne_ = 0, vaoBake_ = 0, vboBake_ = 0;
    GLuint vaoCast_ = 0, vboCast_ = 0;
    GLuint vaoText_ = 0, vboText_ = 0, iboQuads_ = 0, vaoEmpty_ = 0, vaoCurl_ = 0, vboCurl_ = 0;
    GLuint atlas_ = 0;
    Target shadowRT_, snap_, page_, layerRT_;
    int pageW_ = 0, pageH_ = 0;
    float turnT_ = -1;
    bool turnSlide_ = false;
    u32 turnBack_ = 0xFFF4D6;
    int drawCalls_ = 0;
    static constexpr int kCurlRows = 160;
};

}  // namespace pm
