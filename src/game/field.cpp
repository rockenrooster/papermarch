// The battle on screen (see field.h).
#include "game/field.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/log.h"
#include "core/tuning.h"
#include "gfx/palette.h"

namespace pm {

namespace {

constexpr float kPi = 3.14159265f;

// The battle's colours, the same in every world: the squad blue, the horde red, gold barrels.
constexpr u32 kBlue = 0x3B8CFF, kBlueDark = 0x2160D8, kBlueDeep = 0x16388A, kBlueLight = 0xA6D2FF;
constexpr u32 kRed = 0xE8343C, kRedDark = 0xA81B2C, kRedDeep = 0x5A0D19;
constexpr u32 kGoldC = 0xFFC83A, kGoldDarkC = 0xD98E14, kGoldDeep = 0x8A5208, kGoldLight = 0xFFE28A;
constexpr u32 kGun = 0x3A3344, kWoodC = 0x7A5030, kSkin = 0xF0BE92;
constexpr u32 kVioletC = 0x8A5CFF, kVioletDark = 0x5A34C8, kVioletLight = 0xCDBBFF;
constexpr u32 kOgre = 0x86BE5C, kOgreDark = 0x5E9A42, kIvoryC = 0xFFF6E0;
constexpr u32 kWhiteC = 0xFFFFFF, kSmoke = 0xF2ECE2, kSpark = 0xFFF2A0;

struct RoadLook {
    u32 stoneA, stoneB, mortar, curb, grassA, grassB, haze;
    float speckle;
};
const RoadLook kLooks[kWorldCount] = {
    {0xCDBFA8, 0xA8977E, 0x76674F, 0xEEE4D0, 0x86C95A, 0x62AE46, 0xD4ECF2, 0.5f},  // Greenmeadow
    {0xE8C690, 0xCCA36A, 0x98744A, 0xF6E2B6, 0xF2CD80, 0xE2B062, 0xF9E6C0, 0.3f},  // Sunbaked
    {0xC2CEDA, 0x9EAEC0, 0x6C7C90, 0xF6FAFE, 0xF4F8FC, 0xD6E4F2, 0xEAF4FC, 0.0f},  // Frostvale
    {0xCDB89E, 0xAA937A, 0x74604A, 0xEEDFC8, 0xE0A050, 0xC67E34, 0xF8DEC2, 0.4f},  // Emberfall
    {0xDCD4C4, 0xBBB19F, 0x8A806E, 0xFAF4E8, 0xF4DFAA, 0xE8CB8E, 0xD2F2F8, 0.5f},  // Tidereach
    {0xDACACA, 0xBCA8AA, 0x8E7A7E, 0xF8EEF0, 0x9CD472, 0x80C05C, 0xFAE8F0, 0.9f},  // Bloomgrove
    {0xACAC94, 0x8E8E78, 0x5E5E4C, 0xD4D4BC, 0x66A676, 0x4E8A5E, 0xDCE8D8, 0.2f},  // Mirefen
    {0xD4DCE8, 0xB2BCCC, 0x7E889A, 0xFFFFFF, 0x92DA88, 0x72C870, 0xDCF2FE, 0.6f},  // Skyhold
    {0x7A6E7C, 0x5E5464, 0x2E2830, 0xA898AA, 0x584650, 0x46343E, 0xA88088, 0.0f},  // Cinderpeak
    {0xBCA4B4, 0x9C8492, 0x5E4A58, 0xECDAE6, 0x74A070, 0x5C845A, 0xEDCDDC, 0.7f},  // Lanternfair
};

u32 mixc(u32 a, u32 b, float t) {
    t = clampv(t, 0.0f, 1.0f);
    u32 out = 0;
    for (int s = 0; s <= 16; s += 8) {
        const float ca = float((a >> s) & 0xFF), cb = float((b >> s) & 0xFF);
        out |= u32(ca + (cb - ca) * t + 0.5f) << s;
    }
    return out;
}
u32 hash32(u32 x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
float h01(u32 x) { return float(hash32(x) & 0xFFFFFF) / 16777216.0f; }

Sticker sticker(Shape shape, float x, float y, float hw, float hh, u32 c0, u32 c1, u32 c2, float alpha = 1.0f) {
    Sticker s = makeSticker(shape, x, y, hw, hh, c0, alpha);
    setColor(s.c1, c1, alpha);
    setColor(s.c2, c2, alpha);
    return s;
}

}  // namespace

bool Camera::project(float x, float zw, float y, float& sx, float& sy, float& scale) const {
    const float t = zw - z;
    const float depth = (h - y) * sn + t * cs;
    if (depth < 0.5f) return false;
    const float up = (y - h) * cs + t * sn;
    scale = f / depth;
    sx = cx + x * scale;
    sy = cy - up * scale;
    return true;
}

float Field::rnd() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return float(rng_ & 0xFFFFFF) / 16777216.0f;
}

void Field::init(const Layout& L) {
    L_ = L;
    layoutCamera();
}

void Field::resize(const Layout& L) {
    L_ = L;
    layoutCamera();
}

void Field::setArt(Art* art) {
    art_ = art && art->ready() ? art : nullptr;
    ids_ = Ids{};
    partCount_ = partHead_ = 0;  // particles may name the old art's anims
    if (!art_) return;
    auto kind = [&](const char* name) { return art_->findKind(name); };
    ids_.soldier = kind("soldier.march");
    ids_.foot = kind("foot.march");
    ids_.rider = kind("rider.march");
    ids_.brute = kind("brute.march");
    ids_.boss = kind("boss.march");
    for (int w = 0; w < 10; ++w) {
        char own[32];
        std::snprintf(own, sizeof own, "boss_w%d.march", w + 1);
        const int b = kind(own);
        ids_.bossOf[w] = b >= 0 ? b : ids_.boss;
    }
    if (ids_.boss < 0) ids_.boss = ids_.bossOf[0];
    ids_.barrel = kind("barrel");
    ids_.banner = kind("squad.banner.march");
    ids_.cheer = kind("soldier.cheer");
    ids_.muzzle = kind("fx.muzzle");
    ids_.tracer = kind("fx.tracer");
    ids_.hit = kind("fx.hit");
    ids_.poofRed = kind("fx.poof_red");
    ids_.poofBlue = kind("fx.poof_blue");
    ids_.debris = kind("fx.debris");
    ids_.explosion = kind("fx.explosion");
    ids_.explosionGlow = kind("fx.explosion_glow");
    ids_.bigboom = kind("fx.bigboom");
    ids_.bigboomGlow = kind("fx.bigboom_glow");
    ids_.ring = kind("fx.ring");
    ids_.dust = kind("fx.dust");
    ids_.sparkle = kind("fx.sparkle");
    const char* sides[3] = {"add", "mul", "neg"};
    char name[48];
    for (int i = 0; i < 3; ++i) {
        std::snprintf(name, sizeof name, "fx.gatepass_%s", sides[i]);
        ids_.gatePass[i] = kind(name);
        std::snprintf(name, sizeof name, "gate.post_%s", sides[i]);
        ids_.gatePost[i] = kind(name);
    }
    ids_.gateSpikes = kind("gate.spikes");
    for (int i = 0; i < 6; ++i) {
        std::snprintf(name, sizeof name, "fx.confetti_%c", 'a' + i);
        const int c = kind(name);
        if (c >= 0) ids_.confetti[ids_.confettiCount++] = c;
    }
    PM_LOG("field: art: soldier %d foot %d rider %d brute %d boss %d barrel %d gate posts %d %d %d; fx muzzle %d poof %d %d boom %d, "
           "%d confetti",
           ids_.soldier, ids_.foot, ids_.rider, ids_.brute, ids_.boss, ids_.barrel, ids_.gatePost[0], ids_.gatePost[1], ids_.gatePost[2],
           ids_.muzzle, ids_.poofRed, ids_.poofBlue, ids_.bigboom, ids_.confettiCount);
}

void Field::castShadow(float x, float z, float height, float radius) {
    if (casterN_ >= kCasters) return;
    const float dl = std::sqrt(shDx_ * shDx_ + shDz_ * shDz_);
    const float ux = dl > 0.01f ? shDx_ / dl : 0.0f, uz = dl > 0.01f ? shDz_ / dl : 1.0f;
    const float half = maxv(radius, 0.5f * height * dl);  // along the sun
    const float cx = x + shDx_ * height * 0.5f, cz = z + shDz_ * height * 0.5f;
    float c0x, c0y, a1x, a1y, a2x, a2y, b1x, b1y, b2x, b2y, s;
    if (!cam_.project(cx, cz, 0, c0x, c0y, s) || !cam_.project(cx - ux * half, cz - uz * half, 0, a1x, a1y, s) ||
        !cam_.project(cx + ux * half, cz + uz * half, 0, a2x, a2y, s) || !cam_.project(cx - uz * radius, cz + ux * radius, 0, b1x, b1y, s) ||
        !cam_.project(cx + uz * radius, cz - ux * radius, 0, b2x, b2y, s))
        return;
    const float ax = a2x - a1x, ay = a2y - a1y;
    const float hw = 0.5f * std::sqrt(ax * ax + ay * ay), hh = maxv(0.8f, 0.5f * std::sqrt((b2x - b1x) * (b2x - b1x) + (b2y - b1y) * (b2y - b1y)));
    if (c0x + hw < -20 || c0x - hw > float(L_.w) + 20 || c0y + hw < -20 || c0y - hw > float(L_.h) + 20) return;
    Sticker sh = makeSticker(kShEllipse, c0x, c0y, maxv(hw, 0.8f), hh, 0x000000);
    sh.rot = std::atan2(ay, ax);
    sh.soft = maxv(1.0f, hh * 0.7f);
    sh.lift = 0.01f;
    casters_[casterN_++] = sh;
}

void Field::dying(int anim, float x, float z, float knock, float seconds, float scale) {
    if (anim < 0) return;
    Particle& p = parts_[partHead_];
    partHead_ = (partHead_ + 1) % kParticles;
    partCount_ = minv(partCount_ + 1, kParticles);
    p = Particle{};
    p.x = x;
    p.z = z;
    p.vz = knock / maxv(0.05f, seconds);
    p.max = p.life = seconds;
    p.size = scale;
    p.rot = rnd() * 9.0f;  // its animation phase
    p.kind = 8;
    p.anim = i16(anim);
}

void Field::confetti(float x, float z, float y, int n, float spread) {
    if (ids_.confettiCount == 0) return;
    if (reduceEffects) n = (n + 2) / 3;
    for (int i = 0; i < n; ++i) {
        Particle& p = parts_[partHead_];
        partHead_ = (partHead_ + 1) % kParticles;
        partCount_ = minv(partCount_ + 1, kParticles);
        p = Particle{};
        p.x = x + (rnd() - 0.5f) * spread;
        p.z = z + (rnd() - 0.5f) * spread * 0.6f;
        p.y = y + rnd();
        p.vx = (rnd() - 0.5f) * 4.0f;
        p.vz = (rnd() - 0.5f) * 3.0f;
        p.vy = 3.0f + rnd() * 4.0f;
        p.max = p.life = 1.6f + rnd() * 1.2f;
        p.size = 1.0f;
        p.rot = rnd() * kPi;
        p.vr = (rnd() - 0.5f) * 10.0f;
        p.kind = 5;
        p.anim = i16(ids_.confetti[i % ids_.confettiCount]);
    }
}

Field::Placed Field::sprite(int anim, float x, float z, float y, float t, float scale, float alpha, float rot, bool mirror, float flash) {
    Placed out;
    float sx, sy, s;
    if (anim < 0 || !art_ || !cam_.project(x, z, y, sx, sy, s)) return out;
    out.elev = std::atan2(cam_.h - y, maxv(0.01f, z - cam_.z)) * (180.0f / kPi);
    const SpriteFrame& fr = art_->frame(anim, out.elev, t);
    const float k = fr.height * s * scale / fr.h;
    out.sx = sx;
    out.sy = sy;
    out.k = k;
    out.s = s;
    out.fr = &fr;
    const float hw = fr.w * 0.5f * k, hh = fr.h * 0.5f * k;
    float ox = (fr.w * 0.5f - fr.px) * k, oy = (fr.h * 0.5f - fr.py) * k;
    if (mirror) ox = -ox;
    if (rot != 0.0f) {
        const float c = std::cos(rot), sn = std::sin(rot);
        const float rx = c * ox - sn * oy, ry = sn * ox + c * oy;
        ox = rx;
        oy = ry;
    }
    const float cx = sx + ox, cy = sy + oy, r = maxv(hw, hh) * 1.42f;
    if (cx + r < 0 || cx - r > float(L_.w) || cy + r < 0 || cy - r > float(L_.h) || hh < 0.4f) return out;
    Sticker* st = add(z, 1);
    if (!st) return out;
    Sticker& q = *st;
    q = Sticker{};
    q.x = cx;
    q.y = cy;
    q.hw = mirror ? -hw : hw;
    q.hh = hh;
    q.rot = rot;
    q.shape = float(kShSprite);
    q.soft = fr.layer;
    q.p0 = fr.u0;
    q.p1 = fr.v0;
    q.p2 = fr.u1;
    q.fold = fr.v1;
    setColor(q.c0, 0xFFFFFF, alpha);
    setColor(q.c1, 0xFFFFFF, flash);
    q.c2[3] = art_->anim(anim).additive ? 255 : 0;
    out.st = st;
    return out;
}

Sticker* Field::spriteAt(int anim, float key, float sx, float sy, float s, float t, float scale, float alpha, float rot) {
    if (anim < 0 || !art_) return nullptr;
    const SpriteFrame& fr = art_->frame(anim, 0, t);
    const float k = fr.height * s * scale / fr.h;
    float ox = (fr.w * 0.5f - fr.px) * k, oy = (fr.h * 0.5f - fr.py) * k;
    if (rot != 0.0f) {
        const float c = std::cos(rot), sn = std::sin(rot);
        const float rx = c * ox - sn * oy, ry = sn * ox + c * oy;
        ox = rx;
        oy = ry;
    }
    Sticker* st = add(key, 1);
    if (!st) return nullptr;
    Sticker& q = *st;
    q = Sticker{};
    q.x = sx + ox;
    q.y = sy + oy;
    q.hw = fr.w * 0.5f * k;
    q.hh = fr.h * 0.5f * k;
    q.rot = rot;
    q.shape = float(kShSprite);
    q.soft = fr.layer;
    q.p0 = fr.u0;
    q.p1 = fr.v0;
    q.p2 = fr.u1;
    q.fold = fr.v1;
    setColor(q.c0, 0xFFFFFF, alpha);
    setColor(q.c1, 0xFFFFFF, 0.0f);
    q.c2[3] = art_->anim(anim).additive ? 255 : 0;
    return st;
}

void Field::flipbook(int anim, float x, float z, float y, float scale, float rot, float delay) {
    if (anim < 0 || !art_) return;
    const SpriteAnim& a = art_->anim(anim);
    Particle& p = parts_[partHead_];
    partHead_ = (partHead_ + 1) % kParticles;
    partCount_ = minv(partCount_ + 1, kParticles);
    p = Particle{};
    p.x = x;
    p.z = z;
    p.y = y;
    p.max = float(a.frames) / maxv(1.0f, a.fps);
    p.life = p.max + delay;
    p.size = scale;
    p.rot = rot;
    p.kind = 6;
    p.anim = i16(anim);
}

// The world's look from the art: its ground textures, haze, sky, and the post pass's glow, grade and vignette.
void Field::drawGround(PaperFrame& f) {
    f.spriteTex = art_ ? art_->sprites() : 0;
    f.post = PostParams{};
    GroundParams& g = f.ground;
    g.roadTex = g.landTex = 0;
    if (!art_ || !def_) return;
    const int world = maxv<int>(def_->world, 1);
    const WorldArt& w = art_->world(world);
    if (!w.defined) return;
    if (art_->useWorld(world)) {
        g.roadTex = art_->road();
        g.landTex = art_->land();
        g.roadWidth = w.roadWidth;
        g.roadTile = w.roadTile;
        g.landTile = w.landTile;
    }
    g.haze = w.haze;
    g.hazeStart = w.hazeStart;
    g.hazeEnd = w.hazeEnd;
    g.skyTop = w.skyTop;
    g.skyHorizon = w.skyHorizon;
    g.backdropTex = art_->backdrop();
    g.backdropHeight = w.backdropHeight * float(L_.w);
    g.backdropShift = -0.02f * viewX_;
    f.post.on = true;
    f.post.threshold = w.bloomThreshold;
    f.post.bloom = w.bloomStrength;
    f.post.light = w.light;
    f.post.lightStrength = w.lightStrength;
    f.post.vignette = w.vignette;
    f.post.contrast = w.contrast;
    f.post.saturation = w.saturation;
    f.post.hudTop = (L_.top() + L_.px(kTopBandDp) * 0.7f) / float(maxv(1, L_.h));
    f.post.edge = edge_ * (reduceEffects ? 0.15f : 0.4f);
    shDx_ = w.shadowDx;
    shDz_ = w.shadowDz;
    shAlpha_ = w.shadowAlpha;
    shColor_ = w.shadowColor;
}

// Props beside the road from the world's rules, the same at every visit (hashed from the rule and the road's cells).
void Field::sceneryArt(float zLo, float zHi) {
    const WorldArt& w = art_->world(maxv<int>(def_->world, 1));
    for (int ri = 0; ri < w.propCount; ++ri) {
        const PropRule& r = w.props[ri];
        for (int cell = int(std::floor(zLo / r.spacing)); cell <= int(std::floor(zHi / r.spacing)); ++cell) {
            for (int side = -1; side <= 1; side += 2) {
                const u32 h = hash32(u32(cell * 2 + (side > 0)) * 2654435761u + u32(ri + 1) * 40503u + u32(def_->world) * 977u);
                if (h01(h ^ 0xA5u) > minv(1.0f, r.weight)) continue;
                const float x = float(side) * (r.xMin + (r.xMax - r.xMin) * h01(h));
                const float z = (float(cell) + h01(h ^ 0x3Cu)) * r.spacing;
                if (z < zLo || z > zHi) continue;
                const float scale = r.scaleMin + (r.scaleMax - r.scaleMin) * h01(h ^ 0x77u);
                const Placed pl = sprite(r.anim, x, z, 0, time + h01(h ^ 0x19u) * 3.0f, scale, 1.0f);
                if (pl.st) {
                    const float unit = pl.fr->height * scale / pl.fr->h;  // road units per atlas pixel
                    castShadow(x, z, pl.fr->py * unit, pl.fr->w * unit * 0.3f);
                }
            }
        }
    }
    for (int ei = 0; ei < w.edgeCount; ++ei) {
        const EdgeRule& e = w.edges[ei];
        for (int cell = int(std::ceil(zLo / e.spacing)); float(cell) * e.spacing <= zHi; ++cell) {
            for (int side = -1; side <= 1; side += 2) {
                const Placed pl = sprite(e.anim, float(side) * e.x, float(cell) * e.spacing, 0, time + float(cell) * 0.37f, 1.0f, 1.0f);
                if (pl.st) {
                    const float unit = pl.fr->height / pl.fr->h;
                    castShadow(float(side) * e.x, float(cell) * e.spacing, pl.fr->py * unit, pl.fr->w * unit * 0.2f);
                }
            }
        }
    }
}

// The world's drifting particles (leaves, snow, embers), spawned over the road ahead.
void Field::ambientArt(float dt) {
    if (!art_ || !def_) return;
    const WorldArt& w = art_->world(maxv<int>(def_->world, 1));
    for (int i = 0; i < w.ambientCount && i < WorldArt::kMaxAmbient; ++i) {
        ambientClock_[i] += dt * w.ambient[i].perSecond * (reduceEffects ? 0.4f : 1.0f);
        while (ambientClock_[i] >= 1.0f) {
            ambientClock_[i] -= 1.0f;
            Particle& p = parts_[partHead_];
            partHead_ = (partHead_ + 1) % kParticles;
            partCount_ = minv(partCount_ + 1, kParticles);
            p = Particle{};
            p.x = (rnd() - 0.5f) * 26.0f;
            p.z = viewZ_ + 2.0f + rnd() * 50.0f;
            p.y = 5.0f + rnd() * 6.0f;
            p.vx = (rnd() - 0.3f) * 1.2f;
            p.vz = -0.5f - rnd();
            p.vy = -0.8f - rnd() * 0.8f;
            p.max = p.life = 5.0f + rnd() * 3.0f;
            p.size = 0.7f + rnd() * 0.6f;
            p.rot = rnd() * 6.28f;
            p.vr = (rnd() - 0.5f) * 3.0f;
            p.kind = 7;  // drifting: drawn with its anim looping, fading at both ends
            p.anim = i16(w.ambient[i].anim);
        }
    }
}

void Field::layoutCamera() {
    const float W = float(L_.w), H = float(L_.h);
    const float th = tune.camPitch * kPi / 180.0f;
    cam_.sn = std::sin(th);
    cam_.cs = std::cos(th);
    cam_.h = tune.camHeight;
    const float ts = tune.camBack;
    depthS_ = cam_.h * cam_.sn + ts * cam_.cs;
    const float up = (cam_.h * cam_.cs - ts * cam_.sn) / depthS_;
    const float roadPx = minv(tune.camRoad * W, tune.camRoadH * H);
    cam_.f = roadPx * depthS_ / (2.0f * sim::kRoadHalf);
    cam_.cx = W * 0.5f;
    cam_.cy = tune.camSquadY * H - cam_.f * up;
    auto tAt = [&](float sy) {
        const float k = (sy - cam_.cy) / cam_.f;
        const float den = k * cam_.cs + cam_.sn;
        return den > 0.004f ? minv(400.0f, cam_.h * (cam_.cs - k * cam_.sn) / den) : 400.0f;
    };
    cam_.tTop = tAt(0.0f);
    cam_.tBottom = tAt(H);
}

void Field::setLevel(const sim::LevelDef& d, int number) {
    def_ = &d;
    number_ = number;
    partCount_ = partHead_ = 0;
    for (Floater& f : floaters_) f.life = 0;
    for (float& f : flash_) f = 0;
    viewZ_ = viewX_ = lastZ_ = lastX_ = dz_ = dx_ = 0;
    shownSquad_ = float(d.start);
    badgePop_ = shake_ = bossSeen_ = 0;
    confettiT_ = -1;
    cueN_ = 0;
    shownBefore_ = 0;
    bossFrame_ = -1;
    bossTenth_ = 10;
    hitStop = zoom_ = edge_ = 0;
    for (float& g : gateUsedAt_) g = -1;
    for (float& c : ambientClock_) c = 0;
    for (float& c : cueClock_) c = -10;
    rng_ = 0x9E3779B9u ^ u32(number * 7919);
}

// ---- Events: what a step did becomes bursts, figures falling, numbers and cues ----------------------------------------

void Field::cue(u8 c, int step, float x) {
    static const float gap[20] = {0.075f, 0.05f, 0.035f, 0, 0, 0.06f, 0.07f, 0, 0, 0, 0, 0.09f, 0, 0.1f, 0, 0, 0, 0, 0, 0};
    if (c < 20 && gap[c] > 0) {
        if (time - cueClock_[c] < gap[c]) return;
        cueClock_[c] = time;
    }
    if (cueN_ < kCues) cues_[cueN_++] = {c, step, clampv(x / sim::kRoadHalf, -1.0f, 1.0f) * 0.6f};
}

int Field::takeCues(CueEvent* out, int max) {
    const int n = minv(cueN_, max);
    for (int i = 0; i < n; ++i) out[i] = cues_[i];
    cueN_ = 0;
    return n;
}

void Field::burst(float x, float z, float y, int n, u32 c0, u32 c1, float speed, float size, u8 kind) {
    if (reduceEffects) n = (n + 2) / 3;
    for (int i = 0; i < n; ++i) {
        Particle& p = parts_[partHead_];
        partHead_ = (partHead_ + 1) % kParticles;
        partCount_ = minv(partCount_ + 1, kParticles);
        const float a = rnd() * 2.0f * kPi, v = speed * (0.4f + 0.6f * rnd());
        p.x = x;
        p.z = z;
        p.y = y;
        p.vx = std::cos(a) * v;
        p.vz = std::sin(a) * v * 0.8f;
        p.vy = kind == 1 ? 0.6f + rnd() : (kind == 3 ? speed * (0.3f + rnd()) : speed * (0.5f + 0.8f * rnd()));
        p.max = p.life = kind == 1 ? 0.55f + 0.3f * rnd() : (kind == 4 ? 0.45f : 0.6f + 0.5f * rnd());
        p.size = size * (0.6f + 0.8f * rnd());
        p.rot = rnd() * kPi;
        p.vr = (rnd() - 0.5f) * 14.0f;
        p.c0 = rnd() < 0.5f ? c0 : c1;
        p.c1 = c1;
        p.shape = kind == 3 ? kShStar : (kind == 1 ? kShEllipse : (kind == 4 ? kShRing : (rnd() < 0.5f ? kShBox : kShTri)));
        p.kind = kind;
        p.anim = -1;
        if (kind == 4) {
            p.vx = p.vz = p.vy = 0;
            p.size = size;
        }
    }
}

void Field::figureFalls(float x, float z, float size, u32 c0, u32 c1, u32 c2, float dir, int anim) {
    Particle& p = parts_[partHead_];
    p.anim = i16(anim);
    partHead_ = (partHead_ + 1) % kParticles;
    partCount_ = minv(partCount_ + 1, kParticles);
    p.x = x;
    p.z = z;
    p.y = 0;
    p.vx = dir * (0.6f + rnd());
    p.vz = 1.2f + rnd();
    p.vy = 2.6f + rnd();
    p.max = p.life = 0.75f;
    p.size = size;
    p.rot = 0;
    p.vr = dir * (5.0f + 3.0f * rnd());
    p.c0 = c0;
    p.c1 = c1;
    p.kind = 2;
    p.shape = c2 == kBlueDeep ? 1 : 0;  // 1: one of ours (a gold belt), 0 the horde's
}

void Field::floater(float x, float z, float y, const char* text, u32 fill, u32 stroke, float size, bool follow) {
    Floater& f = floaters_[floaterHead_];
    floaterHead_ = (floaterHead_ + 1) % kFloaters;
    f.follow = follow;
    f.x = x;
    f.z = z;
    f.y = y;
    f.life = 1.1f;
    f.fill = fill;
    f.stroke = stroke;
    f.size = size;
    std::snprintf(f.text, sizeof f.text, "%s", text);
}

void Field::celebrate() {
    confettiT_ = 0;
    if (ids_.confettiCount > 0) {
        confetti(viewX_, viewZ_ + 6.0f, 5.0f, 160, 10.0f);
        return;
    }
    const u32 cs[5] = {kGoldC, kBlue, 0xFF6FA8, 0x4ED8A0, kWhiteC};
    for (int i = 0; i < (reduceEffects ? 20 : 70); ++i) {
        Particle& p = parts_[partHead_];
        partHead_ = (partHead_ + 1) % kParticles;
        partCount_ = minv(partCount_ + 1, kParticles);
        p.x = viewX_ + (rnd() - 0.5f) * 8.0f;
        p.z = viewZ_ + 2.0f + rnd() * 8.0f;
        p.y = 4.0f + rnd() * 4.0f;
        p.vx = (rnd() - 0.5f) * 3.0f;
        p.vz = (rnd() - 0.5f) * 2.0f;
        p.vy = 2.0f + rnd() * 3.0f;
        p.max = p.life = 2.2f + rnd();
        p.size = 0.16f + 0.08f * rnd();
        p.rot = rnd() * kPi;
        p.vr = (rnd() - 0.5f) * 10.0f;
        p.c0 = cs[i % 5];
        p.c1 = mixc(cs[i % 5], 0x000000, 0.25f);
        p.shape = kShBox;
        p.kind = 5;  // confetti: falls slowly, flutters
        p.anim = -1;
    }
}

void Field::events(const sim::Battle& b, const sim::BattleEvents& ev) {
    dz_ = b.z - lastZ_;
    dx_ = b.x - lastX_;
    lastZ_ = b.z;
    lastX_ = b.x;
    if (!def_) return;
    const int shown = minv(kMaxShown, minv(int(tune.soldiersShown), maxv<i32>(b.squad, 0)));
    if (ev.shots > 0) {
        for (int i = 0; i < minv(ev.shots, 3) && shown > 0; ++i) flash_[int(rnd() * float(shown)) % shown] = 0.07f;
        cue(kCueShot, ev.shots, b.x);
    }
    if (ev.hits > 0) cue(kCueHit, 0, b.x);
    for (int i = 0; i < ev.killN; ++i) {
        const u8 kind = ev.killKind[i];
        if (kind == sim::kBoss) continue;
        const float size = kind == sim::kBrute ? 1.8f : (kind == sim::kRider ? 1.25f : 1.0f);
        const int figure = kind == sim::kBrute ? ids_.brute : (kind == sim::kRider ? ids_.rider : ids_.foot);
        if (figure >= 0) {  // the art: knocked back 0.3, shrinks and fades into a puff of its colour
            dying(figure, ev.killX[i], ev.killZ[i], 0.3f, 0.12f);
            flipbook(ids_.poofRed, ev.killX[i], ev.killZ[i] - 0.05f, 0.0f, kind == sim::kBrute ? 1.6f : 1.0f);
        } else {
            figureFalls(ev.killX[i], ev.killZ[i], size, kRed, kRedDark, kRedDeep, ev.killX[i] < b.x ? -1.0f : 1.0f);
            burst(ev.killX[i], ev.killZ[i], 0.5f, kind == sim::kBrute ? 8 : 2, kRed, kRedDark, 3.5f, 0.11f, 0);
        }
        cue(kCueKill, kind, ev.killX[i]);
    }
    // The boss: a blast on its body for every tenth of its strength gone.
    if (const int bi0 = b.bossIndex(); bi0 >= 0 && ids_.explosion >= 0 && def_->bossHp > 0) {
        const sim::Enemy& e = b.enemies[bi0];
        const int tenth = int(std::ceil(10.0f * float(e.hp) / float(def_->bossHp)));
        if (tenth < bossTenth_) {
            bossTenth_ = tenth;
            flipbook(ids_.explosion, e.x + (rnd() - 0.5f) * 2.4f, e.z - 0.4f, 1.0f + rnd() * 2.2f, 0.6f);
            shake_ = maxv(shake_, 0.25f);
        }
    }
    if (ids_.dust >= 0 && b.started && !b.won && !b.lost && rnd() < (reduceEffects ? 0.06f : 0.18f)) {
        const float r = b.radius();
        flipbook(ids_.dust, b.x + (rnd() - 0.5f) * r * 1.6f, b.z - r * 0.7f * rnd(), 0.0f, 0.7f + 0.5f * rnd());
    }
    // Gates and barrels hit this step (their clocks were just reset).
    for (int g = 0; g < def_->gateCount; ++g) {
        const sim::GateState& gs = b.gates[g];
        if (gs.used || gs.hit > sim::kDt * 1.5f) continue;
        cue(kCueGrow, minv(gs.value, 24), (def_->gates[g].x0 + def_->gates[g].x1) * 0.5f);
        if (ids_.hit >= 0 && rnd() < 0.5f) {
            const sim::GateDef& gd = def_->gates[g];
            flipbook(ids_.hit, gd.x0 + 0.3f + rnd() * (gd.x1 - gd.x0 - 0.6f), gd.z - 0.05f, 0.5f + rnd() * 1.4f, 1.0f, rnd() * 6.28f);
        }
    }
    for (int k = 0; k < def_->barrelCount; ++k) {
        const sim::BarrelState& bs = b.barrels[k];
        if (bs.broken || bs.hit > sim::kDt * 1.5f) continue;
        cue(kCueBarrelHit, 0, def_->barrels[k].x);
        if (rnd() < 0.3f) burst(def_->barrels[k].x, def_->barrels[k].z - 0.4f, 0.8f, 1, kGoldC, kGoldDarkC, 2.5f, 0.09f, 0);
    }
    char t[24];
    if (ev.gate >= 0) {
        const sim::GateDef& g = def_->gates[ev.gate];
        const i32 delta = ev.gateAfter - ev.gateBefore;
        const bool good = delta >= 0;
        if (g.kind == sim::kGateMul) std::snprintf(t, sizeof t, "\xC3\x97%d", b.gates[ev.gate].value);
        else std::snprintf(t, sizeof t, "%+d", b.gates[ev.gate].value);
        floater(0, 1.0f, 4.4f, t, good ? kWhiteC : 0xFF7A86, 0x16224F, good ? 1.1f : 1.0f, true);
        const u32 c = g.kind == sim::kGateMul ? kVioletC : (b.gates[ev.gate].value > 0 ? kBlue : kRed);
        gateUsedAt_[ev.gate] = time;
        const int side = g.kind == sim::kGateMul ? 1 : (ev.gateBefore >= 0 && b.gates[ev.gate].value >= 0 ? 0 : 2);
        if (ids_.gatePass[side] >= 0) {
            flipbook(ids_.gatePass[side], (g.x0 + g.x1) * 0.5f, g.z - 0.05f, 1.2f, 1.0f);
            if (side < 2) {
                confetti((g.x0 + g.x1) * 0.5f, g.z, 1.4f, 24, g.x1 - g.x0);
            } else {
                for (int i = 0; i < 10; ++i) flipbook(ids_.poofBlue, b.x + (rnd() - 0.5f) * b.radius() * 2.0f, b.z + (rnd() - 0.5f) * b.radius(), 0, 1.0f);
                edge_ = 1.0f;
            }
        } else {
            burst(b.x, g.z, 1.0f, 12, c, mixc(c, kWhiteC, 0.5f), 4.0f, 0.13f, 0);
        }
        if (good && ids_.sparkle >= 0)  // the new recruits arrive in sparkles
            for (int i = 0; i < 6; ++i) flipbook(ids_.sparkle, b.x + (rnd() - 0.5f) * b.radius() * 1.8f, b.z + (rnd() - 0.5f) * b.radius(), 0.6f, 1.0f);
        int step = 0;
        for (i32 v = maxv<i32>(1, delta < 0 ? -delta : delta); v > 1 && step < 14; v /= 2) ++step;
        cue(good ? kCueGate : kCueGateBad, step, b.x);
        badgePop_ = 1;
    }
    if (ev.barrel >= 0) {
        const sim::BarrelDef& bd = def_->barrels[ev.barrel];
        std::snprintf(t, sizeof t, "+%d", ev.barrelReward);
        floater(bd.x, bd.z, 1.8f, t, kWhiteC, 0x16224F, 1.0f);
        if (ids_.explosion >= 0) {  // the art: a blast with its glow, a ring on the road, planks flying
            flipbook(ids_.explosionGlow, bd.x, bd.z - 0.12f, 0.0f, 1.0f);
            flipbook(ids_.explosion, bd.x, bd.z - 0.1f, 0.0f, 1.0f);
            flipbook(ids_.ring, bd.x, bd.z - 0.2f, 0.02f, 1.0f);
            if (ids_.debris >= 0) {
                const int first = partHead_;
                burst(bd.x, bd.z, 0.7f, 8, kWoodC, kWoodC, 6.5f, 1.0f, 0);
                for (int i = 0; i < (reduceEffects ? 3 : 8); ++i) parts_[(first + i) % kParticles].anim = i16(ids_.debris);
            }
            if (ids_.sparkle >= 0)
                for (int i = 0; i < 5; ++i) flipbook(ids_.sparkle, b.x + (rnd() - 0.5f) * b.radius() * 1.8f, b.z + (rnd() - 0.5f) * b.radius(), 0.6f, 1.0f, 0, 0.15f);
        } else {
            burst(bd.x, bd.z, 0.7f, 16, kGoldC, kGoldDarkC, 6.0f, 0.15f, 0);
            burst(bd.x, bd.z, 0.6f, 5, kSmoke, 0xE0D6C8, 1.0f, 0.7f, 1);
            burst(bd.x, bd.z, 0.05f, 1, 0xFFB040, 0xFFB040, 0, 1.0f, 4);
            burst(bd.x, bd.z, 0.8f, 8, kSpark, 0xFFB040, 5.0f, 0.12f, 3);
        }
        shake_ = maxv(shake_, 0.35f);
        cue(kCueBarrel, 0, bd.x);
        badgePop_ = 1;
    }
    if (ev.crashed >= 0) {
        const sim::BarrelDef& bd = def_->barrels[ev.crashed];
        burst(bd.x, bd.z, 0.7f, 12, kGoldC, kGoldDarkC, 5.0f, 0.15f, 0);
        burst(bd.x, bd.z, 0.6f, 4, kSmoke, 0xE0D6C8, 1.0f, 0.6f, 1);
        shake_ = maxv(shake_, 0.3f);
        edge_ = 1.0f;
        cue(kCueCrash, 0, bd.x);
    }
    if (ev.wave >= 0) cue(kCueWave, ev.wave, 0);
    if (ev.lost > 0 && !ev.lostAll) {
        const float r = b.radius();
        for (int i = 0; i < minv(ev.lost, 4); ++i) {
            const float a = kPi * (0.15f + 0.7f * rnd());
            const float x = b.x + std::cos(a) * r, z = b.z + std::sin(a) * r * 0.8f;
            if (ids_.soldier >= 0) {
                dying(ids_.soldier, x, z, -0.3f, 0.12f);
                flipbook(ids_.poofBlue, x, z - 0.05f, 0.0f, 1.0f);
            } else {
                figureFalls(x, z, 0.9f, kBlue, kBlueDark, kBlueDeep, rnd() < 0.5f ? -1.0f : 1.0f);
            }
        }
        if (ev.gate < 0) {
            std::snprintf(t, sizeof t, "-%d", ev.lost);
            if (ev.crashed >= 0) floater(0, 1.0f, 4.4f, t, 0xFF7A86, 0x16224F, 1.0f, true);
        }
        cue(kCueLoss, ev.lost, b.x);
    }
    const int bi = b.bossIndex();
    if (bi >= 0 && bossSeen_ == 0 && b.enemies[bi].z - b.z < 60.0f) {
        bossSeen_ = 1;
        cue(kCueBoss, 0, b.enemies[bi].x);
    }
    if (ev.bossHit && bi >= 0) {
        cue(kCueBossHit, 0, b.enemies[bi].x);
        if (rnd() < 0.35f) burst(b.enemies[bi].x + (rnd() - 0.5f) * 2.0f, b.enemies[bi].z - 1.6f, 1.5f + rnd() * 2.0f, 2, kSpark, 0xFFB040, 3.0f, 0.12f, 3);
    }
    if (ev.bossDown) {
        // The boss is gone from the battle's list now: the burst goes where the squad was looking.
        const float bz = b.z + 7.0f;
        if (ids_.bigboom >= 0) {
            // Three blasts on its body over 0.6 s, then the big one with its glow and a ring; it shrinks away; a beat of
            // stillness and the camera pulls back.
            for (int i = 0; i < 3; ++i)
                flipbook(ids_.explosion, b.x + (rnd() - 0.5f) * 2.6f, bz - 0.3f, 0.8f + rnd() * 2.4f, 0.8f, 0, 0.2f * float(i));
            flipbook(ids_.bigboomGlow, b.x, bz - 0.25f, 0.0f, 1.0f, 0, 0.6f);
            flipbook(ids_.bigboom, b.x, bz - 0.2f, 0.0f, 1.0f, 0, 0.6f);
            flipbook(ids_.ring, b.x, bz - 0.3f, 0.02f, 1.0f, 0, 0.6f);
            dying(ids_.bossOf[clampv<int>(def_->world, 1, 10) - 1], b.x, bz, 0.0f, 0.75f);
            hitStop = 0.15f;
            zoom_ = 1.0f;
        } else {
            burst(b.x, bz, 2.0f, 30, kOgre, kRedDark, 8.0f, 0.22f, 0);
            burst(b.x, bz, 1.5f, 10, kSmoke, 0xE0D6C8, 1.6f, 1.4f, 1);
            burst(b.x, bz, 0.05f, 1, 0xFFB040, 0xFFB040, 0, 2.6f, 4);
            burst(b.x, bz, 2.0f, 16, kSpark, 0xFFB040, 7.0f, 0.18f, 3);
            figureFalls(b.x, bz, 4.6f, kOgre, kOgreDark, kRedDeep, 1.0f);
        }
        shake_ = 1.0f;
        cue(kCueBossDown, 0, b.x);
    }
    if (ev.won) cue(kCueWin, 0, 0);
    if (ev.lostAll) {
        cue(kCueLose, 0, 0);
        for (int i = 0; i < 5; ++i) {
            const float x = b.x + (rnd() - 0.5f) * 2.0f, z = b.z + rnd();
            if (ids_.soldier >= 0) {
                dying(ids_.soldier, x, z, -0.3f, 0.2f);
                flipbook(ids_.poofBlue, x, z - 0.05f, 0.0f, 1.0f, 0, 0.05f * float(i));
            } else {
                figureFalls(x, z, 0.9f, kBlue, kBlueDark, kBlueDeep, rnd() < 0.5f ? -1.0f : 1.0f);
            }
        }
    }
}

void Field::update(float dt, const sim::Battle& b) {
    if (hitStop > 0) {  // the beat of stillness after the boss's fall: everything nearly stops
        hitStop = maxv(0.0f, hitStop - dt);
        dt *= 0.05f;
    }
    time += dt;
    const float target = float(b.squad);
    shownSquad_ += (target - shownSquad_) * minv(1.0f, dt * 9.0f);
    if (std::fabs(target - shownSquad_) < 0.5f) shownSquad_ = target;
    badgePop_ = maxv(0.0f, badgePop_ - dt * (ids_.banner >= 0 ? 8.0f : 3.5f));
    if (ids_.banner >= 0 && int(shownSquad_ + 0.5f) != lastShown_) {
        lastShown_ = int(shownSquad_ + 0.5f);
        badgePop_ = 1;
    }
    shake_ = maxv(0.0f, shake_ - dt * 2.2f);
    zoom_ = maxv(0.0f, zoom_ - dt * 0.6f);
    edge_ = maxv(0.0f, edge_ - dt / 0.3f);
    for (float& f : flash_) f = maxv(0.0f, f - dt);
    for (int i = 0; i < partCount_; ++i) {
        Particle& p = parts_[i];
        if (p.life <= 0) continue;
        p.life -= dt;
        p.x += p.vx * dt;
        p.z += p.vz * dt;
        p.y += p.vy * dt;
        p.rot += p.vr * dt;
        if (p.kind == 0 || p.kind == 3) p.vy -= 14.0f * dt;
        else if (p.kind == 2) p.vy -= 12.0f * dt;
        else if (p.kind == 5) {
            p.vy = maxv(p.vy - 6.0f * dt, -1.2f);
            p.vx += std::sin(time * 5.0f + p.rot) * dt * 2.0f;
        } else if (p.kind == 7) {
            p.vx += std::sin(time * 1.7f + p.rot) * dt * 0.8f;
            if (p.y <= 0.02f) {  // landed: lies still and fades
                p.vx = p.vz = p.vy = p.vr = 0;
                p.life = minv(p.life, 0.6f);
                continue;
            }
        }
        if (p.y < 0) {
            p.y = 0;
            p.vy *= -0.25f;
            p.vx *= 0.5f;
            p.vz *= 0.5f;
            p.vr *= 0.5f;
        }
        if (p.kind == 2) p.rot = clampv(p.rot, -1.55f, 1.55f);
    }
    for (Floater& f : floaters_) {
        if (f.life <= 0) continue;
        f.life -= dt;
        f.y += dt * 2.2f;
    }
    ambientArt(dt);
}

// ---- The draw list ---------------------------------------------------------------------------------------------------

Sticker* Field::add(float key, int n) {
    if (poolN_ + n > kPool || itemN_ >= kItems) return nullptr;
    Item& it = items_[itemN_++];
    it.key = key;
    it.first = u16(poolN_);
    it.count = u16(n);
    Sticker* s = pool_ + poolN_;
    poolN_ += n;
    return s;
}

void Field::shadow(float sx, float sy, float hw, float hh) {
    if (casterN_ >= kCasters) return;
    Sticker s = makeSticker(kShEllipse, sx + hw * 0.18f, sy + hh * 0.15f, hw, hh, 0x000000);
    s.lift = 1.0f;
    casters_[casterN_++] = s;
}

void Field::label(UiBatch& b, const char* s, float x, float y, float size, u32 fill, u32 stroke) {
    b.text(s, x, y, size, rgba(fill), rgba(stroke), maxv(1.5f, size * 0.13f), Align::Center, true, 0);
}

// Trees, bushes and banners beside the road, the same at every visit (hashed from the road's cells).
void Field::scenery(float zLo, float zHi) {
    if (!def_) return;
    const WorldPalette& wp = worldPalette(def_->world);
    const int family = wp.trees;
    const float W = float(L_.w);
    for (int cell = int(std::floor(zLo / 4.5f)); cell <= int(std::floor(zHi / 4.5f)); ++cell) {
        for (int side = -1; side <= 1; side += 2) {
            const u32 h = hash32(u32(cell * 2 + (side > 0)) * 2654435761u + u32(def_->world) * 977u);
            const float r1 = float(h & 0xFFFF) / 65535.0f, r2 = float(h >> 16) / 65535.0f;
            const float z = float(cell) * 4.5f + r2 * 3.5f;
            if (z < zLo || z > zHi) continue;
            // A banner on a pole at the curb, every few cells.
            if (cell % 3 == 0) {
                float sx, sy, s;
                if (cam_.project(float(side) * 5.95f, float(cell) * 4.5f, 0, sx, sy, s) && sx > -40 && sx < W + 40) {
                    Sticker* st = add(float(cell) * 4.5f, 2);
                    if (st) {
                        st[0] = sticker(kShBox, sx, sy - 1.3f * s, maxv(1.0f, 0.06f * s), 1.3f * s, 0x5A4636, 0x3E2E22, 0x3E2E22);
                        st[1] = sticker(kShFlag, sx + 0.42f * s, sy - 2.2f * s, 0.46f * s, 0.26f * s, wp.accent, wp.accent, wp.accentDark);
                        st[1].p0 = time * 3.0f + float(cell);
                        shadow(sx, sy, 0.25f * s, 0.08f * s);
                    }
                }
            }
            const float x = float(side) * (7.0f + r1 * 5.5f);
            float sx, sy, s;
            if (!cam_.project(x, z, 0, sx, sy, s)) continue;
            const float size = 0.8f + 0.45f * h01(h ^ 0x55u);
            if (sx < -3.0f * s * size || sx > W + 3.0f * s * size) continue;
            const bool alt = (h >> 7) % 4 == 0;
            const u32 dark = alt ? wp.tree2Dark : wp.treeDark, light = alt ? wp.tree2Light : wp.treeLight;
            const float k = s * size;
            switch (family) {
                case 0:
                case 2: {  // pines (snowy in the north)
                    Sticker* st = add(z, 4);
                    if (!st) break;
                    st[0] = sticker(kShBox, sx, sy - 0.5f * k, 0.18f * k, 0.5f * k, 0x6E4A2E, 0x6E4A2E, 0x6E4A2E);
                    for (int i = 0; i < 3; ++i) {
                        const float w = (1.35f - 0.3f * float(i)) * k, hh = (0.85f - 0.1f * float(i)) * k;
                        st[1 + i] = sticker(kShTri, sx, sy - (1.6f + 0.95f * float(i)) * k, w, hh, family == 2 ? 0xF4F8FC : light, dark, dark);
                        st[1 + i].fold = 1;
                    }
                    shadow(sx, sy, 1.1f * k, 0.3f * k);
                    break;
                }
                case 1:
                case 4: {  // palms
                    Sticker* st = add(z, 6);
                    if (!st) break;
                    const float lean = float(side) * 0.18f;
                    st[0] = sticker(kShCapsule, sx + lean * 1.6f * k, sy - 1.6f * k, 1.7f * k, 0.13f * k, 0x9A6E44, 0x7A5232, 0x7A5232);
                    st[0].rot = -kPi * 0.5f + lean;
                    for (int i = 0; i < 5; ++i) {  // leaves fanning out from the crown, drooping a little
                        const float a = -kPi * 0.5f + (float(i) - 2.0f) * 0.7f + std::sin(time * 1.3f + float(cell)) * 0.04f;
                        st[1 + i] = sticker(kShEllipse, sx + lean * 3.2f * k + std::cos(a) * 0.8f * k, sy - 3.25f * k + std::sin(a) * 0.5f * k + 0.25f * k,
                                            0.85f * k, 0.2f * k, light, dark, dark);
                        st[1 + i].rot = a;
                        st[1 + i].fold = 2;
                    }
                    shadow(sx, sy, 1.0f * k, 0.28f * k);
                    break;
                }
                case 6: {  // mushrooms
                    Sticker* st = add(z, 4);
                    if (!st) break;
                    st[0] = sticker(kShBox, sx, sy - 0.6f * k, 0.3f * k, 0.6f * k, 0xF4ECDC, 0xD8CCB6, 0xD8CCB6);
                    st[0].p0 = 0.4f;
                    st[0].fold = 1;
                    st[1] = sticker(kShEllipse, sx, sy - 1.35f * k, 1.0f * k, 0.6f * k, light, dark, dark);
                    st[1].fold = 1;
                    st[2] = sticker(kShEllipse, sx - 0.35f * k, sy - 1.55f * k, 0.15f * k, 0.1f * k, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF);
                    st[3] = sticker(kShEllipse, sx + 0.3f * k, sy - 1.3f * k, 0.12f * k, 0.08f * k, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF);
                    shadow(sx, sy, 0.9f * k, 0.25f * k);
                    break;
                }
                default: {  // round trees: maples, blossoms, bushes, ember trees, lantern plums
                    const bool bush = family == 7 && (h >> 9) % 2 == 0;
                    Sticker* st = add(z, family == 9 ? 4 : 3);
                    if (!st) break;
                    if (bush) {
                        st[0] = sticker(kShEllipse, sx - 0.45f * k, sy - 0.45f * k, 0.7f * k, 0.5f * k, light, dark, dark);
                        st[1] = sticker(kShEllipse, sx + 0.4f * k, sy - 0.4f * k, 0.6f * k, 0.45f * k, light, dark, dark);
                        st[2] = sticker(kShEllipse, sx, sy - 0.75f * k, 0.55f * k, 0.45f * k, light, dark, dark);
                        for (int i = 0; i < 3; ++i) st[i].fold = 1;
                    } else {
                        st[0] = sticker(kShBox, sx, sy - 0.75f * k, 0.2f * k, 0.75f * k, family == 8 ? 0x3A2A28 : 0x7A5232, 0x5E3E26, 0x5E3E26);
                        st[0].fold = 1;
                        st[1] = sticker(kShEllipse, sx, sy - 2.3f * k, 1.35f * k, 1.15f * k, light, dark, dark);
                        st[1].fold = 1;
                        const u32 hl = mixc(light, 0xFFFFFF, 0.3f);
                        st[2] = sticker(kShEllipse, sx - 0.45f * k, sy - 2.65f * k, 0.45f * k, 0.35f * k, hl, hl, hl);
                    }
                    if (family == 9) {
                        st[3] = sticker(kShEllipse, sx + 0.7f * k, sy - 1.7f * k, 0.2f * k, 0.26f * k, 0xFF9A3D, 0xE06A18, 0xFFD27A);
                        st[3].fold = 1;
                    }
                    shadow(sx, sy, (bush ? 1.1f : 1.2f) * k, 0.3f * k);
                    break;
                }
            }
        }
    }
}

void Field::drawGates(const sim::Battle& b, UiBatch& labels) {
    const float zMin = cam_.z + cam_.tBottom - 1.0f, zMax = cam_.z + cam_.tTop + 2.0f;
    if (ids_.gatePost[0] >= 0) {  // the art: an animated panel between two posts (ART_DIRECTION 6)
        constexpr float kBottom = 0.25f, kTop = 2.2f;
        for (int g = 0; g < def_->gateCount; ++g) {
            const sim::GateDef& gd = def_->gates[g];
            const sim::GateState& gs = b.gates[g];
            if (gd.z < zMin || gd.z > zMax) continue;
            float collapse = 1.0f, taken = 0.0f;
            if (gs.used) {  // taken: flashes white and folds down into the road
                const float since = time - gateUsedAt_[g];
                if (gateUsedAt_[g] < 0 || since > 0.18f) continue;
                const float k = since / 0.18f;
                collapse = 1.0f - k * k;
                taken = 0.6f * maxv(0.0f, 1.0f - since / 0.15f);
            }
            const float fade = gs.passed && !gs.used ? 0.25f + 0.75f * clampv(1.0f - (b.z - gd.z) / 1.5f, 0.0f, 1.0f) : 1.0f;
            const bool mul = gd.kind == sim::kGateMul;
            const int kind = mul ? 1 : (gs.value > 0 ? 0 : 2);
            static const u32 kC[3] = {0x2E8BFF, 0x9B4DFF, 0xF0323C}, kH[3] = {0xBFE4FF, 0xE6CCFF, 0xFFC2B8};
            const u32 C = !mul && gs.value == 0 ? 0x8C96A8 : kC[kind], Hc = !mul && gs.value == 0 ? 0xD8DEE8 : kH[kind];
            const float top = kBottom + (kTop - kBottom) * collapse;
            float xl, yb, sb, xr, yr, sr, xt, yt, st;
            if (!cam_.project(gd.x0, gd.z, kBottom, xl, yb, sb) || !cam_.project(gd.x1, gd.z, kBottom, xr, yr, sr) ||
                !cam_.project(gd.x0, gd.z, top, xt, yt, st))
                continue;
            const float hit = gs.hit < 0.1f ? (1.0f - gs.hit / 0.1f) * 0.55f : 0.0f;  // shot all the time: a gentle flash
            Sticker* p = add(gd.z, 1);
            if (!p) return;
            *p = Sticker{};
            p->x = (xl + xr) * 0.5f;
            p->y = (yb + yt) * 0.5f;
            p->hw = (xr - xl) * 0.5f;
            p->hh = maxv(0.5f, (yb - yt) * 0.5f);
            p->shape = float(kShGate);
            p->soft = 1.0f;
            p->p0 = float(kind);
            p->p1 = gd.x1 - gd.x0;
            p->p2 = top - kBottom;
            p->fold = hit;
            setColor(p->c0, C, fade);
            setColor(p->c1, Hc, 1.0f);
            p->c2[0] = u8(taken * 255.0f);
            p->c2[3] = gs.passed ? 0 : 255;
            // Its posts (over the panel's sides) and, for a barricade, sharpened stakes along its foot.
            const int post = ids_.gatePost[kind];
            for (int side = 0; side < 2; ++side) {
                const float x = side ? gd.x1 : gd.x0;
                if (sprite(post, x, gd.z, 0, time, 1.0f, fade).st) castShadow(x, gd.z, 2.7f, 0.18f);
            }
            if (kind == 2 && ids_.gateSpikes >= 0)
                for (float x = gd.x0 + 0.5f; x < gd.x1 - 0.2f; x += 1.0f) sprite(ids_.gateSpikes, x, gd.z - 0.02f, 0, time, 1.0f, fade);
            // Its number: white, outlined in a dark shade of its colour, pulsing when it changes.
            char t[16];
            if (mul) std::snprintf(t, sizeof t, "\xC3\x97%d", gs.value);
            else if (gs.value > 0) std::snprintf(t, sizeof t, "+%d", gs.value);
            else std::snprintf(t, sizeof t, "%d", gs.value);
            const float pulse = gs.hit < 0.12f ? 1.0f + 0.15f * (1.0f - gs.hit / 0.12f) : 1.0f;
            // A row of choices gets big numbers; a column's panels (one a row, close behind each other) smaller ones.
            bool row = false;
            for (int ri = 0; ri < def_->rowCount && !row; ++ri) row = std::fabs(def_->rowZ[ri] - gd.z) < 0.05f;
            float size = (row ? 1.7f : 0.95f) * sb * tune.tagScale * pulse * collapse;
            const float w = UiBatch::textWidth(t, size);
            if (w > (xr - xl) * 0.8f) size *= (xr - xl) * 0.8f / w;
            if (size >= 7.0f && fade > 0.3f) label(labels, t, (xl + xr) * 0.5f, (yb + yt) * 0.5f, size, kWhiteC, mixc(0x000000, C, 0.35f));
        }
        return;
    }
    for (int g = 0; g < def_->gateCount; ++g) {
        const sim::GateDef& gd = def_->gates[g];
        const sim::GateState& gs = b.gates[g];
        if (gs.used || gd.z < zMin || gd.z > zMax) continue;
        float xl, yl, sl, xr, yr, sr;
        if (!cam_.project(gd.x0 + 0.06f, gd.z, 0, xl, yl, sl) || !cam_.project(gd.x1 - 0.06f, gd.z, 0, xr, yr, sr)) continue;
        const float s = sl, cx = (xl + xr) * 0.5f, hw = (xr - xl) * 0.5f, hh = 0.82f * s;
        const bool mul = gd.kind == sim::kGateMul;
        const bool good = mul || gs.value > 0;
        const u32 fill = mul ? kVioletC : (gs.value > 0 ? kBlue : (gs.value == 0 ? 0x9AA4B4 : kRed));
        const u32 post = mul ? kVioletDark : (gs.value > 0 ? kBlueDark : (gs.value == 0 ? 0x6A7484 : kRedDark));
        const u32 top = mul ? kVioletLight : (gs.value > 0 ? kBlueLight : (gs.value == 0 ? 0xC8D0DC : 0xFFA0A0));
        const float flash = gs.hit < 0.09f ? 1.0f - gs.hit / 0.09f : 0.0f;
        const float fade = gs.passed ? 0.45f : 1.0f;
        Sticker* st = add(gd.z, 4);
        if (!st) return;
        st[0] = sticker(kShBox, cx, yl - hh, hw, hh, mixc(fill, kWhiteC, 0.15f + 0.4f * flash), fill, fill, (0.55f + 0.25f * flash) * fade);
        st[0].p0 = 0.1f;
        st[0].fold = 2;
        setColor(st[0].c1, mixc(fill, 0x000000, 0.08f), (0.62f + 0.25f * flash) * fade);
        st[1] = sticker(kShBox, cx, yl - 2.0f * hh, hw + 0.05f * s, maxv(1.0f, 0.075f * s), mixc(top, kWhiteC, flash), top, top, fade);
        st[1].p0 = 0.5f;
        st[2] = sticker(kShBox, xl, yl - hh, maxv(1.0f, 0.07f * s), hh + 0.05f * s, post, post, post, fade);
        st[3] = sticker(kShBox, xr, yl - hh, maxv(1.0f, 0.07f * s), hh + 0.05f * s, post, post, post, fade);
        shadow(cx, yl, hw, 0.12f * s);
        // Its number.
        char t[16];
        if (mul) std::snprintf(t, sizeof t, "\xC3\x97%d", gs.value);
        else if (gs.value > 0) std::snprintf(t, sizeof t, "+%d", gs.value);
        else std::snprintf(t, sizeof t, "%d", gs.value);
        float size = 0.95f * s * tune.tagScale * (1.0f + 0.25f * flash);
        const float w = UiBatch::textWidth(t, size);
        if (w > hw * 1.7f) size *= hw * 1.7f / w;
        if (size >= 7.0f) label(labels, t, cx, yl - hh, size, kWhiteC, good ? (mul ? kVioletDark : kBlueDeep) : kRedDeep);
    }
}

void Field::drawBarrels(const sim::Battle& b, UiBatch& labels) {
    const float zMin = cam_.z + cam_.tBottom - 1.0f, zMax = cam_.z + cam_.tTop + 2.0f;
    for (int k = 0; k < def_->barrelCount; ++k) {
        const sim::BarrelDef& bd = def_->barrels[k];
        const sim::BarrelState& bs = b.barrels[k];
        if (bs.broken || bd.z < zMin || bd.z > zMax) continue;
        const float flash = bs.hit < 0.07f ? 1.0f - bs.hit / 0.07f : 0.0f;
        const float jig = flash * 0.06f * std::sin(time * 70.0f);
        float sx, sy, s;
        if (!cam_.project(bd.x + jig, bd.z, 0, sx, sy, s)) continue;
        const float hw = 0.5f * s;
        float top = sy - 1.28f * s;  // the barrel's top on screen (its tag sits above)
        if (ids_.barrel >= 0) {
            const Placed pl = sprite(ids_.barrel, bd.x + jig, bd.z, 0, time, 1.0f, 1.0f, 0, false, flash * 0.3f * (0.5f + 0.5f * std::sin(time * 47.0f)));
            if (pl.fr) top = pl.sy - pl.fr->py * pl.k;
            if (pl.st) {
                pl.st->hh *= 1.0f - 0.06f * flash;  // squashed by the hit, feet on the road
                pl.st->y += pl.st->hh * 0.06f * flash;
                castShadow(bd.x, bd.z, 1.3f, 0.5f);
            }
        } else {
            const float hh = 0.64f * s;
            Sticker* st = add(bd.z, 5);
            if (!st) return;
            const u32 body = mixc(kGoldC, kWhiteC, flash * 0.6f);
            st[0] = sticker(kShBox, sx, sy - hh, hw, hh, body, mixc(kGoldDarkC, kWhiteC, flash * 0.5f), kGoldDarkC);
            st[0].p0 = 0.32f;
            st[0].fold = 1;
            st[1] = sticker(kShBox, sx, sy - hh * 1.45f, hw * 1.01f, maxv(1.0f, 0.055f * s), kGoldDeep, kGoldDeep, kGoldDeep);
            st[2] = sticker(kShBox, sx, sy - hh * 0.55f, hw * 1.01f, maxv(1.0f, 0.055f * s), kGoldDeep, kGoldDeep, kGoldDeep);
            st[3] = sticker(kShEllipse, sx, sy - 2.0f * hh + 0.08f * s, hw * 0.9f, 0.13f * s, kGoldLight, kGoldLight, kGoldLight);
            st[4] = sticker(kShStar, sx, sy - hh, 0.22f * s, 0.22f * s, 0xFFF4C8, 0xFFF4C8, 0xFFF4C8, 0.9f);
            shadow(sx, sy, hw * 1.05f, 0.16f * s);
        }
        char t[16];
        std::snprintf(t, sizeof t, "+%d", bd.reward);
        const float size = 0.72f * s * tune.tagScale;
        const float ly = top - 0.62f * s;
        if (size >= 7.0f) {
            label(labels, t, sx, ly, size, kWhiteC, kGoldDeep);
            const float bw = 0.95f * s, bh = maxv(3.0f, 0.13f * s);
            const float frac = clampv(float(bs.hp) / float(maxv<i32>(bd.hp, 1)), 0.0f, 1.0f);
            labels.rect(sx, ly + size * 0.62f, bw, bh, bh * 0.5f, rgba(0x3A2410, 200));
            if (frac > 0) labels.rect(sx - bw * 0.5f * (1.0f - frac), ly + size * 0.62f, bw * frac, bh * 0.7f, bh * 0.35f, rgba(0xFFE24A));
        }
    }
}

void Field::drawEnemy(const sim::Enemy& e, float z) {
    float sx, sy, s;
    if (!cam_.project(e.x, z, 0, sx, sy, s)) return;
    const float W = float(L_.w), H = float(L_.h);
    if (sx < -60.0f || sx > W + 60.0f || sy < -40.0f || sy > H + 160.0f) return;
    const float flash = e.hit < 0.08f ? 1.0f - e.hit / 0.08f : 0.0f;
    const int anim = e.kind == sim::kRider ? ids_.rider : (e.kind == sim::kBrute ? ids_.brute : ids_.foot);
    if (anim >= 0) {
        // Each a little different: its size, a shade darker or lighter, its step.
        const float var = 0.94f + 0.12f * h01(u32(e.seed) * 2654435761u + 3u);
        const float shade = 0.90f + 0.16f * h01(u32(e.seed) * 40503u + 11u);
        const Placed pl = sprite(anim, e.x, z, 0, time + float(e.seed & 255) * 0.043f, var, 1.0f, 0, false,
                                 maxv(flash * 0.85f, shade - 1.0f));
        if (pl.st) {
            if (shade < 1.0f) setColor(pl.st->c0, mixc(0x000000, 0xFFFFFF, shade), 1.0f);
            const float h = e.kind == sim::kBrute ? 1.9f : (e.kind == sim::kRider ? 1.7f : 1.05f);
            const float r = e.kind == sim::kBrute ? 0.65f : (e.kind == sim::kRider ? 0.5f : 0.27f);
            castShadow(e.x, z, h * var, r * var);
            ++figures_;
        }
        return;
    }
    const float walk = time * 10.0f + float(e.seed & 255) * 0.1f;
    const float bob = std::fabs(std::sin(walk)) * 0.07f * s;
    const float sway = std::sin(walk) * 0.07f;
    const u32 body = mixc(kRed, kWhiteC, flash * 0.75f), side = mixc(kRedDark, kWhiteC, flash * 0.6f);
    ++figures_;
    if (e.kind == sim::kRider) {
        const float hw = 0.62f * s, hh = 0.42f * s;
        const bool right = true;  // the horse shape faces right
        Sticker* st = add(z, 3);
        if (!st) return;
        st[0] = sticker(kShHorse, sx, sy - hh - bob * 0.5f, hw, hh, 0x7A4A2E, 0x4E2E1C, 0x2E1A10);
        st[1] = sticker(kShPawn, sx - (right ? 0.08f : -0.08f) * s, sy - 2.0f * hh - 0.3f * s - bob, 0.24f * s, 0.38f * s, body, side, kRedDeep);
        st[1].rot = sway;
        st[2] = sticker(kShCapsule, sx + (right ? 0.45f : -0.45f) * s, sy - 2.0f * hh - 0.4f * s - bob, 0.5f * s, maxv(0.8f, 0.035f * s), 0xC8C0B0,
                        0xC8C0B0, 0xC8C0B0);
        st[2].rot = right ? -0.5f : 0.5f;
        shadow(sx, sy, 0.7f * s, 0.18f * s);
        return;
    }
    const bool brute = e.kind == sim::kBrute;
    const float k = brute ? 1.75f : 1.0f;
    const float hw = 0.3f * s * k, hh = 0.48f * s * k;
    const float cy = sy - hh - bob;
    const bool detail = s * k > detailScale_;
    Sticker* st = add(z, detail ? (brute ? 5 : 4) : 2);
    if (!st) return;
    st[0] = sticker(kShPawn, sx, cy, hw, hh, brute ? mixc(kRedDark, kWhiteC, flash * 0.7f) : body, brute ? mixc(kRedDeep, kWhiteC, flash * 0.6f) : side,
                    brute ? 0x2A0A10 : kRedDeep);
    st[0].rot = sway;
    // A spiked helmet.
    st[1] = sticker(kShTri, sx + sway * hh, cy - 1.12f * hh, 0.2f * hw * 2.0f, 0.2f * hh, 0x4A0A16, 0x4A0A16, 0x4A0A16);
    if (detail) {
        st[2] = sticker(kShEllipse, sx + sway * hh * 0.6f, cy - 0.52f * hh, 0.26f * hw * 2.0f, 0.13f * hh * 2.0f, kSkin, mixc(kSkin, 0x000000, 0.12f), kSkin);
        st[2].fold = 1;
        st[3] = sticker(kShCapsule, sx + hw * 0.95f, cy - 0.35f * hh, 0.75f * hh, maxv(0.8f, 0.05f * hh), 0x6A4A34, 0x6A4A34, 0x6A4A34);
        st[3].rot = -1.35f + sway;
        if (brute) {  // a round shield
            st[4] = sticker(kShEllipse, sx - hw * 0.55f, cy + 0.25f * hh, 0.5f * hw, 0.42f * hh, 0x8A8E98, 0x5E626C, 0x5E626C);
            st[4].fold = 1;
        }
    }
    shadow(sx, sy, hw * 1.15f, 0.14f * s * k);
}

void Field::drawBoss(const sim::Enemy& e, float z, UiBatch& labels) {
    (void)labels;
    float sx, sy, s;
    if (!cam_.project(e.x, z, 0, sx, sy, s)) return;
    const float flash = e.hit < 0.07f ? 1.0f - e.hit / 0.07f : 0.0f;
    const float breathe = 1.0f + 0.025f * std::sin(time * 2.4f);
    const int boss = ids_.bossOf[clampv<int>(def_->world, 1, 10) - 1];
    if (boss >= 0) {
        // Hit all the time while the squad fires: a quick flicker, never a white-out.
        const float flicker = flash * 0.28f * (0.5f + 0.5f * std::sin(time * 47.0f));
        const WorldArt& wa = art_->world(def_->world);
        const Placed pl = sprite(boss, e.x, z, 0, time, breathe, 1.0f, 0, false, flicker);
        if (pl.st) {
            // The world's recolour of the shared boss, and its add-on drawn over it in step.
            float hue = std::fmod(wa.bossHue, 360.0f);
            if (hue < 0) hue += 360.0f;
            pl.st->c2[0] = u8(hue / 360.0f * 255.0f + 0.5f);
            setColor(pl.st->c0, wa.bossTint, 1.0f);
            if (wa.bossAddon >= 0) sprite(wa.bossAddon, e.x, z, 0, time, breathe, 1.0f, 0, false, flicker);
            castShadow(e.x, z, 4.6f, 1.7f);
            ++figures_;
        }
        // Its footfalls (the stomp's first and middle frames): a ring on the road, dust, a tremor.
        const SpriteAnim& A = art_->anim(boss);
        const int fr = int(std::floor(time * A.fps)) % maxv(1, A.frames);
        if (fr != bossFrame_) {
            bossFrame_ = fr;
            if ((fr == 0 || fr == A.frames / 2) && pl.st) {
                flipbook(ids_.ring, e.x, z, 0.02f, 0.5f);
                for (int i = 0; i < 3; ++i) flipbook(ids_.dust, e.x + (rnd() - 0.5f) * 2.4f, z - 0.3f, 0.0f, 3.0f);
                shake_ = maxv(shake_, 0.12f);
            }
        }
        return;
    }
    const float k = s * breathe;
    const float swing = std::sin(time * 3.2f) * 0.35f;
    auto skin = [&](float t) { return mixc(kOgre, kWhiteC, t); };
    Sticker* st = add(z, 16);
    if (!st) return;
    const u32 armor = mixc(kRedDark, kWhiteC, flash * 0.6f), armorD = mixc(kRedDeep, kWhiteC, flash * 0.5f);
    int i = 0;
    st[i++] = sticker(kShBox, sx - 0.65f * k, sy - 0.55f * k, 0.32f * k, 0.55f * k, 0x5A3A2A, 0x42281C, 0x42281C);  // legs
    st[i++] = sticker(kShBox, sx + 0.65f * k, sy - 0.55f * k, 0.32f * k, 0.55f * k, 0x5A3A2A, 0x42281C, 0x42281C);
    st[i++] = sticker(kShCapsule, sx - 1.55f * k, sy - 2.2f * k, 0.9f * k, 0.3f * k, skin(flash * 0.7f), kOgreDark, kOgreDark);  // left arm
    st[i - 1].rot = 1.25f;
    st[i++] = sticker(kShEllipse, sx, sy - 2.05f * k, 1.55f * k, 1.5f * k, armor, armorD, armorD);  // the body
    st[i - 1].fold = 1;
    st[i++] = sticker(kShEllipse, sx - 0.1f * k, sy - 1.85f * k, 0.95f * k, 0.85f * k, skin(flash * 0.7f), mixc(kOgreDark, kWhiteC, flash * 0.5f),
                      kOgreDark);  // the belly
    st[i - 1].fold = 1;
    st[i++] = sticker(kShBox, sx, sy - 1.15f * k, 1.45f * k, 0.14f * k, 0x3E2618, 0x3E2618, 0x3E2618);  // the belt
    st[i++] = sticker(kShEllipse, sx, sy - 3.65f * k, 0.88f * k, 0.78f * k, skin(flash * 0.7f), mixc(kOgreDark, kWhiteC, flash * 0.5f),
                      kOgreDark);  // the head
    st[i - 1].fold = 1;
    st[i++] = sticker(kShTri, sx - 0.62f * k, sy - 4.45f * k, 0.15f * k, 0.36f * k, kIvoryC, 0xD8CCB0, kIvoryC);  // horns
    st[i - 1].rot = -0.45f;
    st[i++] = sticker(kShTri, sx + 0.62f * k, sy - 4.45f * k, 0.15f * k, 0.36f * k, kIvoryC, 0xD8CCB0, kIvoryC);
    st[i - 1].rot = 0.45f;
    st[i++] = sticker(kShEllipse, sx - 0.32f * k, sy - 3.78f * k, 0.15f * k, 0.11f * k, 0xFFE24A, 0xFFE24A, 0xFFE24A);  // eyes
    st[i++] = sticker(kShEllipse, sx + 0.32f * k, sy - 3.78f * k, 0.15f * k, 0.11f * k, 0xFFE24A, 0xFFE24A, 0xFFE24A);
    st[i++] = sticker(kShEllipse, sx, sy - 3.3f * k, 0.42f * k, 0.13f * k, 0x3A1010, 0x3A1010, 0x3A1010);  // the mouth
    st[i++] = sticker(kShTri, sx - 0.24f * k, sy - 3.38f * k, 0.07f * k, 0.13f * k, kIvoryC, kIvoryC, kIvoryC);  // tusks
    st[i++] = sticker(kShTri, sx + 0.24f * k, sy - 3.38f * k, 0.07f * k, 0.13f * k, kIvoryC, kIvoryC, kIvoryC);
    st[i++] = sticker(kShCapsule, sx + 1.75f * k, sy - 2.9f * k, 1.45f * k, 0.26f * k, kWoodC, 0x5A3820, 0x5A3820);  // the club
    st[i - 1].rot = -1.05f + swing;
    st[i++] = sticker(kShEllipse, sx + 1.5f * k, sy - 2.1f * k, 0.36f * k, 0.32f * k, skin(flash * 0.7f), kOgreDark, kOgreDark);  // its fist
    shadow(sx, sy, 1.9f * s, 0.45f * s);
    ++figures_;
}

void Field::drawSoldiers(const sim::Battle& b, float zs, float xs, UiBatch& labels) {
    const int m = minv(kMaxShown, minv(int(tune.soldiersShown), int(shownSquad_ + 0.5f)));
    const float R = minv(4.4f, 0.55f + 0.3f * std::sqrt(maxv(1.0f, shownSquad_)));
    const bool marching = b.started && !b.won && !b.lost;
    for (int i = 0; i < m; ++i) {
        const float u = std::sqrt((float(i) + 0.5f) / float(m));
        const float a = float(i) * 2.39996323f;
        const float jx = (h01(u32(i) * 7u + 1u) - 0.5f) * 0.12f, jz = (h01(u32(i) * 13u + 5u) - 0.5f) * 0.12f;
        const float x = xs + std::cos(a) * u * R * 0.95f + jx;
        const float z = zs + std::sin(a) * u * R * 0.85f + jz;
        if (ids_.soldier >= 0) {
            // A new soldier pops in (0 to 1.15 to 1 over 0.2 s); each is a little taller or shorter than the next.
            if (i >= shownBefore_) bornAt_[i] = time;
            const float age = time - bornAt_[i];
            const float pop = age >= 0.2f ? 1.0f : (age < 0.13f ? 1.15f * age / 0.13f : 1.15f - 0.15f * (age - 0.13f) / 0.07f);
            const float var = 0.94f + 0.12f * h01(u32(i) * 31u + 7u);
            const bool cheering = b.won && ids_.cheer >= 0;  // hats in the air
            const Placed pl = sprite(cheering ? ids_.cheer : ids_.soldier, x, z, 0,
                                     marching || cheering ? time + float(i) * 0.137f : 0.0f, var * pop, 1.0f);
            if (!pl.st) continue;
            castShadow(x, z, 1.2f * var, 0.25f);
            ++figures_;
            float mx, my;
            if (flash_[i] > 0 && ids_.muzzle >= 0 && art_->point(ids_.soldier, pl.elev, "muzzle", mx, my)) {
                const SpriteFrame& f0 = art_->frameAt(ids_.soldier, pl.elev, 0);
                spriteAt(ids_.muzzle, z - 0.01f, pl.sx + (mx - f0.px) * pl.k, pl.sy + (my - f0.py) * pl.k, pl.s, 0.07f - flash_[i], 1.0f, 1.0f);
            }
            continue;
        }
        float sx, sy, s;
        if (!cam_.project(x, z, 0, sx, sy, s)) continue;
        const float walk = time * 10.0f + float(i) * 1.7f;
        const float bob = marching ? std::fabs(std::sin(walk)) * 0.07f * s : 0.0f;
        const float sway = marching ? std::sin(walk) * 0.06f : 0.0f;
        const float hw = 0.27f * s, hh = 0.44f * s;
        const float cy = sy - hh - bob;
        const bool flash = flash_[i] > 0;
        Sticker* st = add(z, flash ? 4 : 3);
        if (!st) break;
        st[0] = sticker(kShPawn, sx, cy, hw, hh, kBlue, kBlueDark, 0xF2C230);
        st[0].rot = sway;
        st[1] = sticker(kShEllipse, sx + sway * hh, cy - 0.7f * hh, 0.42f * hh, 0.26f * hh, kBlueDeep, 0x0E2660, kBlueDeep);
        st[1].fold = 1;
        st[2] = sticker(kShCapsule, sx + hw * 0.8f, cy - 0.55f * hh, 0.62f * hh, maxv(0.8f, 0.07f * hh), kGun, kGun, kGun);
        st[2].rot = -1.5f + sway;
        if (flash) st[3] = sticker(kShStar, sx + hw * 0.82f, cy - 1.22f * hh, 0.22f * hh, 0.22f * hh, kSpark, kSpark, kSpark, 0.95f);
        shadow(sx, sy, hw * 1.1f, 0.12f * s);
        ++figures_;
    }
    shownBefore_ = m;
    // The squad's banner, carried in its middle with the count on its cloth (ART_DIRECTION 15.3).
    if (ids_.banner >= 0) {
        const Placed pl = sprite(ids_.banner, xs, zs + 0.2f, 0, time, 1.0f, 1.0f);
        float tx, ty;
        if (pl.st && art_->point(ids_.banner, pl.elev, "text", tx, ty)) {
            const SpriteFrame& f0 = art_->frameAt(ids_.banner, pl.elev, 0);
            char t[24];
            formatHost(i64(shownSquad_ + 0.5f), t, sizeof t);
            float size = 0.62f / UiBatch::capHeight() * pl.s * (1.0f + 0.25f * badgePop_);
            const float w = UiBatch::textWidth(t, size);
            if (w > 1.7f * pl.s) size *= 1.7f * pl.s / w;
            labels.text(t, pl.sx + (tx - f0.px) * pl.k, pl.sy + (ty - f0.py) * pl.k, size, rgba(kWhiteC), rgba(0x16224F), maxv(1.5f, 0.05f * pl.s),
                        Align::Center, true, 0);
        }
        return;
    }
    // The badge: how many, over the squad.
    float bx, by, bs;
    if (cam_.project(xs, zs + R * 0.4f, 1.6f + R * 0.25f, bx, by, bs)) {
        char t[24];
        formatHost(i64(shownSquad_ + 0.5f), t, sizeof t);
        const float size = L_.px(26) * (1.0f + 0.25f * badgePop_);
        const float w = UiBatch::textWidth(t, size) + L_.px(22);
        by = minv(by, squadY_ - L_.px(70));
        labels.rect(bx + L_.px(2), by + L_.px(3), w, size * 1.35f, size * 0.6f, rgba(0x0A1A40, 90));
        labels.rect(bx, by, w, size * 1.35f, size * 0.6f, rgba(kBlueDark), rgba(kWhiteC), L_.px(2.5f));
        label(labels, t, bx, by, size, kWhiteC, kBlueDeep);
    }
}

void Field::drawParticles() {
    const float W = float(L_.w);
    for (int i = 0; i < partCount_; ++i) {
        const Particle& p = parts_[i];
        if (p.life <= 0) continue;
        if (p.anim >= 0 && art_) {  // the art: flipbooks, debris, drifting bits, figures shrinking away, confetti
            const float age = p.max - p.life;
            if (p.kind == 6) {
                if (age < 0) continue;  // not yet (a delayed blast)
                const Placed pl = sprite(p.anim, p.x, p.z, p.y, age, p.size, 1.0f, p.rot);
                if (pl.st && p.anim == ids_.ring) {  // lies flat on the road
                    pl.st->hh *= maxv(0.12f, std::sin(pl.elev * kPi / 180.0f));
                }
            } else if (p.kind == 7) {
                sprite(p.anim, p.x, p.z, p.y, age, p.size, minv(1.0f, minv(age * 2.0f, p.life / 0.6f)), p.rot);
            } else if (p.kind == 8) {
                const float t = p.life / p.max;  // 1 .. 0: shrinks to 0.6 and fades
                const Placed pl = sprite(p.anim, p.x, p.z, 0, time + p.rot, p.size * (0.6f + 0.4f * t), t);
                const WorldArt& wa = art_->world(def_ ? def_->world : 1);
                if (pl.st && def_ && p.anim == ids_.bossOf[clampv<int>(def_->world, 1, 10) - 1]) {  // the world's recolour
                    float hue = std::fmod(wa.bossHue, 360.0f);
                    if (hue < 0) hue += 360.0f;
                    pl.st->c2[0] = u8(hue / 360.0f * 255.0f + 0.5f);
                    setColor(pl.st->c0, wa.bossTint, t);
                }
            } else if (p.kind == 5) {
                sprite(p.anim, p.x, p.z, p.y, age, 1.0f, minv(1.0f, p.life / 0.25f), p.rot);
            } else if (p.kind == 0) {
                sprite(p.anim, p.x, p.z, p.y, age, p.size, minv(1.0f, p.life / 0.2f), p.rot);
            }
            continue;
        }
        float sx, sy, s;
        if (!cam_.project(p.x, p.z, p.y, sx, sy, s)) continue;
        if (sx < -100 || sx > W + 100) continue;
        const float t = p.life / p.max;
        switch (p.kind) {
            case 1: {  // a puff of smoke, growing and fading
                const float r = p.size * s * (1.0f + (1.0f - t) * 1.4f);
                Sticker* st = add(p.z, 1);
                if (!st) return;
                st[0] = sticker(kShEllipse, sx, sy, r, r * 0.8f, p.c0, p.c1, p.c1, 0.55f * t);
                break;
            }
            case 2: {  // a figure knocked over
                const float k = p.size;
                const float hw = 0.3f * s * k, hh = 0.48f * s * k;
                Sticker* st = add(p.z, 1);
                if (!st) return;
                const bool ours = p.shape == 1;
                st[0] = sticker(kShPawn, sx, sy - hh * std::cos(p.rot), hw, hh, p.c0, p.c1, ours ? 0xF2C230 : kRedDeep, minv(1.0f, p.life / 0.3f));
                st[0].rot = p.rot;
                break;
            }
            case 3: {  // a spark
                const float r = maxv(1.5f, p.size * s * (0.5f + 0.5f * t));
                Sticker* st = add(p.z, 1);
                if (!st) return;
                st[0] = sticker(kShStar, sx, sy, r, r, p.c0, p.c0, p.c0, minv(1.0f, t * 2.0f));
                st[0].rot = p.rot;
                break;
            }
            case 4: {  // a ring on the ground
                const float r = p.size * s * (0.4f + (1.0f - t) * 2.2f);
                Sticker* st = add(p.z, 1);
                if (!st) return;
                st[0] = sticker(kShRing, sx, sy, r, r * 0.55f, p.c0, p.c0, p.c0, 0.8f * t);
                st[0].p0 = maxv(2.0f, 0.18f * s * p.size);
                break;
            }
            default: {  // a bit of paper or confetti
                const float r = maxv(1.2f, p.size * s);
                Sticker* st = add(p.z, 1);
                if (!st) return;
                st[0] = sticker(Shape(p.shape), sx, sy, r, r * (0.6f + 0.4f * std::fabs(std::sin(p.rot * 1.3f))), p.c0, p.c1, p.c1,
                                minv(1.0f, p.life / 0.25f));
                st[0].rot = p.rot;
                st[0].fold = 1;
                break;
            }
        }
    }
}

void Field::build(PaperFrame& f, UiBatch& labels, const sim::Battle& b, float alpha) {
    labels.clear();
    poolN_ = itemN_ = casterN_ = out_ = 0;
    figures_ = 0;
    f.stickers = nullptr;
    f.stickerCount = 0;
    if (!def_) return;
    // The squad between the last step and the next; the camera follows it up the road; shakes after a blast.
    viewZ_ = b.z + dz_ * alpha;
    viewX_ = b.x + dx_ * alpha;
    layoutCamera();
    cam_.z = viewZ_ - tune.camBack;
    if (art_) {  // pulls back a little after the boss's fall, leans in a little when the squad is small
        const float small = clampv((10.0f - shownSquad_) / 9.0f, 0.0f, 1.0f);
        const float k = (1.0f - 0.03f * zoom_ * zoom_ * (3.0f - 2.0f * zoom_)) * (1.0f + 0.02f * small);
        const float up = (tune.camSquadY * float(L_.h) - cam_.cy) / cam_.f;  // keeps the squad's row where it is
        cam_.f *= k;
        cam_.cy = tune.camSquadY * float(L_.h) - cam_.f * up;
    }
    if (shake_ > 0 && !reduceEffects) {
        cam_.cx += (rnd() - 0.5f) * shake_ * L_.px(14);
        cam_.cy += (rnd() - 0.5f) * shake_ * L_.px(14);
    }
    {
        float s;
        cam_.project(viewX_, viewZ_, 0, squadX_, squadY_, s);
    }
    GroundParams& g = f.ground;
    g.on = true;
    g.cam0[0] = cam_.cx;
    g.cam0[1] = cam_.cy;
    g.cam0[2] = cam_.f;
    g.cam0[3] = cam_.h;
    g.cam1[0] = cam_.sn;
    g.cam1[1] = cam_.cs;
    g.cam1[2] = cam_.z;
    g.cam1[3] = def_->bossHp == 0 ? def_->length : -1.0f;
    const RoadLook& look = kLooks[(maxv<int>(def_->world, 1) - 1) % kWorldCount];
    g.stoneA = look.stoneA;
    g.stoneB = look.stoneB;
    g.mortar = look.mortar;
    g.curb = look.curb;
    g.grassA = look.grassA;
    g.grassB = look.grassB;
    g.haze = look.haze;
    g.speckle = look.speckle;
    g.roadHalf = sim::kRoadHalf;
    g.hazeStart = depthS_ + 26.0f;
    drawGround(f);

    // What matters most is added first, so a full pool drops the least: the squad, gates, barrels, scenery, the horde
    // (fewer details when it is huge), bullets, bits.
    const float zLo = cam_.z + cam_.tBottom - 3.0f, zHi = cam_.z + cam_.tTop + 6.0f;
    if (!b.lost) drawSoldiers(b, viewZ_, viewX_, labels);
    drawGates(b, labels);
    drawBarrels(b, labels);
    if (art_ && (art_->world(def_->world).propCount > 0 || art_->world(def_->world).edgeCount > 0)) sceneryArt(zLo, zHi);
    else scenery(zLo, zHi);
    detailScale_ = b.aliveEnemies() > 700 ? 60.0f : (b.aliveEnemies() > 400 ? 42.0f : 30.0f);
    const float step = alpha * sim::kDt;
    for (int i = 0; i < b.enemyCount; ++i) {
        const sim::Enemy& e = b.enemies[i];
        if (!e.alive) continue;
        if (e.kind == sim::kBoss) {
            const bool halted = e.z - b.z - sim::enemyStats(sim::kBoss).radius <= b.radius() * 0.6f;
            drawBoss(e, halted ? e.z : e.z - sim::enemyStats(e.kind).speed * step, labels);
            continue;
        }
        const float z = e.z - sim::enemyStats(e.kind).speed * def_->pace * step;
        if (z < zLo || z > zHi) continue;
        drawEnemy(e, z);
    }
    // Bullets: streaks of light, no shadow.
    for (int i = 0; i < b.bulletCount; ++i) {
        const sim::Bullet& bl = b.bullets[i];
        if (!bl.alive) continue;
        const float z = bl.z + 42.0f * step;
        if (ids_.tracer >= 0) {  // a streak along its flight: up the road, toward the vanishing point
            float ax, ay, bx, by, s;
            if (!cam_.project(bl.x, z, 0.62f, ax, ay, s) || !cam_.project(bl.x, z + 1.0f, 0.62f, bx, by, s)) continue;
            sprite(ids_.tracer, bl.x, z, 0.62f, time, 1.0f, 1.0f, std::atan2(bx - ax, ay - by));
            continue;
        }
        float sx, sy, s;
        if (!cam_.project(bl.x, z, 0.62f, sx, sy, s)) continue;
        Sticker* st = add(z, 2);
        if (!st) break;
        st[0] = sticker(kShEllipse, sx, sy, maxv(2.0f, 0.13f * s), maxv(5.0f, 0.5f * s), 0xFFC040, 0xFFC040, 0xFFC040, 0.35f);
        st[1] = sticker(kShEllipse, sx, sy, maxv(1.2f, 0.055f * s), maxv(3.5f, 0.34f * s), 0xFFFBE0, 0xFFFBE0, 0xFFFBE0);
    }
    drawParticles();
    // Far to near, then the shadows' and the stickers' lists for the renderer.
    // Ties go by the order things were added, the same every frame (std::sort is not stable: no flicker).
    std::sort(items_, items_ + itemN_, [](const Item& a, const Item& c) { return a.key > c.key || (a.key == c.key && a.first < c.first); });
    for (int i = 0; i < itemN_; ++i) {
        const Item& it = items_[i];
        std::memcpy(sorted_ + out_, pool_ + it.first, sizeof(Sticker) * it.count);
        out_ += it.count;
    }
    f.stickers = sorted_;
    f.stickerCount = out_;
    f.shadows = casters_;
    f.shadowCount = casterN_;
    const bool artWorld = art_ && art_->world(def_->world).defined;
    f.shadowStrength = artWorld ? shAlpha_ : tune.shadowStrength;
    f.shadowColor = artWorld ? shColor_ : 0x1E1810;
    f.over = nullptr;
    f.overCount = 0;
    // Floating numbers, over everything on the road.
    for (const Floater& fl : floaters_) {
        if (fl.life <= 0) continue;
        float sx, sy, s;
        if (!cam_.project(fl.x + (fl.follow ? viewX_ : 0.0f), fl.z + (fl.follow ? viewZ_ : 0.0f), fl.y, sx, sy, s)) continue;
        const float a = minv(1.0f, fl.life / 0.35f);
        const float pop = fl.life > 0.95f ? 1.0f + (fl.life - 0.95f) * 3.0f : 1.0f;
        const float size = L_.px(38) * fl.size * pop;
        labels.text(fl.text, sx, sy, size, rgba(fl.fill, u8(255 * a)), rgba(fl.stroke, u8(255 * a)), size * 0.14f, Align::Center, true, 0);
    }
    f.labels = &labels;
    f.labelOpacity = 1.0f;
    f.time = time;
}

}  // namespace pm
