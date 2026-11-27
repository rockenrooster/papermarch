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
constexpr u32 kOrangeC = 0xFF8A1E;  // weapons' colour (ART_DIRECTION 16)

// The weapons' names: on a gate, and when taken.
const char* const kWeaponShort[] = {"MUSKET", "TWIN", "SCATTER", "LANCE", "ROCKETS", "FLAME", "CANNON", "COIL", "MORTAR", "FROST"};
const char* const kWeaponName[] = {"MUSKET",       "TWIN RIFLES", "SCATTERGUN", "LONG LANCE", "ROCKETS",
                                   "FLAMETHROWER", "CANNON",      "STORM COIL", "MORTAR",     "FROST BLASTER"};
const char* const kPowerName[] = {"",       "RAPID FIRE", "POWER SHOT",  "SHIELD",       "FREEZE",     "AIRSTRIKE",
                                  "MAGNET", "GIANTS",     "REINFORCEMENTS", "QUICK MARCH", "HOURGLASS", "DOUBLE GATES",
                                  "FIELD GUN"};
static_assert(sizeof kWeaponShort / sizeof *kWeaponShort == sim::kWeaponCount && sizeof kWeaponName / sizeof *kWeaponName == sim::kWeaponCount &&
                  sizeof kPowerName / sizeof *kPowerName == sim::kPowerCount,
              "a name for every weapon and power-up");
constexpr u32 kFrostC = 0x9CD4FF;

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
    // Each weapon's gun over the soldiers (the cannon is one piece at the squad's front instead), icon and projectile.
    const char* guns[] = {"musket", "twin", "scatter", "lance", "rocket", "flame", "cannon", "coil", "mortar", "frost"};
    const char* shots[] = {"fx.tracer", "fx.tracer_twin", "fx.pellet", "fx.bolt", "fx.rocket", "fx.flame", "fx.cannonball", "fx.lightning", "fx.shell",
                           "fx.frost_bolt"};
    static_assert(sizeof guns / sizeof *guns == sim::kWeaponCount && sizeof shots / sizeof *shots == sim::kWeaponCount, "every weapon");
    for (int w = 0; w < sim::kWeaponCount; ++w) {
        std::snprintf(name, sizeof name, "wpn.%s", guns[w]);
        ids_.wpn[w] = kind(name);
        std::snprintf(name, sizeof name, "ui.wpn_%s", guns[w]);
        ids_.wpnIcon[w] = kind(name);
        ids_.shotFx[w] = kind(shots[w]);
    }
    if (ids_.wpnIcon[sim::kCannon] < 0) ids_.wpnIcon[sim::kCannon] = kind("ui.pow_fieldgun");  // the field gun's cannon
    ids_.gatePostWpn = kind("gate.post_wpn");
    ids_.crateWpn = kind("crate.wpn");
    ids_.crateBreak = kind("fx.crate_break");
    ids_.muzzleWide = kind("fx.muzzle_wide");
    ids_.muzzleRing = kind("fx.muzzle_ring");
    ids_.puffSmall = kind("fx.puff_small");
    ids_.hitCyan = kind("fx.hit_cyan");
    ids_.pierce = kind("fx.pierce");
    ids_.smoke = kind("fx.smoke_trail");
    ids_.medal = kind("pickup.medal");
    ids_.lightColumn = kind("fx.light_column");
    ids_.crateSupply = kind("crate.supply");
    ids_.pickupFx = kind("fx.pickup");
    const char* pows[] = {nullptr,          "ui.pow_rapid",  "ui.pow_power",     "ui.pow_shield", "ui.pow_freeze",
                          "ui.pow_airstrike", "ui.pow_magnet", "ui.pow_giants",    "ui.pow_reinforce", "ui.pow_quick",
                          "ui.pow_hourglass", "ui.pow_double", "ui.pow_fieldgun"};
    static_assert(sizeof pows / sizeof *pows == sim::kPowerCount, "every power-up");
    for (int p = 1; p < sim::kPowerCount; ++p) ids_.powIcon[p] = kind(pows[p]);
    ids_.tracerHot = kind("fx.tracer_hot");
    ids_.gunGlow = kind("fx.gun_glow");
    ids_.shieldDome = kind("fx.shield_dome");
    ids_.shieldCrack = kind("fx.shield_crack");
    ids_.shieldBreak = kind("fx.shield_break");
    ids_.frostRing = kind("fx.frost_ring");
    ids_.frostOverlay = kind("fx.frost_overlay");
    ids_.bomb = kind("fx.bomb");
    ids_.shadowGrow = kind("fx.shadow_grow");
    ids_.archer = kind("archer.idle");
    ids_.archerShoot = kind("archer.shoot");
    ids_.arrow = kind("fx.arrow");
    ids_.mine = kind("haz.mine");
    ids_.cage = kind("cage.still");
    ids_.prisoner = kind("cage.prisoner");
    ids_.lightGreen = kind("fx.light_column_green");
    ids_.cageOpen = kind("fx.cage_open");
    ids_.rail = kind("gate.rail");
    ids_.coin = kind("fx.coin");
    const char* wheels[4] = {"gate.post_add_w", "gate.post_mul_w", "gate.post_neg_w", "gate.post_wpn_w"};
    for (int i = 0; i < 4; ++i) ids_.postWheel[i] = kind(wheels[i]);
    ids_.catapult = kind("catapult.idle");
    ids_.catapultFire = kind("catapult.fire");
    ids_.boulder = kind("fx.boulder");
    ids_.warnCircle = kind("fx.warn_circle");
    ids_.dirtBurst = kind("fx.dirt_burst");
    ids_.warnHalf = kind("fx.warn_half");
    ids_.waveRidge = kind("fx.wave_ridge");
    ids_.arrowGround = kind("fx.arrow_ground");
    ids_.giant = kind("giant.march");
    ids_.bossSlam = kind("boss.slam");
    ids_.bossThrow = kind("boss.throw");
    ids_.bossRoar = kind("boss.roar");
    ids_.cageBack = kind("cage.back");
    ids_.shieldman = kind("shieldman.march");
    ids_.shieldFront = kind("shield.front");
    ids_.bomber = kind("bomber.march");
    ids_.drummer = kind("drummer.march");
    ids_.squadCannon = kind("squad.cannon.march");
    ids_.flame = kind("fx.flame");
    ids_.zap = kind("fx.zap");
    ids_.lightning = kind("fx.lightning");
    ids_.shell = kind("fx.shell");
    ids_.targetRing = kind("fx.target_ring");
    ids_.magnetArc = kind("fx.magnet_arc");
    ids_.speedLines = kind("fx.speed_lines");
    ids_.soundRing = kind("fx.sound_ring");
    ids_.recruitFly = kind("fx.recruit_fly");
    ids_.scorch = kind("fx.char");
    ids_.goldRim = kind("fx.gate_gold_rim");
    const char* poses[3] = {"slam", "throw", "roar"};
    for (int w = 0; w < 10; ++w)
        for (int p = 0; p < 3; ++p) {
            std::snprintf(name, sizeof name, "w%d.boss_addon.%s", w + 1, poses[p]);
            ids_.addonPose[w][p] = art_->find(name);
        }
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

Field::Placed Field::sprite(int anim, float x, float z, float y, float t, float scale, float alpha, float rot, bool mirror, float flash,
                           u32 tint, float hue) {
    Placed out;
    float sx, sy, s;
    if (anim < 0 || !art_ || !cam_.project(x, z, y, sx, sy, s)) return out;
    out.elev = std::atan2(cam_.h - y, maxv(0.01f, z - cam_.z)) * (180.0f / kPi);
    // Between two renderings (camera elevations) a figure cross-fades instead of jumping from one to the next (the owner
    // saw the scenery pop, 2026-11-27): the lower one first at full strength while the upper one comes in over it, then
    // the upper at full while the lower fades out under it, so its body never turns see-through.
    float lo, hi;
    const float w = art_->elevBlend(anim, out.elev, lo, hi);
    const int n = w > 0.0f && w < 1.0f ? 2 : 1;
    const SpriteFrame* fr[2] = {&art_->frame(anim, n == 1 && w >= 1.0f ? hi : lo, t), &art_->frame(anim, hi, t)};
    const float a[2] = {n == 2 ? alpha * minv(1.0f, 2.0f - 2.0f * w) : alpha, alpha * minv(1.0f, 2.0f * w)};
    float cx[2], cy[2], hw[2], hh[2], k[2];
    bool seen = false;
    for (int i = 0; i < n; ++i) {
        const SpriteFrame& f = *fr[i];
        k[i] = f.height * s * scale / f.h;
        hw[i] = f.w * 0.5f * k[i];
        hh[i] = f.h * 0.5f * k[i];
        float ox = (f.w * 0.5f - f.px) * k[i], oy = (f.h * 0.5f - f.py) * k[i];
        if (mirror) ox = -ox;
        if (rot != 0.0f) {
            const float c = std::cos(rot), sn = std::sin(rot);
            const float rx = c * ox - sn * oy, ry = sn * ox + c * oy;
            ox = rx;
            oy = ry;
        }
        cx[i] = sx + ox;
        cy[i] = sy + oy;
        const float r = maxv(hw[i], hh[i]) * 1.42f;
        seen = seen || !(cx[i] + r < 0 || cx[i] - r > float(L_.w) || cy[i] + r < 0 || cy[i] - r > float(L_.h) || hh[i] < 0.4f);
    }
    const int main = n == 2 && w >= 0.5f ? 1 : 0;
    out.sx = sx;
    out.sy = sy;
    out.k = k[main];
    out.s = s;
    out.fr = fr[main];
    if (!seen) return out;
    Sticker* st = add(z, n);
    if (!st) return out;
    float turn = std::fmod(hue, 360.0f);
    if (turn < 0) turn += 360.0f;
    for (int i = 0; i < n; ++i) {
        const SpriteFrame& f = *fr[i];
        Sticker& q = st[i];
        q = Sticker{};
        q.x = cx[i];
        q.y = cy[i];
        q.hw = mirror ? -hw[i] : hw[i];
        q.hh = hh[i];
        q.rot = rot;
        q.shape = float(kShSprite);
        q.soft = f.layer;
        q.p0 = f.u0;
        q.p1 = f.v0;
        q.p2 = f.u1;
        q.fold = f.v1;
        setColor(q.c0, tint, a[i]);
        setColor(q.c1, 0xFFFFFF, flash);
        q.c2[0] = u8(turn / 360.0f * 255.0f + 0.5f);
        q.c2[3] = art_->anim(anim).additive ? 255 : 0;
    }
    out.st = st + main;
    out.st2 = n == 2 ? st + (1 - main) : nullptr;
    return out;
}

void Field::icon(UiBatch& b, int anim, float x, float y, float h, float alpha, float wScale) {
    if (anim < 0 || !art_ || h < 2.0f) return;
    const SpriteFrame& f = art_->frameAt(anim, 0, 0);
    const float uv[5] = {f.u0, f.v0, f.u1, f.v1, f.layer};
    b.sprite(uv, x, y, h * f.w / maxv(1.0f, f.h) * wScale, h, {255, 255, 255, u8(clampv(alpha, 0.0f, 1.0f) * 255.0f + 0.5f)});
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

// Bullets: streaks of light, no shadow. A big squad's ball carries squad / 50 musket balls (Battle::damage): it is drawn
// as a volley of up to four tracers fired from across the squad's front, converging on where it goes.
void Field::drawBullets(const sim::Battle& b, float step) {
    const int volley = clampv(int(shownSquad_ / 50.0f), 1, 4);
    const float R = minv(4.4f, 0.55f + 0.3f * std::sqrt(maxv(1.0f, shownSquad_)));
    for (int i = 0; i < b.bulletCount; ++i) {
        const sim::Bullet& bl = b.bullets[i];
        if (!bl.alive) continue;
        const sim::WeaponStats& ws = sim::weaponStats(bl.kind);
        const float z = bl.z + ws.speed * step, bx0 = bl.x + bl.vx * ws.speed * step;
        int fx = bl.kind < sim::kWeaponCount && ids_.shotFx[bl.kind] >= 0 ? ids_.shotFx[bl.kind] : ids_.tracer;
        // Under RAPID FIRE the musket's and twin's tracers burn orange; under POWER SHOT every shot is half again as big.
        if (b.powered(sim::kRapid) && ids_.tracerHot >= 0 && (bl.kind == sim::kMusket || bl.kind == sim::kTwin)) fx = ids_.tracerHot;
        const float big = b.powered(sim::kPowerShot) ? 1.5f : 1.0f;
        if (fx >= 0) {  // a streak along its flight (each weapon's own): up the road, toward its target
            const float y = bl.kind == sim::kRockets ? 0.9f : 0.62f;
            float ax, ay, bx, by, s;
            if (!cam_.project(bx0, z, y, ax, ay, s) || !cam_.project(bx0 + bl.vx, z + 1.0f, y, bx, by, s)) continue;
            const float rot = std::atan2(bx - ax, ay - by);
            sprite(fx, bx0, z, y, time, big, 1.0f, rot);
            if (bl.kind == sim::kCannon) continue;  // one ball, however many fire it
            const float flown = clampv((z - (bl.zEnd - ws.range)) / maxv(1.0f, ws.range), 0.0f, 1.0f), spread = (1.0f - flown) * R * 1.4f;
            for (int k = 1; k < volley; ++k) {
                const float ox = (h01(u32(i) * 131u + u32(k) * 7u) - 0.5f) * spread, oz = -0.8f * float(k) * (1.0f - flown);
                sprite(fx, bx0 + ox, z + oz, y, time, big * 0.9f, 0.9f, rot);
            }
            continue;
        }
        float sx, sy, s;
        if (!cam_.project(bx0, z, 0.62f, sx, sy, s)) continue;
        Sticker* st = add(z, 2);
        if (!st) break;
        st[0] = sticker(kShEllipse, sx, sy, maxv(2.0f, 0.13f * s), maxv(5.0f, 0.5f * s), 0xFFC040, 0xFFC040, 0xFFC040, 0.35f);
        st[1] = sticker(kShEllipse, sx, sy, maxv(1.2f, 0.055f * s), maxv(3.5f, 0.34f * s), 0xFFFBE0, 0xFFFBE0, 0xFFFBE0);
    }
}

void Field::launch(float x, float z, float power, float dir) {
    Particle& p = parts_[partHead_];
    partHead_ = (partHead_ + 1) % kParticles;
    partCount_ = minv(partCount_ + 1, kParticles);
    p = Particle{};
    p.x = x;
    p.z = z;
    p.y = 0.3f;
    p.vx = dir * (1.5f + 2.5f * rnd()) * power;
    p.vz = -(0.5f + 2.5f * rnd()) * power;  // back toward the camera
    p.vy = (5.0f + 3.0f * rnd()) * (0.75f + 0.35f * power);
    p.vr = (rnd() < 0.5f ? -1.0f : 1.0f) * (7.0f + 7.0f * rnd());
    p.rot = 0;
    p.max = p.life = 1.0f + 0.3f * power;
    p.size = 0.95f + 0.1f * rnd();
    p.kind = 9;
    p.anim = i16(ids_.soldier);
}

void Field::bolt(int anim, float x0, float z0, float x1, float z1, float life) {
    if (anim < 0 || !art_) return;
    bolts_[boltHead_] = {x0, z0, x1, z1, life, life, anim};
    boltHead_ = (boltHead_ + 1) % kBolts;
}

void Field::streak(int anim, float key, float ax, float ay, float bx, float by, float width, float t, float alpha) {
    Sticker* q = spriteAt(anim, key, ax, ay, 1.0f, t, 1.0f, alpha, 0);
    if (!q) return;
    const float dx = bx - ax, dy = by - ay;
    q->x = 0.5f * (ax + bx);
    q->y = 0.5f * (ay + by);
    q->hh = 0.5f * std::sqrt(dx * dx + dy * dy);
    q->hw = 0.5f * width;
    q->rot = std::atan2(dx, -dy);
}

// Each bolt in pieces of about 2.5 units (a jagged line), a little over the road, fading as it goes.
void Field::drawBolts() {
    for (const Bolt& bt : bolts_) {
        if (bt.life <= 0.0f || bt.anim < 0) continue;
        const float dx = bt.x1 - bt.x0, dz = bt.z1 - bt.z0;
        const int n = clampv(int(std::sqrt(dx * dx + dz * dz) / 2.5f) + 1, 1, 8);
        const float a = minv(1.0f, bt.life / bt.max * 1.5f);
        for (int i = 0; i < n; ++i) {
            const float f0 = float(i) / float(n), f1 = float(i + 1) / float(n);
            float ax, ay, as, bx, by, bs;
            if (!cam_.project(bt.x0 + dx * f0, bt.z0 + dz * f0, 0.8f, ax, ay, as) || !cam_.project(bt.x0 + dx * f1, bt.z0 + dz * f1, 0.8f, bx, by, bs))
                continue;
            streak(bt.anim, bt.z0 + dz * f0, ax, ay, bx, by, maxv(4.0f, 0.5f * (as + bs) * 0.7f), time + float(i) * 0.05f, a);
        }
    }
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
    if (def_->sky == sim::kSkyNight) {  // a cold blue grade and a deep vignette
        f.post.light = 0x6A7AD0;
        f.post.lightStrength = 0.5f;
        f.post.vignette = maxv(f.post.vignette, 0.75f);
    } else if (def_->sky == sim::kSkyFog) {
        g.hazeStart *= 0.5f;
        g.hazeEnd *= 0.6f;
    }
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
    static const float gap[24] = {0.075f, 0.05f, 0.035f, 0, 0, 0.06f, 0.07f, 0, 0, 0, 0, 0.09f, 0, 0.1f, 0, 0, 0, 0, 0.06f, 0, 0.12f, 0, 0, 0.08f};
    if (c < 24 && gap[c] > 0) {
        if (time - cueClock_[c] < gap[c]) return;
        cueClock_[c] = time;
    }
    if (cueN_ < kCues) cues_[cueN_++] = {c, weaponNow_, step, clampv(x / sim::kRoadHalf, -1.0f, 1.0f) * 0.6f};
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
    weaponNow_ = b.weapon < sim::kWeaponCount ? b.weapon : 0;
    const int shown = minv(kMaxShown, minv(int(tune.soldiersShown), maxv<i32>(b.squad, 0)));
    if (ev.shots > 0) {
        const int flashes = minv(ev.shots * clampv(shown / 50, 1, 4), 3 + shown / 40);  // a bigger squad, more muzzles
        for (int i = 0; i < flashes && shown > 0; ++i) flash_[int(rnd() * float(shown)) % shown] = 0.07f;
        cue(kCueShot, ev.shots, b.x);
    }
    if (ev.hits > 0) cue(kCueHit, 0, b.x);
    for (int i = 0; i < ev.killN; ++i) {
        const u8 kind = ev.killKind[i];
        if (kind == sim::kBoss) continue;
        const float size = kind == sim::kBrute ? 1.8f : (kind == sim::kRider ? 1.25f : 1.0f);
        const int figure = kind == sim::kBrute                          ? ids_.brute
                           : kind == sim::kRider                        ? ids_.rider
                           : kind == sim::kArcher && ids_.archer >= 0   ? ids_.archer
                           : kind == sim::kGiant && ids_.giant >= 0     ? ids_.giant
                           : kind == sim::kShieldman && ids_.shieldman >= 0 ? ids_.shieldman
                           : kind == sim::kBomber && ids_.bomber >= 0   ? ids_.bomber
                           : kind == sim::kDrummer && ids_.drummer >= 0 ? ids_.drummer
                                                                        : ids_.foot;
        if (figure >= 0) {  // the art: knocked back 0.3, shrinks and fades into a puff of its colour
            dying(figure, ev.killX[i], ev.killZ[i], 0.3f, 0.12f);
            flipbook(ids_.poofRed, ev.killX[i], ev.killZ[i] - 0.05f, 0.0f, kind == sim::kBrute ? 1.6f : kind == sim::kGiant ? 2.6f : 1.0f);
            // The flamethrower leaves a scorch where one burned.
            if (weaponNow_ == sim::kFlame && ids_.scorch >= 0 && rnd() < 0.3f) flipbook(ids_.scorch, ev.killX[i], ev.killZ[i] + 0.05f, 0.01f, 2.0f);
        } else {
            figureFalls(ev.killX[i], ev.killZ[i], size, kRed, kRedDark, kRedDeep, ev.killX[i] < b.x ? -1.0f : 1.0f);
            burst(ev.killX[i], ev.killZ[i], 0.5f, kind == sim::kBrute ? 8 : 2, kRed, kRedDark, 3.5f, 0.11f, 0);
        }
        cue(kCueKill, kind, ev.killX[i]);
    }
    // The boss: a blast on its body for every tenth of its strength gone.
    if (const int bi0 = b.bossIndex(); bi0 >= 0 && ids_.explosion >= 0 && def_->bossHp > 0) {
        const sim::Enemy& e = b.enemies[bi0];
        const int tenth = int(std::ceil(10.0f * float(e.hp) / float(def_->bossHp * sim::kHp)));
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
        float g0, g1;
        b.gateSpan(g, b.time, g0, g1);
        cue(kCueGrow, minv(gs.value, 24), (g0 + g1) * 0.5f);
        if (ids_.hit >= 0 && rnd() < 0.5f)
            flipbook(ids_.hit, g0 + 0.3f + rnd() * (g1 - g0 - 0.6f), def_->gates[g].z - 0.05f, 0.5f + rnd() * 1.4f, 1.0f, rnd() * 6.28f);
    }
    for (int k = 0; k < def_->barrelCount; ++k) {
        const sim::BarrelState& bs = b.barrels[k];
        if (bs.broken || bs.hit > sim::kDt * 1.5f) continue;
        cue(kCueBarrelHit, 0, def_->barrels[k].x);
        if (rnd() < 0.3f) burst(def_->barrels[k].x, def_->barrels[k].z - 0.4f, 0.8f, 1, kGoldC, kGoldDarkC, 2.5f, 0.09f, 0);
    }
    // Rockets bursting, lance bolts going through.
    for (int i = 0; i < ev.spotN; ++i) {
        const float x = ev.spotX[i], z = ev.spotZ[i];
        if (ev.spotWhat[i] == 0) {
            if (ids_.explosion >= 0) {
                flipbook(ids_.explosionGlow, x, z - 0.12f, 0.0f, 0.8f);
                flipbook(ids_.explosion, x, z - 0.1f, 0.0f, 0.8f);
                flipbook(ids_.ring, x, z - 0.2f, 0.02f, 0.8f);
            } else {
                burst(x, z, 0.6f, 8, 0xFFB040, kSpark, 5.0f, 0.12f, 3);
            }
            shake_ = maxv(shake_, 0.12f);
            cue(kCueBlast, 0, x);
        } else if (ev.spotWhat[i] == 1) {
            if (ids_.pierce >= 0) flipbook(ids_.pierce, x, z - 0.05f, 0.7f, 1.0f);
        } else if (ev.spotWhat[i] == 3) {  // a mine: a blast at 0.7, a ring
            if (ids_.explosion >= 0) {
                flipbook(ids_.explosionGlow, x, z - 0.12f, 0.0f, 0.7f);
                flipbook(ids_.explosion, x, z - 0.1f, 0.0f, 0.7f);
                flipbook(ids_.ring, x, z - 0.2f, 0.02f, 0.9f);
            } else {
                burst(x, z, 0.5f, 10, 0xFFB040, kSpark, 5.0f, 0.12f, 3);
            }
            shake_ = maxv(shake_, 0.3f);
            cue(kCueBlast, 0, x);
        } else if (ev.spotWhat[i] == 4 || ev.spotWhat[i] == 5) {  // a bomber's keg, a mortar's shell: a blast and dirt
            const float k = ev.spotWhat[i] == 4 ? 0.9f : 1.1f;
            if (ids_.explosion >= 0) {
                flipbook(ids_.explosionGlow, x, z - 0.12f, 0.0f, k);
                flipbook(ids_.explosion, x, z - 0.1f, 0.0f, k);
                flipbook(ids_.ring, x, z - 0.2f, 0.02f, k);
                if (ids_.dirtBurst >= 0 && ev.spotWhat[i] == 5) flipbook(ids_.dirtBurst, x, z - 0.15f, 0.0f, 0.8f);
            } else {
                burst(x, z, 0.6f, 10, 0xFFB040, kSpark, 5.0f, 0.12f, 3);
            }
            shake_ = maxv(shake_, ev.spotWhat[i] == 4 ? 0.3f : 0.2f);
            cue(kCueBlast, 0, x);
        } else if (ev.spotWhat[i] == 6) {  // a cannonball strikes one in its way
            if (ids_.hit >= 0) flipbook(ids_.hit, x, z - 0.05f, 0.6f, 1.4f, rnd() * 6.28f);
            if (ids_.dust >= 0 && rnd() < 0.5f) flipbook(ids_.dust, x, z, 0.0f, 1.2f);
        } else {  // an airstrike's bomb: a big blast, half size, its glow and a ring
            if (ids_.bigboom >= 0) {
                flipbook(ids_.bigboomGlow, x, z - 0.12f, 0.0f, 0.5f);
                flipbook(ids_.bigboom, x, z - 0.1f, 0.0f, 0.5f);
                flipbook(ids_.ring, x, z - 0.2f, 0.02f, 1.2f);
            } else {
                burst(x, z, 0.6f, 14, 0xFFB040, kSpark, 6.0f, 0.15f, 3);
            }
            shake_ = maxv(shake_, 0.45f);
            cue(kCueBomb, 0, x);
        }
    }
    // The storm coil's bolts, a spark where each strikes; the flamethrower's jet while it burns something.
    for (int i = 0; i < ev.zapN; ++i) {
        bolt(ids_.lightning, ev.zapX0[i], ev.zapZ0[i], ev.zapX1[i], ev.zapZ1[i], 0.12f);
        if (i < 3 && ids_.zap >= 0) flipbook(ids_.zap, ev.zapX1[i], ev.zapZ1[i] - 0.05f, 0.7f, 0.9f, rnd() * 6.28f);
    }
    if (weaponNow_ == sim::kFlame && ev.hits > 0) flameT_ = 0.3f;
    if (ev.magnet >= 0) {  // the magnet pulls it open: an arc from the squad
        const bool cage = ev.magnet >= 100;
        const float tx = cage ? def_->cages[ev.magnet - 100].x : def_->barrels[ev.magnet].x;
        const float tz = cage ? def_->cages[ev.magnet - 100].z : def_->barrels[ev.magnet].z;
        bolt(ids_.magnetArc, b.x, b.z + 0.5f, tx, tz, 0.4f);
        if (ids_.sparkle >= 0) flipbook(ids_.sparkle, tx, tz - 0.1f, 0.8f, 1.2f);
    }
    for (int i = 0; i < minv<i32>(ev.reinforced, 3) && ids_.recruitFly >= 0; ++i) {  // reinforcements float down on to it
        Particle& p = parts_[partHead_];
        partHead_ = (partHead_ + 1) % kParticles;
        partCount_ = minv(partCount_ + 1, kParticles);
        p = Particle{};
        p.x = b.x + (rnd() - 0.5f) * b.radius() * 1.6f;
        p.z = b.z + (rnd() - 0.3f) * b.radius();
        p.y = 4.5f + rnd() * 1.5f;
        p.vz = b.halted ? 0.0f : def_->speed;
        p.vy = -3.0f;
        p.max = p.life = 3.0f;
        p.size = 1.0f;
        p.rot = rnd() * 6.28f;
        p.kind = 7;
        p.anim = i16(ids_.recruitFly);
    }
    char t[24];
    if (ev.doubled) floater(0, 1.0f, 5.8f, "DOUBLED!", kGoldC, 0x16224F, 1.0f, true);
    if (ev.wallDown >= 0) {
        const float wz = def_->walls[ev.wallDown].z;
        for (int i = 0; i < 5; ++i) {
            const float x = -4.0f + 2.0f * float(i);
            if (ids_.explosion >= 0) flipbook(ids_.explosion, x, wz - 0.1f, 0.6f + rnd() * 1.2f, 0.9f, 0, 0.06f * float(i));
            if (ids_.dust >= 0) flipbook(ids_.dust, x, wz, 0.0f, 2.2f, 0, 0.1f + 0.05f * float(i));
        }
        burst(0.0f, wz, 1.2f, reduceEffects ? 10 : 28, 0xC9BFAE, 0x8A8070, 7.0f, 0.22f, 0);
        floater(0.0f, wz, 3.2f, "WALL DOWN!", kWhiteC, 0x16224F, 1.2f);
        shake_ = maxv(shake_, 0.7f);
        cue(kCueBomb, 0, 0.0f);
    }
    if (ev.power > 0 && ev.power < sim::kPowerCount) {  // a power-up: its name, a burst of light, its sound
        std::snprintf(t, sizeof t, "%s!", kPowerName[ev.power]);
        floater(0, 1.0f, 5.2f, t, kGoldC, 0x16224F, 1.1f, true);
        if (ids_.pickupFx >= 0) flipbook(ids_.pickupFx, b.x, b.z + 0.5f, 0.8f, 1.0f);
        if (ev.power == sim::kFreeze && ids_.frostRing >= 0) flipbook(ids_.frostRing, b.x, b.z + 1.0f, 0.03f, 9.0f);
        cue(kCuePower, ev.power, b.x);
        badgePop_ = 1;
    }
    if (ev.absorbed > 0 && ids_.shieldCrack >= 0) {  // the shield takes the blow
        const float r = b.radius(), a = kPi * (0.15f + 0.7f * rnd());
        flipbook(ids_.shieldCrack, b.x + std::cos(a) * r, b.z + std::sin(a) * r * 0.8f, 0.8f + rnd(), 1.0f);
        cue(kCueShield, 0, b.x);
    }
    if (ev.shieldGone) {
        if (ids_.shieldBreak >= 0) flipbook(ids_.shieldBreak, b.x, b.z - b.radius() - 0.5f, 0.0f, maxv(1.0f, b.radius()));
        cue(kCueShield, 1, b.x);
    }
    if (ev.cage >= 0) {  // prisoners freed: their number, a green burst, sparkles
        const sim::CageDef& cd = def_->cages[ev.cage];
        std::snprintf(t, sizeof t, "+%d", cd.reward);
        floater(cd.x, cd.z, 2.4f, t, 0x8CFF9C, 0x16224F, 1.05f);
        if (ids_.cageOpen >= 0) flipbook(ids_.cageOpen, cd.x, cd.z - 0.1f, 0.0f, 1.0f);
        else burst(cd.x, cd.z, 0.8f, 14, 0x4ED87A, 0x2E9E4E, 5.0f, 0.13f, 0);
        if (ids_.sparkle >= 0)
            for (int i = 0; i < 8; ++i) flipbook(ids_.sparkle, cd.x + (rnd() - 0.5f) * 2.0f, cd.z + (rnd() - 0.5f), 0.8f, 1.0f, 0, 0.03f * float(i));
        cue(kCueCage, 0, cd.x);
        badgePop_ = 1;
    }
    if (ev.mineStepped) {
        floater(0, 1.0f, 4.4f, "-6", 0xFF7A86, 0x16224F, 1.0f, true);
        edge_ = 1.0f;
    }
    if (ev.arrows > 0) cue(kCueArrow, ev.arrows, b.x);
    // The boss's attacks: its pose, a roar's ring of sound; blows landing (dirt, dust, a tremor).
    if (ev.strikeShown == 1) bossAct_ = 1, bossActT_ = 0;
    if (ev.bossThrew) bossAct_ = 2, bossActT_ = 0;
    if (ev.roar) {
        bossAct_ = 3, bossActT_ = 0;
        if (const int bi1 = b.bossIndex(); bi1 >= 0 && ids_.ring >= 0) flipbook(ids_.ring, b.enemies[bi1].x, b.enemies[bi1].z - 0.5f, 3.0f, 2.0f);
        floater(0, 1.0f, 5.0f, "WAR CRY!", 0xFF7A86, 0x16224F, 1.1f, true);
        cue(kCueBoss, 0, 0);
    }
    if (ev.thrown > 0) cue(kCueArrow, 6, b.x);
    if (ev.strikeLanded >= 0) {
        const float x = ev.strikeX, z = ev.strikeZ;
        if (ev.strikeLanded == 0) {
            if (ids_.dirtBurst >= 0) flipbook(ids_.dirtBurst, x, z - 0.1f, 0.0f, 1.0f);
            else burst(x, z, 0.4f, 12, 0x8A6A4A, 0x5A4A3A, 5.0f, 0.15f, 0);
        } else {
            for (int i = 0; i < 5; ++i) {
                const float dx = ev.strikeLanded == 1 ? (x < 0.0f ? -1.0f : 1.0f) * (0.5f + rnd() * 4.0f) : (rnd() - 0.5f) * 9.0f;
                flipbook(ids_.dust, dx, z + (rnd() - 0.3f) * 3.0f, 0.0f, 2.0f, 0, 0.03f * float(i));
            }
        }
        shake_ = maxv(shake_, ev.strikeLanded == 0 ? 0.35f : 0.6f);
        cue(kCueBomb, 0, x);
    }
    if (ev.weapon >= 0 && ev.weapon < sim::kWeaponCount) {  // a new weapon: its name over the squad, a fanfare
        std::snprintf(t, sizeof t, "%s!", kWeaponName[ev.weapon]);
        floater(0, 1.0f, 4.8f, t, kOrangeC, 0x16224F, 1.15f, true);
        if (ids_.sparkle >= 0)
            for (int i = 0; i < 10; ++i) flipbook(ids_.sparkle, b.x + (rnd() - 0.5f) * b.radius() * 2.0f, b.z + (rnd() - 0.5f) * b.radius(), 0.8f, 1.0f, 0, 0.03f * float(i));
        cue(kCueWeapon, ev.weapon, b.x);
        badgePop_ = 1;
    }
    if (ev.gate >= 0) {
        const sim::GateDef& g = def_->gates[ev.gate];
        const i32 delta = ev.gateAfter - ev.gateBefore;
        const bool good = delta >= 0;
        const i32 face = b.gateNow(ev.gate, b.time);
        if (g.kind == sim::kGateMul) std::snprintf(t, sizeof t, "\xC3\x97%d", face);
        else std::snprintf(t, sizeof t, "%+d", face);
        if (g.kind != sim::kGateWeapon) floater(0, 1.0f, 4.4f, t, good ? kWhiteC : 0xFF7A86, 0x16224F, good ? 1.1f : 1.0f, true);
        const u32 c = g.kind == sim::kGateMul ? kVioletC : (face > 0 ? kBlue : kRed);
        gateUsedAt_[ev.gate] = time;
        const int side = g.kind == sim::kGateMul ? 1 : (ev.gateBefore >= 0 && face >= 0 ? 0 : 2);
        float g0, g1;
        b.gateSpan(ev.gate, b.time, g0, g1);
        if (def_->mode == sim::kModeGold && ids_.coin >= 0 && good)  // a bonus road: gold coins burst from every gate
            for (int i = 0; i < (reduceEffects ? 4 : 9); ++i) {
                Particle& p = parts_[partHead_];
                partHead_ = (partHead_ + 1) % kParticles;
                partCount_ = minv(partCount_ + 1, kParticles);
                p = Particle{};
                p.x = g0 + rnd() * (g1 - g0);
                p.z = g.z;
                p.y = 1.0f + rnd();
                p.vx = (rnd() - 0.5f) * 3.0f;
                p.vz = (rnd() - 0.3f) * 2.0f;
                p.vy = 4.0f + rnd() * 3.0f;
                p.max = p.life = 1.4f + rnd() * 0.6f;
                p.size = 1.0f;
                p.rot = rnd() * 6.28f;
                p.kind = 0;  // ballistic, spinning
                p.anim = i16(ids_.coin);
            }
        if (ids_.gatePass[side] >= 0) {
            flipbook(ids_.gatePass[side], (g0 + g1) * 0.5f, g.z - 0.05f, 1.2f, 1.0f);
            if (side < 2) {
                confetti((g0 + g1) * 0.5f, g.z, 1.4f, 24, g1 - g0);
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
        if (g.kind != sim::kGateWeapon) cue(good ? kCueGate : kCueGateBad, step, b.x);
        badgePop_ = 1;
    }
    if (ev.barrel >= 0 && (def_->barrels[ev.barrel].weapon > 0 || def_->barrels[ev.barrel].power > 0)) {  // a crate opens
        const sim::BarrelDef& bd = def_->barrels[ev.barrel];
        if (ids_.crateBreak >= 0) flipbook(ids_.crateBreak, bd.x, bd.z - 0.1f, 0.0f, 1.0f);
        else burst(bd.x, bd.z, 0.7f, 14, kOrangeC, kWoodC, 6.0f, 0.15f, 0);
        if (ids_.debris >= 0) {
            const int first = partHead_;
            burst(bd.x, bd.z, 0.7f, 8, kWoodC, kWoodC, 6.0f, 1.0f, 0);
            for (int i = 0; i < (reduceEffects ? 3 : 8); ++i) parts_[(first + i) % kParticles].anim = i16(ids_.debris);
        }
        shake_ = maxv(shake_, 0.25f);
    } else if (ev.barrel >= 0) {
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
        // Soldiers hit are knocked flying from the squad's front (from where a club or a giant struck, harder), with a
        // spark where each was hit; a few more flash red; the count lost rises over the squad.
        const float r = b.radius();
        const int fly = minv(ev.lost, ev.blow ? (reduceEffects ? 5 : 12) : (reduceEffects ? 2 : 5));
        for (int i = 0; i < fly; ++i) {
            float x, z;
            if (ev.blow) {
                x = clampv(ev.blowX + (rnd() - 0.5f) * 2.4f, b.x - r, b.x + r);
                z = b.z + r * (0.2f + 0.5f * rnd());
            } else {
                const float a = kPi * (0.15f + 0.7f * rnd());
                x = b.x + std::cos(a) * r;
                z = b.z + std::sin(a) * r * 0.8f;
            }
            if (ids_.soldier >= 0) {
                launch(x, z, ev.blow ? 2.0f : 1.0f, x < b.x ? -1.0f : 1.0f);
                if (ids_.hit >= 0 && i < 4) flipbook(ids_.hit, x, z - 0.1f, 0.7f, 1.0f, rnd() * 6.28f);
            } else {
                figureFalls(x, z, 0.9f, kBlue, kBlueDark, kBlueDeep, rnd() < 0.5f ? -1.0f : 1.0f);
            }
        }
        const int shown = minv(kMaxShown, minv(int(tune.soldiersShown), maxv<i32>(b.squad, 1)));
        for (int i = 0; i < minv(ev.lost * 2, 10); ++i) hurt_[int(rnd() * float(shown)) % shown] = 0.22f;
        if (ev.gate < 0) {
            std::snprintf(t, sizeof t, "-%d", ev.lost);
            if (ev.crashed >= 0) floater(0, 1.0f, 4.4f, t, 0xFF7A86, 0x16224F, 1.0f, true);
            else lossAcc_ += float(ev.lost);
        }
        if (ev.lost * 20 >= maxv<i32>(b.squad, 1)) {  // a twentieth of the squad or more: the screen's edge flashes red
            edge_ = maxv(edge_, 0.6f);
            shake_ = maxv(shake_, 0.15f);
        }
        cue(kCueLoss, ev.lost, b.x);
    }
    if (ev.blow) {  // a club landed: dust, a ring on the road, a jolt
        if (ids_.ring >= 0) flipbook(ids_.ring, ev.blowX, ev.blowZ, 0.02f, ev.blowBoss ? 1.6f : 1.0f);
        if (ids_.dirtBurst >= 0) flipbook(ids_.dirtBurst, ev.blowX, ev.blowZ - 0.1f, 0.0f, ev.blowBoss ? 1.4f : 1.0f);
        for (int i = 0; i < 3 && ids_.dust >= 0; ++i) flipbook(ids_.dust, ev.blowX + (rnd() - 0.5f) * 3.0f, ev.blowZ - 0.3f, 0.0f, 2.2f);
        shake_ = maxv(shake_, ev.blowBoss ? 0.55f : 0.35f);
        cue(kCueBomb, 0, ev.blowX);
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
    // Rockets leave a trail of smoke: a puff behind each every 0.05 s (a dozen at most at a time).
    smokeClock_ += dt;
    if (smokeClock_ >= 0.05f) {
        smokeClock_ = std::fmod(smokeClock_, 0.05f);
        int n = 0;
        for (int i = 0; i < b.bulletCount && n < 12 && ids_.smoke >= 0; ++i) {
            const sim::Bullet& bl = b.bullets[i];
            if (!bl.alive || bl.kind != sim::kRockets) continue;
            flipbook(ids_.smoke, bl.x - bl.vx * 0.6f, bl.z - 0.6f, 0.9f, 0.8f + 0.3f * rnd(), rnd() * 6.28f);
            ++n;
        }
    }
    shake_ = maxv(0.0f, shake_ - dt * 2.2f);
    zoom_ = maxv(0.0f, zoom_ - dt * 0.6f);
    bossActT_ += dt;
    edge_ = maxv(0.0f, edge_ - dt / 0.3f);
    for (float& f : flash_) f = maxv(0.0f, f - dt);
    for (float& f : hurt_) f = maxv(0.0f, f - dt);
    lossClock_ += dt;
    if (lossClock_ >= 0.45f) {
        lossClock_ = 0;
        if (lossAcc_ >= 1.0f) {
            char t[16];
            std::snprintf(t, sizeof t, "-%d", int(lossAcc_ + 0.5f));
            floater((rnd() - 0.5f) * 1.5f, 1.2f, 4.0f, t, 0xFF5A6A, 0x3A0A14, 0.95f, true);
        }
        lossAcc_ = 0;
    }
    flameT_ = maxv(0.0f, flameT_ - dt);
    for (Bolt& bt : bolts_) bt.life -= dt;
    for (int i = 0; i < partCount_; ++i) {
        Particle& p = parts_[i];
        if (p.life <= 0) continue;
        p.life -= dt;
        p.x += p.vx * dt;
        p.z += p.vz * dt;
        p.y += p.vy * dt;
        p.rot += p.vr * dt;
        if (p.kind == 0 || p.kind == 3 || p.kind == 9) p.vy -= 14.0f * dt;
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
            const bool mul = gd.kind == sim::kGateMul, wpn = gd.kind == sim::kGateWeapon;
            const float tNow = b.time + gateStep_;
            const i32 value = b.gateNow(g, tNow);  // a flip gate's face now
            const bool hidden = gd.trick == 2 && gs.seen < 5;
            if (gd.trick == 1) {  // it turns over (the panel squashes to nothing and back) around each change
                const float ph = std::fmod(tNow, 1.5f), near = minv(ph, 1.5f - ph);
                if (near < 0.15f) collapse *= maxv(0.08f, near / 0.15f);
            }
            const int kind = wpn ? 3 : (mul ? 1 : (value > 0 ? 0 : 2));
            // Blue adds, violet multiplies, red takes; a weapon is a cream panel in an orange frame (ART_DIRECTION 16).
            static const u32 kC[4] = {0x2E8BFF, 0x9B4DFF, 0xF0323C, 0xFFF6E0}, kH[4] = {0xBFE4FF, 0xE6CCFF, 0xFFC2B8, 0xFF8A1E};
            const bool grey = !mul && !wpn && value == 0;
            const u32 C = hidden ? 0x8A84B8 : grey ? 0x8C96A8 : kC[kind], Hc = hidden ? 0xDCD8F0 : grey ? 0xD8DEE8 : kH[kind];
            const float top = kBottom + (kTop - kBottom) * collapse;
            // Where it stands now (a moving gate slides on a rail across the road, its posts on wheels).
            float gx0, gx1;
            b.gateSpan(g, b.time + gateStep_, gx0, gx1);
            if (gd.moves && ids_.rail >= 0 && !gs.used)
                for (float rx = -4.0f; rx <= 4.01f; rx += 2.0f) sprite(ids_.rail, rx, gd.z + 0.05f, 0, 0, 1.0f, fade);
            float xl, yb, sb, xr, yr, sr, xt, yt, st;
            if (!cam_.project(gx0, gd.z, kBottom, xl, yb, sb) || !cam_.project(gx1, gd.z, kBottom, xr, yr, sr) ||
                !cam_.project(gx0, gd.z, top, xt, yt, st))
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
            int post = wpn ? (ids_.gatePostWpn >= 0 ? ids_.gatePostWpn : ids_.gatePost[0]) : ids_.gatePost[kind];
            if (gd.moves && ids_.postWheel[kind] >= 0) post = ids_.postWheel[kind];
            for (int side = 0; side < 2; ++side) {
                const float x = side ? gx1 : gx0;
                if (sprite(post, x, gd.z, 0, time, 1.0f, fade).st) castShadow(x, gd.z, 2.7f, 0.18f);
            }
            if (kind == 2 && ids_.gateSpikes >= 0)
                for (float x = gx0 + 0.5f; x < gx1 - 0.2f; x += 1.0f) sprite(ids_.gateSpikes, x, gd.z - 0.02f, 0, time, 1.0f, fade);
            if (wpn) {  // its weapon's icon, 1.3 units tall, the name under it
                const int w = clampv<int>(gs.value, 0, sim::kWeaponCount - 1);
                const float ph = yb - yt, cx = (xl + xr) * 0.5f;
                if (fade > 0.3f) {
                    icon(labels, ids_.wpnIcon[w], cx, yt + ph * 0.42f, minv(1.3f * sb, ph * 0.66f), fade);
                    float size = 0.42f * sb * tune.tagScale;
                    const float tw = UiBatch::textWidth(kWeaponShort[w], size);
                    if (tw > (xr - xl) * 0.85f) size *= (xr - xl) * 0.85f / tw;
                    if (size >= 7.0f) label(labels, kWeaponShort[w], cx, yt + ph * 0.86f, size, 0x16224F, 0xFFF6E0);
                }
                continue;
            }
            // Its number: white, outlined in a dark shade of its colour, pulsing when it changes. A mystery gate shows "?" and
            // its two outcomes under it; a flip gate the face it turns to next, small over its top.
            char t[16];
            if (hidden) std::snprintf(t, sizeof t, "?");
            else if (mul) std::snprintf(t, sizeof t, "\xC3\x97%d", value);
            else if (value > 0) std::snprintf(t, sizeof t, "+%d", value);
            else std::snprintf(t, sizeof t, "%d", value);
            if ((hidden || gd.trick == 1) && fade > 0.3f && !gs.used) {
                char u[32];
                const i32 other = gd.trick == 1 ? (value == gd.alt ? gs.value : gd.alt) : 0;
                if (hidden) std::snprintf(u, sizeof u, "%+d OR %+d", maxv(gs.value, gd.alt), minv(gs.value, gd.alt));
                else std::snprintf(u, sizeof u, "NEXT %+d", other);
                const float us = 0.42f * sb * tune.tagScale;
                if (us >= 7.0f) label(labels, u, (xl + xr) * 0.5f, hidden ? (yb + yt) * 0.5f + 0.75f * sb : yt - 0.35f * sb, us, kWhiteC, 0x16224F);
            }
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
        const bool supply = bd.power > 0 && bd.power < sim::kPowerCount;
        const bool crate = supply || (bd.weapon > 0 && bd.weapon < sim::kWeaponCount);
        const int crateArt = supply ? ids_.crateSupply : ids_.crateWpn;
        if (crate && crateArt >= 0) {
            // A weapon or supply crate: its weapon's or power-up's icon bobbing over it, its strength above (ART_DIRECTION 16).
            const Placed pl = sprite(crateArt, bd.x + jig, bd.z, 0, time, 1.0f, 1.0f, 0, false, flash * 0.3f);
            if (pl.st) {
                castShadow(bd.x, bd.z, 1.25f, 0.6f);
                const float iy = sy - (1.5f + 0.08f * std::sin(time * 3.0f + float(k))) * s;
                icon(labels, supply ? ids_.powIcon[bd.power] : ids_.wpnIcon[bd.weapon], sx, iy, 0.9f * s);
                const float bw = 0.95f * s, bh = maxv(3.0f, 0.13f * s), by = iy - 0.62f * s;
                const float frac = clampv(float(bs.hp) / float(maxv<i32>(bd.hp * sim::kHp, 1)), 0.0f, 1.0f);
                if (0.72f * s * tune.tagScale >= 7.0f) {
                    labels.rect(sx, by, bw, bh, bh * 0.5f, rgba(0x3A2410, 200));
                    if (frac > 0) labels.rect(sx - bw * 0.5f * (1.0f - frac), by, bw * frac, bh * 0.7f, bh * 0.35f, rgba(supply ? kGoldC : kOrangeC));
                }
            }
            continue;
        }
        if (ids_.barrel >= 0) {
            const Placed pl = sprite(ids_.barrel, bd.x + jig, bd.z, 0, time, 1.0f, 1.0f, 0, false, flash * 0.3f * (0.5f + 0.5f * std::sin(time * 47.0f)));
            if (pl.fr) top = pl.sy - pl.fr->py * pl.k;
            if (pl.st) {
                for (Sticker* q : {pl.st, pl.st2})
                    if (q) {
                        q->hh *= 1.0f - 0.06f * flash;  // squashed by the hit, feet on the road
                        q->y += q->hh * 0.06f * flash;
                    }
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
        if (crate) std::snprintf(t, sizeof t, "%s", supply ? kPowerName[bd.power] : kWeaponShort[bd.weapon]);
        else std::snprintf(t, sizeof t, "+%d", bd.reward);
        const float size = 0.72f * s * tune.tagScale;
        const float ly = top - 0.62f * s;
        if (size >= 7.0f) {
            label(labels, t, sx, ly, size, kWhiteC, kGoldDeep);
            const float bw = 0.95f * s, bh = maxv(3.0f, 0.13f * s);
            const float frac = clampv(float(bs.hp) / float(maxv<i32>(bd.hp * sim::kHp, 1)), 0.0f, 1.0f);
            labels.rect(sx, ly + size * 0.62f, bw, bh, bh * 0.5f, rgba(0x3A2410, 200));
            if (frac > 0) labels.rect(sx - bw * 0.5f * (1.0f - frac), ly + size * 0.62f, bw * frac, bh * 0.7f, bh * 0.35f, rgba(0xFFE24A));
        }
    }
}

// Medallions spinning in their light, the shield's dome over the squad, an airstrike's bombs falling (ART_DIRECTION 16).
void Field::drawPowers(const sim::Battle& b, UiBatch& labels) {
    if (!art_) return;
    const float zMin = cam_.z + cam_.tBottom - 1.0f, zMax = cam_.z + cam_.tTop + 2.0f;
    for (int m = 0; m < def_->medalCount + b.dropCount; ++m) {  // the level's medallions, then those giants dropped
        const bool drop = m >= def_->medalCount;
        const sim::MedalDef& md = drop ? b.drops[m - def_->medalCount] : def_->medals[m];
        if ((drop ? b.dropTaken[m - def_->medalCount] : b.medalTaken[m]) || md.z < zMin || md.z > zMax) continue;
        const float t = time + float(m) * 0.31f;
        if (ids_.lightColumn >= 0) sprite(ids_.lightColumn, md.x, md.z + 0.01f, 0, t, 1.0f, 0.8f);
        const Placed pl = sprite(ids_.medal, md.x, md.z, 0, t, 1.0f, 1.0f);
        if (!pl.st) continue;
        castShadow(md.x, md.z, 0.3f, 0.35f);
        float sx, sy, s;  // its power-up's icon on the disc, turning with it
        if (cam_.project(md.x, md.z, 1.0f, sx, sy, s))
            icon(labels, ids_.powIcon[md.power < sim::kPowerCount ? md.power : 0], sx, sy, 0.7f * s, 1.0f,
                 maxv(0.08f, std::fabs(std::cos(kPi * std::floor(t * 10.0f) / 8.0f))));
    }
    if (b.powered(sim::kShield) && ids_.shieldDome >= 0 && !b.lost) {  // over the whole squad, flickering as it runs out
        const float R = minv(4.4f, 0.55f + 0.3f * std::sqrt(maxv(1.0f, shownSquad_)));
        const float left = b.powerLeft[sim::kShield], a = left < 1.5f ? 0.55f + 0.45f * std::sin(time * 18.0f) : 1.0f;
        float sx, sy, s;
        if (cam_.project(viewX_, viewZ_, 0, sx, sy, s)) spriteAt(ids_.shieldDome, viewZ_ - R - 1.0f, sx, sy, s, time, R, 0.85f * a);
    }
    if (b.powered(sim::kQuick) && ids_.speedLines >= 0 && !b.halted && !b.won)  // QUICK MARCH: lines streaming past it
        sprite(ids_.speedLines, viewX_, viewZ_ - 0.5f, 0.6f, time, 0.9f + 0.25f * std::sqrt(maxv(1.0f, shownSquad_)) / 3.0f, 0.75f);
    if (b.powered(sim::kDouble) && ids_.goldRim >= 0)  // DOUBLE GATES: the next gates that add glow gold at their feet
        for (int g = 0, shown = 0; g < def_->gateCount && shown < 3; ++g) {
            const sim::GateDef& gd = def_->gates[g];
            if (gd.kind != sim::kGateAdd || b.gates[g].used || b.gates[g].passed || b.gates[g].value <= 0 || gd.z < b.z || gd.z > zMax) continue;
            float g0, g1;
            b.gateSpan(g, b.time, g0, g1);
            const Placed pl = sprite(ids_.goldRim, 0.5f * (g0 + g1), gd.z - 0.05f, 0, time, (g1 - g0) / 5.0f, 0.9f);
            for (Sticker* q : {pl.st, pl.st2})
                if (q) q->hh *= maxv(0.12f, std::sin(pl.elev * kPi / 180.0f));
            ++shown;
        }
    for (int k = 0; k < b.bombCount; ++k) {  // falling the last half second, their shadows growing under them
        const sim::Battle::Bomb& bm = b.bombs[k];
        if (bm.t > 0.5f) continue;
        const float f = 1.0f - bm.t / 0.5f;
        if (ids_.shadowGrow >= 0) sprite(ids_.shadowGrow, bm.x, bm.z + 0.02f, 0, 0, 0.3f + 0.7f * f, 0.35f + 0.5f * f);
        if (ids_.bomb >= 0) sprite(ids_.bomb, bm.x, bm.z, 12.0f * (1.0f - f), 0, 1.0f, 1.0f);
    }
}

// The blows shown on the road before they land: a boulder's red circle with the boulder arcing at it, the club's half of
// the road in red chevrons, the stomp's ridge rolling down with its gap (an arrow on the road points at it). Warnings lie
// flat: a sprite as wide as `width` road units, squashed by the view's elevation.
void Field::drawStrikes(const sim::Battle& b, float step) {
    auto flat = [&](int anim, float x, float z, float width, float alpha) {
        if (anim < 0 || !art_) return false;
        const SpriteFrame& f0 = art_->frame(anim, 0, 0);
        const float unitsW = f0.w * f0.height / maxv(1.0f, f0.h);
        const Placed pl = sprite(anim, x, z, 0.03f, time, width / maxv(0.01f, unitsW), alpha);
        for (Sticker* q : {pl.st, pl.st2})
            if (q) q->hh *= maxv(0.12f, std::sin(pl.elev * kPi / 180.0f));
        return true;
    };
    auto redZone = [&](float x0, float x1, float z0, float z1, float alpha) {  // without the art: a red patch on the road
        float ax, ay, as, bx, by, bs;
        if (!cam_.project(0.5f * (x0 + x1), z0, 0, ax, ay, as) || !cam_.project(0.5f * (x0 + x1), z1, 0, bx, by, bs)) return;
        Sticker* st = add(z1, 1);
        if (st) *st = sticker(kShBox, 0.5f * (ax + bx), 0.5f * (ay + by), 0.5f * (x1 - x0) * 0.5f * (as + bs), maxv(2.0f, 0.5f * (ay - by)), 0xF0323C, 0xF0323C, 0xF0323C, alpha);
    };
    for (int k = 0; k < b.strikeCount; ++k) {
        const sim::Strike& s = b.strikes[k];
        const float left = maxv(0.0f, s.t - step), f = 1.0f - left / maxv(0.01f, s.full);  // 0 when shown, 1 as it lands
        const float pulse = 0.6f + 0.4f * std::sin(time * 14.0f);
        if (s.kind == 0) {  // the circle (2.3 units across for a radius of 1); the stone leaves the thrower's hand or cup on
                            // its release frame (the boss's 5th, the catapult's 4th, at 12 a second) and arcs down onto it
            if (!flat(ids_.warnCircle, s.x, s.z, 2.3f * s.r, 0.9f * pulse)) redZone(s.x - s.r, s.x + s.r, s.z - s.r, s.z + s.r, 0.35f * pulse);
            const float rel = s.byBoss ? 5.0f / 12.0f : 4.0f / 12.0f, gone = f * s.full;
            if (gone < rel) continue;
            const float g = clampv((gone - rel) / maxv(0.05f, s.full - rel), 0.0f, 1.0f);
            if (ids_.shadowGrow >= 0) sprite(ids_.shadowGrow, s.x, s.z + 0.02f, 0, 0, (0.3f + 0.7f * g) * s.r / 1.7f, 0.25f + 0.5f * g);
            const float bx = s.fromX + (s.x - s.fromX) * g, bz = s.fromZ + (s.z - s.fromZ) * g, by = 3.0f + 9.0f * g * (1.0f - g) - 2.5f * g;
            if (ids_.boulder >= 0) {
                sprite(ids_.boulder, bx, bz, by, time, s.byBoss ? 1.5f : 1.0f, 1.0f);
            } else {
                float sx, sy, sc;
                if (cam_.project(bx, bz, by, sx, sy, sc)) {
                    Sticker* st = add(bz, 1);
                    if (st) *st = sticker(kShEllipse, sx, sy, 0.6f * sc, 0.55f * sc, 0x7A6A5A, 0x5A4A3A, 0x5A4A3A);
                }
            }
        } else if (s.kind == 1) {  // the club's half in 2.5-unit tiles, from just behind the squad to well ahead of it
            const float x0 = s.x < 0.0f ? -sim::kRoadHalf : 0.0f;
            if (ids_.warnHalf >= 0) {
                for (float zz = b.z - 2.5f; zz < b.z + 8.0f; zz += 2.5f)
                    for (int col = 0; col < 2; ++col) flat(ids_.warnHalf, x0 + 1.25f + 2.5f * float(col), zz, 2.5f, 0.85f * pulse);
            } else {
                redZone(x0, x0 + sim::kRoadHalf, b.z - 3.0f, b.z + 8.0f, 0.3f * pulse);
            }
        } else if (s.kind == 3) {  // a roller: its lane in red tiles from the squad to the boulder rolling down it
            const float zr = b.z + (s.z - b.z) * left / maxv(0.01f, s.full);
            for (float zz = b.z - 1.5f; zz < zr - 0.5f; zz += 2.5f)
                if (!flat(ids_.warnHalf, s.x, zz, 2.4f, 0.8f * pulse)) redZone(s.x - 1.2f, s.x + 1.2f, zz - 1.2f, zz + 1.2f, 0.3f * pulse);
            if (ids_.boulder >= 0) {
                castShadow(s.x, zr, 2.0f, 1.0f);
                sprite(ids_.boulder, s.x, zr, 0.0f, 0, 2.3f, 1.0f, -time * 7.0f);
            }
        } else {  // the stomp's ridge (2-unit tiles, upright) rolls from the boss down to the squad, leaving its gap
            const float zr = b.z + (s.z - b.z) * left / maxv(0.01f, s.full);
            for (float xx = -4.0f; xx <= 4.01f; xx += 2.0f) {
                if (std::fabs(xx - s.x) < 2.0f) continue;
                if (ids_.waveRidge >= 0) sprite(ids_.waveRidge, xx, zr, 0, time + xx * 0.1f, 1.0f, 1.0f);
                else redZone(xx - 1.0f, xx + 1.0f, zr - 0.4f, zr + 0.4f, 0.55f);
            }
            if (ids_.arrowGround >= 0) flat(ids_.arrowGround, s.x, b.z + 3.0f, 1.6f, 0.6f + 0.4f * pulse);
        }
    }
    // Weather over the road: ice a pale sheen in slices up the road; wind arrows on the road and streaks blowing across.
    // The hazards (no art of their own): mud, the tide, the narrows' gorge and railings, pits, hammers on their gantries.
    const float zMin = cam_.z + cam_.tBottom - 1.0f, zMax = cam_.z + cam_.tTop + 2.0f;
    // A band of the road from xa to xb, z0 to z1 (flat on the road, behind what stands on it), in slices about 6 pixels
    // high so its slanting sides do not step.
    auto band = [&](float xa, float xb, float z0, float z1, u32 c, float a) {
        const float xm = 0.5f * (xa + xb);
        float step = 2.0f;
        for (float zz = maxv(z0, zMin); zz < minv(z1, zMax); zz += step) {
            float ax, ay, as, bx, by, bs;
            if (!cam_.project(xm, zz, 0, ax, ay, as) || !cam_.project(xm, zz + 1.0f, 0, bx, by, bs)) continue;
            step = clampv(6.0f / maxv(0.5f, ay - by), 0.15f, 2.0f);
            const float z2 = minv(zz + step, z1);
            if (!cam_.project(xm, z2, 0, bx, by, bs)) continue;
            Sticker* q = add(z2 + 0.5f, 1);
            if (q) *q = sticker(kShBox, 0.5f * (ax + bx), 0.5f * (ay + by), 0.25f * (xb - xa) * (as + bs), maxv(1.0f, 0.5f * (ay - by)), c, c, c, a);
        }
    };
    // An upright post at x, z, `h` high (a railing's, a warning's, the hammer's gantry).
    auto post = [&](float x, float z, float h, float w, u32 c) {
        float sx, sy, s;
        if (!cam_.project(x, z, 0, sx, sy, s)) return;
        Sticker* q = add(z, 1);
        if (q) *q = sticker(kShBox, sx, sy - 0.5f * h * s, maxv(1.0f, w * s), 0.5f * h * s, c, mixc(c, 0x000000, 0.3f), 0x2A241E);
    };
    // A bar between two points in the air (a railing, the hammer's beam and chain).
    auto bar = [&](float x0, float z0, float y0, float x1, float z1, float y1, float w, u32 c, float key) {
        float ax, ay, as, bx, by, bs;
        if (!cam_.project(x0, z0, y0, ax, ay, as) || !cam_.project(x1, z1, y1, bx, by, bs)) return;
        Sticker* q = add(key, 1);
        if (!q) return;
        *q = sticker(kShCapsule, 0.5f * (ax + bx), 0.5f * (ay + by), 0.5f * std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay)) + w * as,
                     maxv(0.8f, w * 0.5f * (as + bs)), c, c, c);
        q->rot = std::atan2(by - ay, bx - ax);
    };
    // What lies below the road where it is gone: the world's water, clouds, lava or a dark gorge.
    const int wn = def_->world;
    const u32 below = wn == 5 || wn == 1 ? 0x2F6FA8 : wn == 8 ? 0xDCE8F6 : wn == 9 ? 0xE0541C : wn == 3 ? 0x1E3A52 : 0x2A2420;
    const u32 flood = wn == 9 ? 0xF0601E : wn == 8 ? 0xF2F5FF : wn == 7 ? 0x5C6B3A : 0x3D8FD6;
    const float now = b.time + step;
    for (int k = 0; k < def_->stripCount; ++k) {
        const sim::StripDef& st = def_->strips[k];
        if (st.z1 < zMin || st.z0 > zMax) continue;
        if (st.kind == sim::kStripMud) {  // brown over the road, darker puddles
            band(-sim::kRoadHalf, sim::kRoadHalf, st.z0, st.z1, 0x6B4A2E, 0.55f);
            const int n = int((st.z1 - st.z0) / 2.5f);
            for (int i = 0; i < n; ++i) {
                const float px = h01(u32(k * 131 + i * 7)) * 8.0f - 4.0f, pz = st.z0 + (float(i) + 0.5f) * (st.z1 - st.z0) / float(n);
                float sx, sy, s;
                if (pz < zMin || pz > zMax || !cam_.project(px, pz, 0, sx, sy, s)) continue;
                Sticker* q = add(pz + 0.4f, 1);
                if (q) *q = sticker(kShEllipse, sx, sy, (0.6f + 0.5f * h01(u32(k * 17 + i))) * s, 0.22f * s, 0x4A3220, 0x4A3220, 0x4A3220, 0.75f);
            }
            continue;
        }
        if (st.kind == sim::kStripTide) {  // the half that floods: a wet sheen; a red shimmer as it comes; water sliding in
            const float sgn = st.push < 0.0f ? -1.0f : 1.0f, c = sim::tideClock(st, now);
            const float edge = sim::kRoadHalf + 0.6f;
            band(sgn < 0 ? -edge : 0.0f, sgn < 0 ? 0.0f : edge, st.z0, st.z1, flood, 0.24f);
            if (c < 1.0f) {
                band(sgn < 0 ? -edge : 0.0f, sgn < 0 ? 0.0f : edge, st.z0, st.z1, 0xF0323C, 0.12f + 0.12f * (0.5f + 0.5f * std::sin(c * 25.0f)));
            } else if (c < 4.0f) {
                const float f = minv(1.0f, minv((c - 1.0f) / 0.4f, (4.0f - c) / 0.4f)), inner = edge - edge * f;  // the water's edge
                const float xa = sgn < 0 ? -edge : sgn * inner, xb = sgn < 0 ? -inner : edge;
                band(xa, xb, st.z0, st.z1, flood, 0.82f);
                band(sgn < 0 ? -inner - 0.25f : inner, sgn < 0 ? -inner : inner + 0.25f, st.z0, st.z1, kWhiteC, 0.6f + 0.2f * std::sin(now * 9.0f));
            }
            continue;
        }
        if (st.kind == sim::kStripNarrow) {  // the gorge either side of a bridge, its railings
            const float edge = sim::kRoadHalf + 0.7f, nh = sim::kNarrowHalf;
            band(-edge, -nh, st.z0, st.z1, below, 1.0f);
            band(nh, edge, st.z0, st.z1, below, 1.0f);
            for (float zz = st.z0; zz <= st.z1 + 0.01f; zz += 2.0f) {
                if (zz < zMin || zz > zMax) continue;
                for (int sd = -1; sd <= 1; sd += 2) {
                    post(float(sd) * (nh + 0.1f), zz, 1.0f, 0.07f, 0x8A5A32);
                    if (zz + 2.0f <= st.z1 + 0.01f) bar(float(sd) * (nh + 0.1f), zz, 0.95f, float(sd) * (nh + 0.1f), zz + 2.0f, 0.95f, 0.06f, 0x9A6A3E, zz + 1.0f);
                }
            }
            continue;
        }
        if (st.kind == sim::kStripPit) {  // the road gone on one side: what lies below, a crumbling rim, warning posts
            const float w = std::fabs(st.push), sgn = st.push < 0.0f ? -1.0f : 1.0f, edge = sim::kRoadHalf + 0.7f;
            const float inner = sgn * (sim::kRoadHalf - w);
            band(sgn < 0 ? -edge : inner, sgn < 0 ? inner : edge, st.z0, st.z1, below, 1.0f);
            band(sgn < 0 ? inner : inner - 0.3f, sgn < 0 ? inner + 0.3f : inner, st.z0, st.z1, 0x8C7B66, 0.9f);
            for (float zz : {st.z0 - 0.4f, st.z1 + 0.2f})
                if (zz > zMin && zz < zMax) {
                    post(inner - sgn * 0.3f, zz, 1.1f, 0.08f, 0xF0323C);
                    post(sgn * (sim::kRoadHalf - 0.3f), zz, 1.1f, 0.08f, 0xF4F0E8);
                }
            continue;
        }
        if (st.kind == sim::kStripHammer) {  // a wrecking ball on a chain from a gantry over the road, its path in red
            const float z0 = st.z0, top = 7.5f, len = 6.8f;
            band(-sim::kHammerReach - sim::kHammerHead, sim::kHammerReach + sim::kHammerHead, z0 - 0.5f, z0 + 0.5f, 0xF0323C, 0.22f);
            post(-sim::kRoadHalf - 0.4f, z0, top, 0.16f, 0x7A4E2C);
            post(sim::kRoadHalf + 0.4f, z0, top, 0.16f, 0x7A4E2C);
            bar(-sim::kRoadHalf - 0.6f, z0, top, sim::kRoadHalf + 0.6f, z0, top, 0.2f, 0x8A5A32, z0 + 0.05f);
            const float hx = sim::hammerX(st, now), hy = top - std::sqrt(maxv(1.0f, len * len - hx * hx));
            bar(0.0f, z0, top, hx, z0, hy + 0.8f, 0.05f, 0x3A342C, z0 + 0.02f);
            if (ids_.boulder >= 0 && sprite(ids_.boulder, hx, z0, hy - 0.9f, 0, 2.2f, 1.0f, std::atan2(hx, len)).st) castShadow(hx, z0, hy, 1.0f);
            continue;
        }
        if (st.kind == sim::kStripIce) {
            for (float zz = maxv(st.z0, zMin); zz < minv(st.z1, zMax); zz += 2.0f) {
                float ax, ay, as, bx, by, bs;
                const float z2 = minv(zz + 2.0f, st.z1);
                if (!cam_.project(0, zz, 0, ax, ay, as) || !cam_.project(0, z2, 0, bx, by, bs)) continue;
                Sticker* q = add(z2 + 0.5f, 1);
                if (q) *q = sticker(kShBox, 0.5f * (ax + bx), 0.5f * (ay + by), sim::kRoadHalf * 0.5f * (as + bs), maxv(1.0f, 0.5f * (ay - by)), 0xD8F4FF,
                                    0xD8F4FF, 0xD8F4FF, 0.38f + 0.06f * std::sin(time * 2.0f + zz));
            }
        } else if (st.kind == sim::kStripWind) {
            const float dir = st.push < 0.0f ? -1.0f : 1.0f;
            for (float zz = st.z0 + 2.0f; zz < st.z1; zz += 5.0f)
                for (int c = 0; c < 2; ++c) {
                    const Placed pl = sprite(ids_.arrowGround, c ? 2.2f : -2.2f, zz, 0.03f, time, 0.65f, 0.55f, dir * kPi * 0.5f);
                    for (Sticker* q : {pl.st, pl.st2})
                        if (q) q->hh *= maxv(0.12f, std::sin(pl.elev * kPi / 180.0f));
                }
            if (ids_.speedLines >= 0)
                for (int i = 0; i < 4; ++i) {
                    const float zz = st.z0 + (st.z1 - st.z0) * (0.15f + 0.23f * float(i));
                    const float xx = std::fmod(time * 6.0f * dir + float(i) * 2.7f + 50.0f, 12.0f) - 6.0f;
                    sprite(ids_.speedLines, xx, zz, 1.2f, time, 1.4f, 0.5f, dir * kPi * 0.5f);
                }
        }
    }
    // Mortar shells arcing to where they land (1.1 s), a ring on the road there.
    for (int k = 0; k < b.shellCount; ++k) {
        const sim::Shell& sh = b.shells[k];
        const float f = clampv(1.0f - (sh.t - step) / 1.1f, 0.0f, 1.0f);
        flat(ids_.targetRing, sh.x, sh.z, 5.2f, 0.35f + 0.45f * f);
        const float x = sh.x0 + (sh.x - sh.x0) * f, z = sh.z0 + (sh.z - sh.z0) * f, y = 1.0f + 9.0f * f * (1.0f - f);
        if (ids_.shell >= 0) sprite(ids_.shell, x, z, y, 0, 1.2f, 1.0f, (f - 0.5f) * 2.4f);
    }
}

// Mines blinking on the road, cages with their prisoners waving in a green light, archers' arrows arcing at the squad.
void Field::drawHazards(const sim::Battle& b, UiBatch& labels, float step) {
    const float zMin = cam_.z + cam_.tBottom - 1.0f, zMax = cam_.z + cam_.tTop + 2.0f;
    for (int m = 0; m < def_->mineCount; ++m) {
        const sim::MineDef& md = def_->mines[m];
        if (b.mineGone[m] || md.z < zMin || md.z > zMax) continue;
        if (ids_.mine >= 0) {  // its light blinks faster as the squad comes near (3 to 10 times a second)
            const float near = clampv(1.0f - (md.z - b.z) / 30.0f, 0.0f, 1.0f);
            sprite(ids_.mine, md.x, md.z, 0, (time + float(m) * 0.37f) * (1.0f + 2.3f * near), 1.0f, 1.0f);
            continue;
        }
        float sx, sy, s;
        if (!cam_.project(md.x, md.z, 0, sx, sy, s)) continue;
        Sticker* st = add(md.z, 2);
        if (!st) return;
        st[0] = sticker(kShEllipse, sx, sy - 0.12f * s, 0.5f * s, 0.24f * s, 0x2A2A30, 0x18181C, 0x18181C);
        const bool on = std::fmod(time + float(m) * 0.37f, 0.8f) < 0.4f;
        st[1] = sticker(kShEllipse, sx, sy - 0.3f * s, 0.09f * s, 0.07f * s, on ? 0xFF3030 : 0x601010, 0xFF3030, 0xFF3030);
    }
    for (int c = 0; c < def_->cageCount; ++c) {
        const sim::CageDef& cd = def_->cages[c];
        if (b.cageOpen[c] || cd.z < zMin || cd.z > zMax) continue;
        // A green light over it; its floor, two prisoners waving, then the near bars and roof (ART_DIRECTION 17).
        if (ids_.lightColumn >= 0) sprite(ids_.lightColumn, cd.x, cd.z + 0.02f, 0, time, 1.3f, 0.7f, 0, false, 0, 0x6CFF8A);
        if (ids_.cageBack >= 0) sprite(ids_.cageBack, cd.x, cd.z + 0.015f, 0, 0, 1.0f, 1.0f);
        if (ids_.prisoner >= 0)
            for (int k = 0; k < 2; ++k) sprite(ids_.prisoner, cd.x + (k ? 0.35f : -0.35f), cd.z + 0.01f, 0, time + float(c * 2 + k) * 0.21f, 1.0f, 1.0f, 0, k == 1);
        float sx, sy, s;
        if (!cam_.project(cd.x, cd.z, 0, sx, sy, s)) continue;
        if (ids_.cage >= 0) {
            if (sprite(ids_.cage, cd.x, cd.z, 0, time, 1.0f, 1.0f).st) castShadow(cd.x, cd.z, 1.8f, 0.8f);
        } else {
            Sticker* st = add(cd.z, 3);
            if (!st) return;
            st[0] = sticker(kShBox, sx, sy - 0.9f * s, 0.8f * s, 0.9f * s, 0x2E9E4E, 0x1E6E36, 0x1E6E36, 0.45f);
            st[1] = sticker(kShPawn, sx, sy - 0.6f * s, 0.25f * s, 0.42f * s, kBlue, kBlueDark, 0xF2C230);
            st[2] = sticker(kShBox, sx, sy - 1.85f * s, 0.85f * s, 0.08f * s, 0x1E6E36, 0x1E6E36, 0x1E6E36);
        }
        char t[16];
        std::snprintf(t, sizeof t, "+%d", cd.reward);
        const float size = 0.72f * s * tune.tagScale;
        if (size >= 7.0f) label(labels, t, sx, sy - 2.6f * s, size, kWhiteC, 0x1E6E36);
    }
    // Boulders waiting at the top of their lanes.
    for (int k = 0; k < def_->rollerCount; ++k) {
        const sim::RollerDef& rd = def_->rollers[k];
        if (b.rollerGone[k] || rd.z < zMin || rd.z > zMax || ids_.boulder < 0) continue;
        if (sprite(ids_.boulder, rd.x, rd.z, 0.0f, 0, 2.3f, 1.0f, float(k) * 1.3f).st) castShadow(rd.x, rd.z, 2.0f, 1.0f);
    }
    // Fort walls: stone across the road with battlements, cracking as they weaken, their strength in a bar over them.
    for (int w = 0; w < def_->wallCount; ++w) {
        const sim::WallDef& wd = def_->walls[w];
        if (b.wallHp[w] <= 0 || wd.z < zMin || wd.z > zMax) continue;
        float lx, ly, ls, rx, ry, rs;
        if (!cam_.project(-sim::kRoadHalf - 0.4f, wd.z, 0, lx, ly, ls) || !cam_.project(sim::kRoadHalf + 0.4f, wd.z, 0, rx, ry, rs)) continue;
        const float frac = clampv(float(b.wallHp[w]) / float(maxv<i32>(1, wd.hp * sim::kHp)), 0.0f, 1.0f);
        const float hit = b.wallHit[w] < 0.08f ? 1.0f - b.wallHit[w] / 0.08f : 0.0f;
        const float h = 2.4f * ls, cx = 0.5f * (lx + rx), hw = 0.5f * (rx - lx);
        const u32 stone = mixc(mixc(0xC9BFAE, 0x8A8070, 1.0f - frac), kWhiteC, hit * 0.5f), side = mixc(0x8A8070, 0x5A5248, 1.0f - frac);
        const int cracks = frac < 0.34f ? 4 : frac < 0.67f ? 2 : 0;
        Sticker* st = add(wd.z, 1 + 7 + cracks + 2);
        if (!st) return;
        int i = 0;
        st[i++] = sticker(kShBox, cx, ly - 0.5f * h, hw, 0.5f * h, stone, side, 0x3A342C);
        for (int c = 0; c < 7; ++c)  // battlements
            st[i++] = sticker(kShBox, lx + hw * (0.14f + 0.286f * float(c)), ly - h - 0.22f * ls, 0.32f * ls, 0.24f * ls, stone, side, 0x3A342C);
        for (int c = 0; c < cracks; ++c) {  // dark cracks
            st[i] = sticker(kShCapsule, lx + hw * (0.3f + 0.45f * float(c)), ly - h * (0.35f + 0.12f * float(c % 2)), 0.5f * ls, maxv(0.8f, 0.04f * ls), 0x2A241E,
                            0x2A241E, 0x2A241E, 0.85f);
            st[i++].rot = c % 2 ? 0.9f : -0.7f;
        }
        const float bw = 2.6f * ls, bh = maxv(4.0f, 0.2f * ls), by = ly - h - 0.9f * ls;  // its strength
        st[i++] = sticker(kShBox, cx, by, bw * 0.5f + 1.5f, bh * 0.5f + 1.5f, 0x16224F, 0x16224F, 0x16224F);
        st[i++] = sticker(kShBox, cx - bw * 0.5f * (1.0f - frac), by, maxv(0.5f, bw * 0.5f * frac), bh * 0.5f, 0xF0323C, 0xF0323C, 0xF0323C);
        castShadow(0.0f, wd.z, 2.4f, sim::kRoadHalf);
    }
    for (int k = 0; k < b.arrowCount; ++k) {  // from the archer to the squad in 0.8 s, along an arc
        const sim::Arrow& a = b.arrows[k];
        const float f = clampv(1.0f - (a.t - step) / 0.8f, 0.0f, 1.0f), f2 = minv(1.0f, f + 0.05f);
        const float x = a.x0 + (b.x - a.x0) * f, z = a.z0 + (b.z - a.z0) * f, y = 1.4f + 10.0f * f * (1.0f - f);
        const float x2 = a.x0 + (b.x - a.x0) * f2, z2 = a.z0 + (b.z - a.z0) * f2, y2 = 1.4f + 10.0f * f2 * (1.0f - f2);
        float ax, ay, bx, by, s;
        if (!cam_.project(x, z, y, ax, ay, s) || !cam_.project(x2, z2, y2, bx, by, s)) continue;
        const float rot = std::atan2(bx - ax, ay - by);
        if (ids_.arrow >= 0) {
            sprite(ids_.arrow, x, z, y, 0, 1.0f, 1.0f, rot);
        } else {
            Sticker* st = add(z, 1);
            if (!st) return;
            st[0] = sticker(kShCapsule, ax, ay, maxv(3.0f, 0.5f * s), maxv(0.8f, 0.04f * s), 0x3A2A1A, 0x3A2A1A, 0x3A2A1A);
            st[0].rot = rot - kPi * 0.5f;
        }
    }
}

void Field::drawEnemy(const sim::Enemy& e, float z) {
    float sx, sy, s;
    if (!cam_.project(e.x, z, 0, sx, sy, s)) return;
    const float W = float(L_.w), H = float(L_.h);
    if (sx < -60.0f || sx > W + 60.0f || sy < -40.0f || sy > H + 160.0f) return;
    const float flash = e.hit < 0.08f ? 1.0f - e.hit / 0.08f : 0.0f;
    // An archer in range draws its bow in time with its next arrow; out of range it stands at ease.
    const bool aiming = e.kind == sim::kArcher && ids_.archerShoot >= 0 && e.z - squadZ_ > 0.0f && e.z - squadZ_ <= 18.0f;
    if (e.kind == sim::kCatapult) {  // the engine at the road's side: its arm swings over (the stone leaves on frame 4, as
                                     // the boulder is thrown) and stays up until the next stone is loaded
        const float phase = std::fmod(3.2f - e.cool + 4.0f / 12.0f, 3.2f);
        const bool swing = e.z - squadZ_ < 45.0f && e.z - squadZ_ > 6.0f && phase < 1.6f && ids_.catapultFire >= 0;
        const int a = swing ? ids_.catapultFire : ids_.catapult;
        if (a >= 0) {
            if (sprite(a, e.x, z, 0, swing ? phase : 0.0f, 1.0f, 1.0f, 0, e.x < 0.0f, flash * 0.6f).st) castShadow(e.x, z, 2.4f, 1.2f);
            return;
        }
        if (!cam_.project(e.x, z, 0, sx, sy, s)) return;
        Sticker* st = add(z, 2);
        if (!st) return;
        st[0] = sticker(kShBox, sx, sy - 0.6f * s, 1.0f * s, 0.6f * s, mixc(0x7A5030, kWhiteC, flash), 0x5A3820, 0x5A3820);
        st[1] = sticker(kShCapsule, sx, sy - 1.5f * s, 1.2f * s, 0.12f * s, 0x5A3820, 0x5A3820, 0x5A3820);
        st[1].rot = swing ? -0.3f : -1.1f;
        return;
    }
    const bool giant = e.kind == sim::kGiant;
    const int anim = e.kind == sim::kRider    ? ids_.rider
                     : e.kind == sim::kBrute  ? ids_.brute
                     : giant                  ? (ids_.giant >= 0 ? ids_.giant : ids_.brute)
                     : e.kind == sim::kArcher ? (aiming ? ids_.archerShoot : (ids_.archer >= 0 ? ids_.archer : ids_.foot))
                     : e.kind == sim::kShieldman && ids_.shieldman >= 0 ? ids_.shieldman
                     : e.kind == sim::kBomber && ids_.bomber >= 0       ? ids_.bomber
                     : e.kind == sim::kDrummer && ids_.drummer >= 0     ? ids_.drummer
                                                                        : ids_.foot;
    if (giant) {  // its health over its head
        const i32 full = e.wave >= 0 && e.wave < def_->waveCount ? maxv<i32>(1, def_->waves[e.wave].hp * sim::kHp) : 1;
        float hx, hy, hs;
        if (cam_.project(e.x, z, 4.4f, hx, hy, hs)) {
            const float frac = clampv(float(e.hp) / float(full), 0.0f, 1.0f), bw = 2.2f * hs, bh = maxv(4.0f, 0.2f * hs);
            Sticker* st = add(z - 0.02f, 2);
            if (st) {
                st[0] = sticker(kShBox, hx, hy, bw * 0.5f + 1.5f, bh * 0.5f + 1.5f, 0x16224F, 0x16224F, 0x16224F);
                st[1] = sticker(kShBox, hx - bw * 0.5f * (1.0f - frac), hy, maxv(0.5f, bw * 0.5f * frac), bh * 0.5f, 0xF0323C, 0xF0323C, 0xF0323C);
            }
        }
    }
    if (anim >= 0) {
        // Each a little different: its size, a shade darker or lighter, its step.
        const float var = (0.94f + 0.12f * h01(u32(e.seed) * 2654435761u + 3u)) * (giant && ids_.giant < 0 ? 2.0f : 1.0f);
        const float shade = 0.90f + 0.16f * h01(u32(e.seed) * 40503u + 11u);
        // Frozen (FREEZE): tinted ice blue, standing still, ice at its feet.
        const bool frozen = frozenNow_ && e.z - squadZ_ < 45.0f;
        const u32 tint = shade < 1.0f ? mixc(0x000000, 0xFFFFFF, shade) : 0xFFFFFF;
        // An aiming archer's draw is timed to its arrow (it leaves on frame 5 at 12 a second, as the arrow is loosed).
        const float t = aiming ? std::fmod(1.6f - e.cool + 5.0f / 12.0f, 1.6f)
                        : frozen ? float(e.seed & 7) * 0.083f
                                 : time + float(e.seed & 255) * 0.043f;
        // Slowed by the frost blaster: a lighter tint of ice.
        const u32 look = frozen ? mixc(tint, kFrostC, 0.6f) : e.slow > 0.0f ? mixc(tint, kFrostC, 0.45f) : tint;
        const Placed pl = sprite(anim, e.x, z, 0, t, var, 1.0f, 0, false, maxv(flash * 0.85f, shade - 1.0f), look);
        if (frozen && pl.st && ids_.frostOverlay >= 0) sprite(ids_.frostOverlay, e.x, z - 0.05f, 0, 0, var, 1.0f);
        // A shield bearer's shield before it while it holds; a drummer's beat in rings.
        if (pl.st && e.kind == sim::kShieldman && e.shield > 0 && ids_.shieldFront >= 0) sprite(ids_.shieldFront, e.x, z - 0.12f, 0, 0, var, 1.0f);
        if (pl.st && e.kind == sim::kDrummer && ids_.soundRing >= 0) {
            const SpriteAnim& ra = art_->anim(ids_.soundRing);
            const float period = float(ra.frames) / maxv(1.0f, ra.fps);
            sprite(ids_.soundRing, e.x, z - 0.02f, 1.3f, std::fmod(time + float(e.seed & 63) * 0.01f, period), 2.6f, 0.8f);
        }
        if (pl.st) {
            const float h = giant ? 3.8f : e.kind == sim::kBrute ? 1.9f : (e.kind == sim::kRider ? 1.7f : 1.05f);
            const float r = giant ? 1.3f : e.kind == sim::kBrute ? 0.65f : (e.kind == sim::kRider ? 0.5f : 0.27f);
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
        // The world's recolour of the shared boss, and its add-on drawn over it in step; while it attacks, its attack's
        // pose (the club raised and down, a throw, a roar) for a second.
        // The club lands on its 5th frame (12 a second) as the blow does: the slam starts 7/12 s after the half turns red.
        const int pose = bossAct_ == 1 ? ids_.bossSlam : bossAct_ == 2 ? ids_.bossThrow : bossAct_ == 3 ? ids_.bossRoar : -1;
        const float pt = bossAct_ == 1 ? bossActT_ - (1.0f - 5.0f / 12.0f) : bossActT_;
        bool posing = pose >= 0 && pt < 0.9f && (bossAct_ != 1 || bossActT_ < 1.4f);
        float at = posing ? maxv(0.0f, pt) : time;
        int shown = posing ? pose : boss, act = bossAct_;
        if (!posing && bossContact_ && ids_.bossSlam >= 0) {  // on the squad: the club comes down (5th frame) on each blow
            shown = ids_.bossSlam;
            at = std::fmod(bossPhase_ + 5.0f / 12.0f, sim::Battle::kBossBlow);
            posing = true;
            act = 1;
        }
        const Placed pl = sprite(shown, e.x, z, 0, at, breathe, 1.0f, 0, false, flicker, wa.bossTint, wa.bossHue);
        if (pl.st) {
            const int w = clampv<int>(def_->world, 1, 10) - 1;
            const int addon = posing ? ids_.addonPose[w][act - 1] : wa.bossAddon;
            if (addon >= 0 && (posing || wa.bossAddon >= 0)) sprite(addon, e.x, z, 0, at, breathe, 1.0f, 0, false, flicker);
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
    const int m = minv(reduceEffects ? 250 : kMaxShown, minv(int(tune.soldiersShown), int(shownSquad_ + 0.5f)));  // fewer under REDUCE EFFECTS
    const float R = minv(4.4f, 0.55f + 0.3f * std::sqrt(maxv(1.0f, shownSquad_)));
    const bool marching = b.started && !b.won && !b.lost;
    const bool walking = marching && !b.halted;  // the squad stands while a horde comes to it
    const bool powerShot = b.powered(sim::kPowerShot) && marching;
    const float giantK = b.powered(sim::kGiants) ? 1.6f : 1.0f;  // GIANTS: every soldier half again as tall
    // Past what the road's width holds (about 150) the crowd grows back toward the camera, up to 2.4 times as deep behind
    // its front, and packs tighter beyond that (the owner, 2026-11-27: lots of soldiers must look like lots).
    const float back = clampv(float(m) / 150.0f, 1.0f, 2.4f);
    for (int i = 0; i < m; ++i) {
        const float u = std::sqrt((float(i) + 0.5f) / float(m));
        const float a = float(i) * 2.39996323f;
        const float jx = (h01(u32(i) * 7u + 1u) - 0.5f) * 0.12f, jz = (h01(u32(i) * 13u + 5u) - 0.5f) * 0.12f;
        float dx = std::cos(a) * u * R * 0.95f + jx, dz = std::sin(a) * u * R * 0.85f + jz;
        if (dz < 0.0f) dz *= back;
        // At an edge of the road the formation bunches up inside it, deeper instead of wider (the owner, 2026-11-27).
        const float room = dx < 0.0f ? xs + sim::kRoadHalf - 0.3f : sim::kRoadHalf - 0.3f - xs;
        const float squeeze = clampv(room / (R * 0.95f + 0.12f), 0.15f, 1.0f);
        if (squeeze < 1.0f) {
            dx *= squeeze;
            dz *= 1.0f + 0.7f * (1.0f - squeeze);
        }
        const float x = xs + dx, z = zs + dz;
        if (ids_.soldier >= 0) {
            // A new soldier pops in (0 to 1.15 to 1 over 0.2 s); each is a little taller or shorter than the next.
            if (i >= shownBefore_) bornAt_[i] = time;
            const float age = time - bornAt_[i];
            const float pop = age >= 0.2f ? 1.0f : (age < 0.13f ? 1.15f * age / 0.13f : 1.15f - 0.15f * (age - 0.13f) / 0.07f);
            const float var = (0.94f + 0.12f * h01(u32(i) * 31u + 7u)) * giantK;
            const bool cheering = b.won && ids_.cheer >= 0;  // hats in the air
            const float step = walking || cheering ? time + float(i) * 0.137f : 0.0f;
            const float hurt = hurt_[i] > 0.0f ? minv(1.0f, hurt_[i] / 0.12f) : 0.0f;  // hit: a red flash
            const Placed pl = sprite(cheering ? ids_.cheer : ids_.soldier, x, z, 0, step, var * pop, 1.0f, 0, false, 0.25f * hurt,
                                     hurt > 0.0f ? mixc(0xFFFFFF, 0xFF4040, 0.75f * hurt) : 0xFFFFFF);
            if (!pl.st) continue;
            if (i < 250 || (i & 1)) castShadow(x, z, 1.2f * var, 0.25f);  // past 250, every other one casts
            ++figures_;
            // Its gun: rendered with the soldier held out, drawn over it at the same spot, size and step (ART_DIRECTION 16).
            const int gun = cheering ? -1 : ids_.wpn[b.weapon < sim::kWeaponCount ? b.weapon : 0];
            const Placed pg = gun >= 0 ? sprite(gun, x, z, 0, step, var * pop, 1.0f) : pl;
            const int src = gun >= 0 ? gun : ids_.soldier;
            // Its muzzle flash, the weapon's own (the twin's right and left rifles by turns).
            const u8 w = b.weapon < sim::kWeaponCount ? b.weapon : 0;
            const int flashFx = w == sim::kScatter && ids_.muzzleWide >= 0  ? ids_.muzzleWide
                                : w == sim::kLance && ids_.muzzleRing >= 0  ? ids_.muzzleRing
                                : w == sim::kRockets && ids_.puffSmall >= 0 ? ids_.puffSmall
                                                                            : ids_.muzzle;
            float mx, my;
            const bool glow = powerShot && ids_.gunGlow >= 0;  // POWER SHOT: a red glow pulsing at every muzzle
            if ((flash_[i] > 0 || glow) && flashFx >= 0 && pg.st &&
                ((w == sim::kTwin && (i & 1) && art_->point(src, pg.elev, "muzzle2", mx, my)) || art_->point(src, pg.elev, "muzzle", mx, my))) {
                const SpriteFrame& f0 = art_->frameAt(src, pg.elev, 0);
                const float px = pg.sx + (mx - f0.px) * pg.k, py = pg.sy + (my - f0.py) * pg.k;
                if (glow) spriteAt(ids_.gunGlow, z - 0.005f, px, py, pg.s, 0, 1.0f, 0.6f + 0.4f * std::fabs(std::sin(time * 5.0f + float(i))));
                if (flash_[i] > 0) spriteAt(flashFx, z - 0.01f, px, py, pg.s, 0.07f - flash_[i], 1.0f, 1.0f);
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
    // The cannon (the weapon) rolls at the squad's front; the field gun (the power-up) at its side.
    if (ids_.squadCannon >= 0 && !b.won) {
        const float roll = walking ? time : 0.0f;
        if (b.weapon == sim::kCannon) {
            const Placed pl = sprite(ids_.squadCannon, xs, zs + R * 0.55f + 0.5f, 0, roll, 1.0f, 1.0f);
            if (pl.st) castShadow(xs, zs + R * 0.55f + 0.5f, 1.2f, 0.9f);
        }
        if (b.powered(sim::kFieldGun)) {
            const float gx = clampv(xs + R + 0.8f, -sim::kRoadHalf + 0.6f, sim::kRoadHalf - 0.6f);
            const Placed pl = sprite(ids_.squadCannon, gx, zs + 0.3f, 0, roll, 0.85f, 1.0f);
            if (pl.st) castShadow(gx, zs + 0.3f, 1.0f, 0.8f);
        }
    }
    // The squad's banner, carried in its middle with the count on its cloth (ART_DIRECTION 15.3).
    if (ids_.banner >= 0) {
        const float scale = clampv(0.6f + 0.4f * R / 2.0f, 0.6f, 1.0f);  // a small squad carries a small banner
        const Placed pl = sprite(ids_.banner, xs, zs + 0.2f, 0, time, scale, 1.0f);
        float tx, ty;
        if (pl.st && art_->point(ids_.banner, pl.elev, "text", tx, ty)) {
            const SpriteFrame& f0 = art_->frameAt(ids_.banner, pl.elev, 0);
            char t[24];
            formatHost(i64(shownSquad_ + 0.5f), t, sizeof t);
            float size = 0.62f / UiBatch::capHeight() * pl.s * scale * (1.0f + 0.25f * badgePop_);
            const float w = UiBatch::textWidth(t, size);
            if (w > 1.7f * pl.s * scale) size *= 1.7f * pl.s * scale / w;
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
                if (pl.st && (p.anim == ids_.ring || p.anim == ids_.frostRing))  // lies flat on the road
                    for (Sticker* q : {pl.st, pl.st2})
                        if (q) q->hh *= maxv(0.12f, std::sin(pl.elev * kPi / 180.0f));
            } else if (p.kind == 7) {
                sprite(p.anim, p.x, p.z, p.y, age, p.size, minv(1.0f, minv(age * 2.0f, p.life / 0.6f)), p.rot);
            } else if (p.kind == 8) {
                const float t = p.life / p.max;  // 1 .. 0: shrinks to 0.6 and fades
                const WorldArt& wa = art_->world(def_ ? def_->world : 1);
                const bool boss = def_ && p.anim == ids_.bossOf[clampv<int>(def_->world, 1, 10) - 1];  // the world's recolour
                sprite(p.anim, p.x, p.z, 0, time + p.rot, p.size * (0.6f + 0.4f * t), t, 0, false, 0, boss ? wa.bossTint : 0xFFFFFF,
                       boss ? wa.bossHue : 0.0f);
            } else if (p.kind == 5) {
                sprite(p.anim, p.x, p.z, p.y, age, 1.0f, minv(1.0f, p.life / 0.25f), p.rot);
            } else if (p.kind == 0) {
                sprite(p.anim, p.x, p.z, p.y, age, p.size, minv(1.0f, p.life / 0.2f), p.rot);
            } else if (p.kind == 9) {  // a soldier knocked flying, spinning
                sprite(p.anim, p.x, p.z, p.y, 0.3f, p.size, minv(1.0f, p.life / 0.3f), p.rot, false, 0, 0xFFD8D8);
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
    g.cam1[3] = def_->bossHp == 0 && def_->mode != sim::kModeStand ? def_->length : -1.0f;  // a last stand has no finish line
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
    gateStep_ = alpha * sim::kDt;
    frozenNow_ = b.powered(sim::kFreeze);
    squadZ_ = b.z;
    drawGates(b, labels);
    drawBarrels(b, labels);
    drawPowers(b, labels);
    drawHazards(b, labels, alpha * sim::kDt);
    drawStrikes(b, alpha * sim::kDt);
    if (art_ && (art_->world(def_->world).propCount > 0 || art_->world(def_->world).edgeCount > 0)) sceneryArt(zLo, zHi);
    else scenery(zLo, zHi);
    detailScale_ = b.aliveEnemies() > 700 ? 60.0f : (b.aliveEnemies() > 400 ? 42.0f : 30.0f);
    bossContact_ = b.bossContact;
    bossPhase_ = sim::Battle::kBossBlow - b.bossSwing + alpha * sim::kDt;
    const float step = alpha * sim::kDt;
    drawBullets(b, step);
    for (int i = 0; i < b.enemyCount; ++i) {
        const sim::Enemy& e = b.enemies[i];
        if (!e.alive) continue;
        if (e.kind == sim::kBoss) {
            const bool halted = e.z - b.z - sim::enemyStats(sim::kBoss).radius <= b.radius() * 0.6f;
            drawBoss(e, halted ? e.z : e.z - b.enemySpeed(e) * step, labels);
            continue;
        }
        const float z = e.z - b.enemySpeed(e) * step;
        if (z < zLo || z > zHi) continue;
        drawEnemy(e, z);
    }
    drawBolts();
    if (flameT_ > 0.0f && ids_.flame >= 0 && weaponNow_ == sim::kFlame && !b.lost && !b.won) {  // the jet over its cone
        const float R = b.radius(), z0 = viewZ_ + R * 0.45f;
        float ax, ay, as, bx, by, bs;
        if (cam_.project(viewX_, z0, 0.4f, ax, ay, as) && cam_.project(viewX_, z0 + sim::weaponStats(sim::kFlame).range, 0.4f, bx, by, bs))
            streak(ids_.flame, z0 + 4.0f, ax, ay, bx, by, 0.5f * (as + bs) * (2.8f + R * 0.4f), time, minv(1.0f, flameT_ / 0.15f));
    }
    drawParticles();
    // Fog veils what lies further than about 18 units, then 28 (it lies under nearer things in the depth order); night
    // darkens all but the squad's own few units.
    if (def_->sky != sim::kSkyClear) {
        const float W = float(L_.w), H = float(L_.h);
        const bool fog = def_->sky == sim::kSkyFog;
        const u32 c = fog ? 0xD6DEE6 : 0x0A1030;
        const float k0 = fog ? 18.0f : 5.0f, k1 = fog ? 28.0f : 22.0f, a0 = fog ? 0.3f : 0.42f, a1 = fog ? 0.6f : 0.35f;
        for (int i = 0; i < 2; ++i)
            if (Sticker* q = add(viewZ_ + (i ? k1 : k0), 1)) *q = sticker(kShBox, W * 0.5f, H * 0.5f, W * 0.5f + 8.0f, H * 0.5f + 8.0f, c, c, c, i ? a1 : a0);
    }
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
