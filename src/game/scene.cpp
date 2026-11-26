// The scene's logic (TECH_SPEC 6): the page's dressing and bake, the clips that play a move, the figures and confetti.
// Drawing is in scene_draw.cpp.
#include "game/scene.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/log.h"
#include "core/tuning.h"

namespace pm {

namespace {
constexpr float kRoadR = 1.5f;      // map units
constexpr float kRiverR = 4.2f;
inline u32 bit(int i) { return 1u << u32(i); }
inline float hashf(u32 a, u32 b) { return float(u32(hash64((u64(a) << 32) ^ b) >> 40)) / 16777216.0f; }

// A value noise in [0, 1] over map units (groves).
float grove(u32 seed, float x, float y) {
    const float gx = x / 14.0f, gy = y / 14.0f;
    const int ix = int(std::floor(gx)), iy = int(std::floor(gy));
    const float fx = gx - float(ix), fy = gy - float(iy);
    auto h = [&](int a, int b) { return hashf(seed, u32(a * 73856093) ^ u32(b * 19349663)); };
    const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const float a = h(ix, iy) + (h(ix + 1, iy) - h(ix, iy)) * sx;
    const float b = h(ix, iy + 1) + (h(ix + 1, iy + 1) - h(ix, iy + 1)) * sx;
    return a + (b - a) * sy;
}

Sticker capsule(float x0, float y0, float x1, float y1, float r, u32 c, float alpha, float k) {
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    Sticker s = makeSticker(kShCapsule, (x0 + x1) * 0.5f * k, (y0 + y1) * 0.5f * k, (len * 0.5f + r) * k, r * k, c, alpha);
    s.rot = std::atan2(dy, dx);
    return s;
}

Sticker blob(float x, float y, float rx, float ry, float wobble, float phase, float n, u32 c, float k, float lift) {
    Sticker s = makeSticker(kShBlob, x * k, y * k, rx * k * 1.12f, ry * k * 1.12f, c);
    // The quad is padded for the wobble: the shader's radii are the quad's divided by 1.12.
    s.hw = rx * k;
    s.hh = ry * k;
    s.p0 = wobble;
    s.p1 = phase;
    s.p2 = n;
    s.soft = 1.2f;
    s.lift = lift * k;
    return s;
}

}  // namespace

void Scene::init(const Layout& L) {
    L_ = L;
    rng_.reseed(20261126);
}

void Scene::resize(const Layout& L) {
    L_ = L;
    if (def_) {
        layoutPage();
        needBake_ = true;
    }
}

void Scene::layoutPage() {
    fit_ = fitPage(L_, float(def_->pageH));
    const float maxScroll = maxv(0.0f, float(def_->pageH) - fit_.viewH);
    scrollTarget_ = clampv(bannerY_ - fit_.viewH * 0.72f, 0.0f, maxScroll);
    scroll_ = scrollTarget_;
}

void Scene::setLevel(const sim::LevelDef& d, int number, const sim::State& st) {
    defCopy_ = d;
    def_ = &defCopy_;
    levelNumber = number;
    const WorldPalette& wp = worldPalette(d.world);
    islands_ = wp.trees == 4 || wp.trees == 7;
    waves_ = d.kind == sim::kWaves;
    river_ = false;
    for (int i = 0; i < d.siteCount && !waves_; ++i)
        if (d.sites[i].kind == sim::kPost) {
            river_ = true;
            riverY_ = float(d.sites[i].y);
            break;
        }
    riverAmp_ = 2.5f + 2.0f * hashf(d.seed, 11);
    riverPhase_ = 6.283f * hashf(d.seed, 12);
    bannerSite_ = 0;
    bannerX_ = float(d.sites[0].x);
    bannerY_ = float(d.sites[0].y);
    bannerDirX_ = 0;
    bannerDirY_ = -1;
    trailLen_ = 0;
    trailHead_ = 0;
    layoutPage();
    if (waves_) scroll_ = 0.0f;  // the opening view: the castle
    buildRoads();
    buildDecor();
    buildBake();
    needBake_ = true;
    unfold_ = 0;
    figCount_ = 0;
    confettiCount_ = 0;
    snap(st);
}

void Scene::snap(const sim::State& st) {
    disp_ = st;
    target_ = st;
    clipHead_ = clipCount_ = 0;
    clipT_ = 0;
    clipStarted_ = false;
    movesQueued_ = 0;
    shownS_ = float(st.s);
    rivalShown_ = float(st.rival);
    shownMine_ = st.mine;
    winDone_ = loseDone_ = false;
    winT_ = loseT_ = -1;
    waveClip_ = -1;
    waveClipT_ = 0;
    for (float& f : waveFlip_) f = 0;
    keepFold_ = -1;
    marching_ = false;
    hintSite_ = -1;
    for (int i = 0; i < sim::kMaxSites; ++i) {
        flipT_[i] = (st.mine & bit(i)) ? 1.0f : 0.0f;
        burnT_[i] = (st.burnt & bit(i)) ? 1.0f : 0.0f;
        pop_[i] = shake_[i] = whyT_[i] = 0;
    }
    // The banner stands at the last site taken (or Home).
    if (!(st.mine & bit(bannerSite_))) {
        bannerSite_ = 0;
        bannerX_ = float(def_->sites[0].x);
        bannerY_ = float(def_->sites[0].y);
    }
    const int rs = sim::rivalSite(*def_, st);
    if (rs >= 0) {
        rivalX_ = float(def_->sites[rs].x);
        rivalY_ = float(def_->sites[rs].y);
    }
    // Figures: the host gathered at the banner, garrisons around their sites.
    figCount_ = 0;
    spawnGarrisons();
}

// ---- Roads, dressing, the bake ---------------------------------------------------------------------------------------

void Scene::buildRoads() {
    const sim::LevelDef& d = *def_;
    for (int r = 0; r < d.roadCount; ++r) {
        const sim::Site& a = d.sites[d.roads[r].a];
        const sim::Site& b = d.sites[d.roads[r].b];
        const float ax = a.x, ay = a.y, bx = b.x, by = b.y;
        const float dx = bx - ax, dy = by - ay, len = maxv(1.0f, std::sqrt(dx * dx + dy * dy));
        const float bend = waves_ ? 0.0f : (hashf(d.seed, u32(100 + r)) - 0.5f) * 0.36f * minv(len, 40.0f);
        const float cx = (ax + bx) * 0.5f - dy / len * bend, cy = (ay + by) * 0.5f + dx / len * bend;
        Road& R = roads_[r];
        R.n = 24;
        for (int i = 0; i < R.n; ++i) {
            const float t = float(i) / float(R.n - 1);
            R.px[i] = (1 - t) * (1 - t) * ax + 2 * (1 - t) * t * cx + t * t * bx;
            R.py[i] = (1 - t) * (1 - t) * ay + 2 * (1 - t) * t * cy + t * t * by;
        }
    }
}

void Scene::buildDecor() {
    const sim::LevelDef& d = *def_;
    decorCount_ = 0;
    const float H = float(d.pageH);
    for (float gy = 4.0f; gy < H - 3.0f; gy += 6.2f) {
        for (float gx = 4.0f; gx < 97.0f; gx += 6.2f) {
            const u32 cell = u32(int(gx * 10)) * 7919u + u32(int(gy * 10));
            const float x = gx + (hashf(d.seed, cell) - 0.5f) * 5.0f;
            const float y = gy + (hashf(d.seed, cell + 1) - 0.5f) * 5.0f;
            if (grove(d.seed, x, y) < 0.42f) continue;
            // On land.
            bool onLand = false;
            if (islands_) {
                for (int i = 0; i < d.siteCount && !onLand; ++i) {
                    const float ex = (x - d.sites[i].x) / 19.0f, ey = (y - d.sites[i].y + 2.0f) / 15.0f;
                    onLand = ex * ex + ey * ey < 1.0f;
                }
            } else {
                const float ex = std::fabs((x - 50.0f) / 46.5f), ey = std::fabs((y - H * 0.5f) / (H * 0.5f - 3.0f));
                onLand = std::pow(ex, 3.2f) + std::pow(ey, 3.2f) < 1.0f;
            }
            if (!onLand) continue;
            bool clear = true;
            for (int i = 0; i < d.siteCount && clear; ++i) {
                const float sx2 = x - d.sites[i].x, sy2 = y - d.sites[i].y;
                if (sx2 * sx2 + sy2 * sy2 < 10.5f * 10.5f) clear = false;
                // Under a tag (above its site) nothing stands.
                if (std::fabs(sx2) < 10.0f && sy2 < -5.0f && sy2 > -27.0f) clear = false;
            }
            for (int r = 0; r < d.roadCount && clear; ++r)
                for (int i = 0; i < roads_[r].n; i += 2) {
                    const float rx = x - roads_[r].px[i], ry = y - roads_[r].py[i];
                    if (rx * rx + ry * ry < 4.6f * 4.6f) {
                        clear = false;
                        break;
                    }
                }
            if (river_ && clear) {
                const float ry = riverY_ + riverAmp_ * std::sin(x / 15.0f + riverPhase_);
                if (std::fabs(y - ry) < 7.5f) clear = false;
            }
            if (!clear || decorCount_ >= kMaxDecor) continue;
            Decor& o = decor_[decorCount_++];
            o.x = x;
            o.y = y;
            o.s = 0.85f + 0.35f * hashf(d.seed, cell + 2);
            const float t = hashf(d.seed, cell + 3);
            o.type = t < 0.06f ? 2 : (t < 0.24f ? 1 : 0);
            o.seed = u8(cell);
        }
    }
}

void Scene::buildBake() {
    const sim::LevelDef& d = *def_;
    const WorldPalette& wp = worldPalette(d.world);
    const float H = float(d.pageH);
    // Bake at the screen's scale, or smaller when the page would not fit in a texture.
    const float k = fit_.k;
    bakeLandN_ = bakeTerrN_ = bakeRoadsN_ = bakeWaterN_ = 0;
    // The land.
    if (islands_) {
        for (int i = 0; i < d.siteCount && bakeLandN_ < 150; ++i)
            bakeLand_[bakeLandN_++] = blob(d.sites[i].x, d.sites[i].y - 1.5f, 17.0f + 3.0f * hashf(d.seed, u32(200 + i)),
                                           13.0f + 2.5f * hashf(d.seed, u32(300 + i)), 0.16f, 6.283f * hashf(d.seed, u32(400 + i)),
                                           2.0f, wp.land, k, 0.45f);
    } else {
        bakeLand_[bakeLandN_++] = blob(50.0f, H * 0.5f, 48.4f, H * 0.5f - 1.6f, 0.025f, 6.283f * hashf(d.seed, 1), 3.2f, wp.land, k, 0.5f);
    }
    // Terraces around the sites: a lighter one, a lighter still.
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < d.siteCount && bakeTerrN_ < 190; ++i) {
            const float sx = d.sites[i].x, sy = d.sites[i].y;
            const float rx = pass == 0 ? 14.0f + 4.0f * hashf(d.seed, u32(500 + i)) : 8.5f + 2.5f * hashf(d.seed, u32(600 + i));
            const float ry = pass == 0 ? 10.0f + 3.0f * hashf(d.seed, u32(700 + i)) : 5.8f + 1.8f * hashf(d.seed, u32(800 + i));
            bakeTerr_[bakeTerrN_++] = blob(sx, sy - 1.0f, rx, ry, 0.16f, 6.283f * hashf(d.seed, u32(900 + i + pass * 50)), 2.0f,
                                           pass == 0 ? wp.terrace1 : wp.terrace2, k, 0.35f);
        }
    // The river, cut out of the land: a dark upper edge, the water, paper ripples.
    if (river_) {
        // From shore to shore: where the land's superellipse is at the river's height (islands: across the sea).
        float x0 = 2.0f, x1 = 98.0f;
        if (!islands_) {
            const float ey = std::fabs((riverY_ - H * 0.5f) / (H * 0.5f - 1.6f));
            const float ext = 48.4f * std::pow(maxv(0.0f, 1.0f - std::pow(minv(ey, 1.0f), 3.2f)), 1.0f / 3.2f);
            x0 = 50.0f - ext + kRiverR + 0.8f;
            x1 = 50.0f + ext - kRiverR - 0.8f;
        }
        float px = x0, py = riverY_ + riverAmp_ * std::sin(px / 15.0f + riverPhase_);
        for (float x = x0 + 2.0f; x <= x1 && bakeWaterN_ < 380; x += 2.0f) {
            const float y = riverY_ + riverAmp_ * std::sin(x / 15.0f + riverPhase_);
            bakeWater_[bakeWaterN_++] = capsule(px, py - 0.8f, x, y - 0.8f, kRiverR + 0.2f, wp.waterDark, 1.0f, k);
            px = x;
            py = y;
        }
        px = x0;
        py = riverY_ + riverAmp_ * std::sin(px / 15.0f + riverPhase_);
        for (float x = x0 + 2.0f; x <= x1 && bakeWaterN_ < 380; x += 2.0f) {
            const float y = riverY_ + riverAmp_ * std::sin(x / 15.0f + riverPhase_);
            bakeWater_[bakeWaterN_++] = capsule(px, py, x, y, kRiverR, wp.water, 1.0f, k);
            px = x;
            py = y;
        }
        for (int r = 0; r < 26 && bakeWaterN_ < 398; ++r) {
            const float x = x0 + 3.0f + (x1 - x0 - 6.0f) * hashf(d.seed, u32(1000 + r));
            const float y = riverY_ + riverAmp_ * std::sin(x / 15.0f + riverPhase_) + (hashf(d.seed, u32(1100 + r)) - 0.5f) * 5.0f;
            bakeWater_[bakeWaterN_++] = capsule(x - 1.6f, y, x + 1.6f, y, 0.28f, wp.waterLight, 0.9f, k);
        }
    }
    // Roads, stitched, and a sandy patch at each site.
    for (int r = 0; r < d.roadCount; ++r) {
        const Road& R = roads_[r];
        for (int i = 0; i + 1 < R.n && bakeRoadsN_ < 1300; ++i) {
            Sticker s = waves_ ? capsule(R.px[i], R.py[i], R.px[i + 1], R.py[i + 1], 0.75f, wp.road, 0.6f, k)
                               : capsule(R.px[i], R.py[i], R.px[i + 1], R.py[i + 1], kRoadR, wp.road, 1.0f, k);
            s.lift = (waves_ ? 0.15f : 0.3f) * k;
            bakeRoads_[bakeRoadsN_++] = s;
        }
    }
    for (int i = 0; i < d.siteCount && bakeRoadsN_ < 1380; ++i) {
        Sticker s = makeSticker(kShEllipse, d.sites[i].x * k, d.sites[i].y * k, 6.2f * k, 4.0f * k, wp.road);
        s.lift = 0.3f * k;
        bakeRoads_[bakeRoadsN_++] = s;
    }
}

bool Scene::bake(Paper& paper) {
    if (!def_) return false;
    const sim::LevelDef& d = *def_;
    const WorldPalette& wp = worldPalette(d.world);
    const float k = fit_.k;
    const int pw = int(100.0f * k + 0.5f), ph = int(float(d.pageH) * k + 0.5f);
    if (ph > paper.maxTextureSize()) PM_WARN("page %d px tall is over the texture limit %d", ph, paper.maxTextureSize());
    Sticker pageS = makeSticker(kShBox, pw * 0.5f, ph * 0.5f, pw * 0.5f - 1.0f, ph * 0.5f - 1.0f, wp.page);
    pageS.p0 = 3.4f * k / minv(pw * 0.5f, ph * 0.5f);
    // Stitches: a dash every other sample along each road.
    static Sticker stitches[sim::kMaxRoads * 12];
    int sn = 0;
    for (int r = 0; r < d.roadCount && !waves_; ++r) {
        const Road& R = roads_[r];
        for (int i = 2; i + 1 < R.n - 2 && sn < sim::kMaxRoads * 12; i += 2)
            stitches[sn++] = capsule(R.px[i], R.py[i], R.px[i + 1], R.py[i + 1], 0.22f, wp.stitch, 0.95f, k);
    }
    Sticker* terr1 = bakeTerr_;
    const int n1 = bakeTerrN_ / 2;
    const Paper::BakeLayer layers[] = {
        {&pageS, 1, 0.0f},
        {bakeLand_, bakeLandN_, 0.30f},
        {terr1, n1, 0.20f},
        {bakeTerr_ + n1, bakeTerrN_ - n1, 0.18f},
        {bakeWater_, bakeWaterN_, 0.0f},
        {bakeRoads_, bakeRoadsN_, 0.28f},
        {stitches, sn, 0.0f},
    };
    const bool ok = paper.bakePage(pw, ph, layers, int(sizeof layers / sizeof layers[0]));
    needBake_ = !ok;
    return ok;
}

// ---- Clips -----------------------------------------------------------------------------------------------------------

void Scene::cue(u8 c, int step, float pan) {
    if (cueCount_ < 64) cues_[cueCount_++] = {c, i16(step), pan};
}

int Scene::takeCues(CueEvent* out, int max) {
    const int n = minv(cueCount_, max);
    std::memcpy(out, cues_, sizeof(CueEvent) * size_t(n));
    cueCount_ = 0;
    return n;
}

bool Scene::pathTo(int site, float* xs, float* ys, int& n) const {
    // Breadth-first over the host's land from the banner's site to a site next to `site`.
    const sim::LevelDef& d = *def_;
    int prev[sim::kMaxSites], prevRoad[sim::kMaxSites], queue[sim::kMaxSites];
    for (int i = 0; i < sim::kMaxSites; ++i) prev[i] = -2;
    int qh = 0, qt = 0;
    queue[qt++] = bannerSite_;
    prev[bannerSite_] = -1;
    int reached = -1, lastRoad = -1;
    while (qh < qt && reached < 0) {
        const int u = queue[qh++];
        for (int r = 0; r < d.roadCount; ++r) {
            int v = -1;
            if (d.roads[r].a == u) v = d.roads[r].b;
            else if (d.roads[r].b == u) v = d.roads[r].a;
            if (v < 0 || !sim::roadOpen(d, disp_, r)) continue;
            if (v == site) {
                reached = u;
                lastRoad = r;
                break;
            }
            if (prev[v] != -2 || !(shownMine_ & bit(v))) continue;
            prev[v] = u;
            prevRoad[v] = r;
            queue[qt++] = v;
        }
    }
    n = 0;
    if (reached < 0) return false;
    // Chain of roads from the banner to `reached`, then the last road.
    int chain[sim::kMaxSites + 1], nodes[sim::kMaxSites + 2];
    int cn = 0;
    nodes[0] = site;
    chain[cn++] = lastRoad;
    int cur = reached;
    int nn = 1;
    nodes[nn++] = cur;
    while (prev[cur] >= 0) {
        chain[cn++] = prevRoad[cur];
        cur = prev[cur];
        nodes[nn++] = cur;
    }
    // Walk the chain from the banner's end.
    for (int c = cn - 1; c >= 0; --c) {
        const int r = chain[c];
        const int from = nodes[c + 1];
        const Road& R = roads_[r];
        const bool forward = d.roads[r].a == from;
        for (int i = 0; i < R.n && n < 96; ++i) {
            const int j = forward ? i : R.n - 1 - i;
            if (n > 0 && i == 0) continue;
            xs[n] = R.px[j];
            ys[n] = R.py[j];
            ++n;
        }
    }
    return n > 1;
}

void Scene::play(const sim::State& after, const sim::Events& ev) {
    const sim::LevelDef& d = *def_;
    const int move = moveSerial_++;
    moveAfter_[move % 4] = after;
    target_ = after;
    ++movesQueued_;
    auto add = [&](u8 type, int site, float dur, i64 a = 0, i64 b = 0) {
        if (clipCount_ >= kMaxClips) return;
        Clip& c = clips_[(clipHead_ + clipCount_++) % kMaxClips];
        c.type = type;
        c.site = i16(site);
        c.dur = dur;
        c.a = a;
        c.b = b;
        c.move = move;
    };
    bool ended = false;
    for (int i = 0; i < ev.n; ++i) {
        const sim::Event& e = ev.e[i];
        switch (e.type) {
            case sim::kEvTake: {
                add(kClipMarch, e.site, 0.0f);  // its duration is set when it starts (the path's length)
                const u8 kind = d.sites[e.site].kind;
                const float dur = kind == sim::kCamp || kind == sim::kDepot || kind == sim::kKey ? 0.9f
                                  : kind == sim::kKeep                                         ? 1.0f
                                  : kind == sim::kShrine                                       ? 0.8f
                                                                                               : 0.7f;
                add(kClipArrive, e.site, dur, e.a, e.b);
                break;
            }
            case sim::kEvBurn: add(kClipBurn, e.site, 0.35f); break;
            case sim::kEvOpen: add(kClipOpen, e.site, 0.3f); break;
            case sim::kEvTurn: add(kClipTurn, -1, 0.3f, e.a); break;
            case sim::kEvRivalMove: add(kClipRivalMove, e.site, 0.6f, e.a, e.b); break;
            case sim::kEvRivalGates: add(kClipRivalMove, 0, 0.6f, e.a, e.a); break;
            case sim::kEvRivalJoin: add(kClipRivalJoin, -1, 0.9f, e.a, e.b); break;
            case sim::kEvRaidJoin: add(kClipRaid, e.site, 1.25f, e.a, e.b); break;
            case sim::kEvRaidWin: add(kClipRaid, e.site, 0.7f, e.a, -1); break;  // it marches in and breaks through
            case sim::kEvRivalWin: break;
            case sim::kEvWon:
                add(kClipEndMove, -1, 0.0f);
                add(kClipWin, d.keep(), 2.0f, e.a);
                ended = true;
                break;
            case sim::kEvLost:
                add(kClipEndMove, -1, 0.0f);
                add(kClipLose, -1, 1.4f, e.a);
                ended = true;
                break;
            default: break;
        }
    }
    if (!ended) add(kClipEndMove, -1, 0.0f);
}

void Scene::startClip() {
    Clip& c = clips_[clipHead_];
    clipT_ = 0;
    clipStarted_ = true;
    applyClipStart(c);
}

void Scene::applyClipStart(const Clip& c) {
    const sim::LevelDef& d = *def_;
    switch (c.type) {
        case kClipMarch: {
            int n = 0;
            if (!pathTo(c.site, marchX_, marchY_, n)) {
                marchX_[0] = bannerX_;
                marchY_[0] = bannerY_;
                marchX_[1] = float(d.sites[c.site].x);
                marchY_[1] = float(d.sites[c.site].y);
                n = 2;
            }
            // Stop short of the site: the fight is at its edge.
            marchN_ = n;
            marchLen_[0] = 0;
            for (int i = 1; i < n; ++i) {
                const float dx = marchX_[i] - marchX_[i - 1], dy = marchY_[i] - marchY_[i - 1];
                marchLen_[i] = marchLen_[i - 1] + std::sqrt(dx * dx + dy * dy);
            }
            const float total = marchLen_[n - 1];
            clips_[clipHead_].dur = clampv(total / tune.marchSpeed, 0.3f, 0.8f);
            marching_ = true;
            cue(kCueTap, 0, panOf(float(d.sites[c.site].x)));
            break;
        }
        case kClipArrive: {
            const u8 kind = d.sites[c.site].kind;
            const float pan = panOf(float(d.sites[c.site].x));
            if (kind == sim::kCamp || kind == sim::kDepot || kind == sim::kKey || kind == sim::kKeep || kind == sim::kRivalCamp)
                cue(kCueClash, 0, pan);
            else if (kind == sim::kVillage) cue(kCueVillage, 0, pan);
            else if (kind == sim::kShrine) cue(kCueShrine, 0, pan);
            else if (kind == sim::kPost) cue(kCuePost, 0, pan);
            else if (kind == sim::kMire) cue(kCueMire, 0, pan);
            countAcc_ = 0;
            countStep_ = 0;
            pop_[c.site] = 1;
            break;
        }
        case kClipBurn: cue(kCueBurn, 0, panOf(float(d.sites[c.site].x))); break;
        case kClipOpen: cue(kCueOpen); break;
        case kClipTurn: cue(kCueTurn); break;
        case kClipRivalMove: cue(kCueRival, 0, c.site > 0 ? panOf(float(d.sites[c.site].x)) : 0.0f); break;
        case kClipRivalJoin: cue(kCueClash); break;
        case kClipRaid:
            cue(kCueRaid);
            waveClip_ = c.site;
            waveClipT_ = 0;
            countStep_ = 0;
            break;
        case kClipWin:
            winT_ = 0;
            keepFold_ = 0;
            cue(kCueKeepFall);
            break;
        case kClipLose:
            loseT_ = 0;
            cue(kCueLose);
            for (int i = 0; i < figCount_; ++i)
                if (figs_[i].team == 0) {
                    figs_[i].vx = (rng_.f01() - 0.5f) * 30.0f + 14.0f;
                    figs_[i].vy = -6.0f - rng_.f01() * 12.0f;
                }
            break;
        default: break;
    }
}

void Scene::applyClipEnd(const Clip& c) {
    const sim::LevelDef& d = *def_;
    switch (c.type) {
        case kClipMarch:
            bannerSite_ = c.site;
            marching_ = false;
            break;
        case kClipArrive:
            shownS_ = float(c.b);
            shownMine_ |= bit(c.site);
            disp_.mine |= bit(c.site);
            flipT_[c.site] = 1;
            disp_.s = c.b;
            spawnGarrisons();
            break;
        case kClipBurn:
            burnT_[c.site] = 1;
            disp_.burnt |= bit(c.site);
            spawnGarrisons();
            break;
        case kClipTurn: disp_.turn = u8(c.a); spawnGarrisons(); break;
        case kClipRivalMove:
            rivalShown_ = float(c.b);
            if (c.site > 0) disp_.razed |= bit(c.site);
            disp_.rival = c.b;
            spawnGarrisons();
            break;
        case kClipRivalJoin:
            shownS_ = float(c.b);
            disp_.s = c.b;
            disp_.rival = 0;
            disp_.flags &= u8(~sim::kRivalAlive);
            spawnGarrisons();
            break;
        case kClipRaid:
            waveClip_ = -1;
            if (c.b >= 0) {
                shownS_ = float(c.b);
                disp_.s = c.b;
                disp_.raidNext = u8(c.site + 1);
                waveFlip_[c.site] = 1;
                spawnGarrisons();
            }
            break;
        case kClipEndMove:
            disp_ = moveAfter_[c.move % 4];
            shownS_ = float(disp_.s);
            rivalShown_ = float(disp_.rival);
            shownMine_ = disp_.mine;
            if (movesQueued_ > 0) --movesQueued_;
            spawnGarrisons();
            break;
        case kClipWin: winDone_ = true; break;
        case kClipLose: loseDone_ = true; break;
        default: break;
    }
    (void)d;
}

void Scene::finishClip() {
    applyClipEnd(clips_[clipHead_]);
    clipHead_ = (clipHead_ + 1) % kMaxClips;
    --clipCount_;
    clipStarted_ = false;
}

void Scene::update(float dt, bool fast) {
    time_ += dt;
    unfold_ += dt;
    hintTime_ += dt;
    const sim::LevelDef* dp = def_;
    if (!dp) return;
    const sim::LevelDef& d = *dp;
    for (int i = 0; i < sim::kMaxSites; ++i) {
        pop_[i] = maxv(0.0f, pop_[i] - dt * 4.0f);
        shake_[i] = maxv(0.0f, shake_[i] - dt);
        whyT_[i] = maxv(0.0f, whyT_[i] - dt);
    }
    // Clips, one after another; the clock runs faster while a second move waits (GAME_DESIGN 4).
    float cdt = dt * ((fast || movesQueued_ > 1) ? 2.5f : 1.0f);
    for (int guard = 0; guard < 16 && clipCount_ > 0 && cdt > 0; ++guard) {
        if (!clipStarted_) startClip();
        Clip& c = clips_[clipHead_];
        const float before = clipT_;
        clipT_ += cdt;
        const float t = c.dur > 0 ? clampv(clipT_ / c.dur, 0.0f, 1.0f) : 1.0f;
        switch (c.type) {
            case kClipMarch: {
                const float total = marchLen_[marchN_ - 1];
                const float e = t < 1.0f ? t * (2.0f - t) * 0.5f + t * 0.5f : 1.0f;
                // Stop 3.2 units short of the site's centre.
                const float want = maxv(0.0f, total - 3.2f) * e;
                int i = 1;
                while (i < marchN_ - 1 && marchLen_[i] < want) ++i;
                const float seg = maxv(1e-4f, marchLen_[i] - marchLen_[i - 1]);
                const float f = clampv((want - marchLen_[i - 1]) / seg, 0.0f, 1.0f);
                const float nx = marchX_[i - 1] + (marchX_[i] - marchX_[i - 1]) * f;
                const float ny = marchY_[i - 1] + (marchY_[i] - marchY_[i - 1]) * f;
                const float mx = nx - bannerX_, my = ny - bannerY_;
                const float ml = std::sqrt(mx * mx + my * my);
                if (ml > 1e-3f) {
                    bannerDirX_ = mx / ml;
                    bannerDirY_ = my / ml;
                }
                bannerX_ = nx;
                bannerY_ = ny;
                if (int(before / 0.14f) != int(clipT_ / 0.14f)) cue(kCueStep, 0, panOf(bannerX_));
                break;
            }
            case kClipArrive: {
                const u8 kind = d.sites[c.site].kind;
                const bool fight = kind == sim::kCamp || kind == sim::kDepot || kind == sim::kKey || kind == sim::kKeep;
                // A fight: the crowds meet (0-0.3 of it), then the garrison flips; the count runs over the last 0.55.
                const float flip0 = fight ? 0.3f : 0.1f;
                flipT_[c.site] = clampv((t - flip0) / 0.4f, 0.0f, 1.0f);
                const float cs = fight ? 0.4f : 0.2f;
                const float ct = clampv((t - cs) / (1.0f - cs), 0.0f, 1.0f);
                const float a = float(c.a), b = float(c.b);
                const float ease = 1.0f - (1.0f - ct) * (1.0f - ct);
                shownS_ = a + (b - a) * ease;
                // Count ticks: at most 24, each a step up (or down) the ladder.
                if (ct > 0.0f && ct < 1.0f) {
                    const int ticks = clampv(int(std::fabs(b - a)), 1, 24);
                    const int now = int(ct * float(ticks));
                    while (countStep_ < now) {
                        ++countStep_;
                        cue(b >= a ? kCueCount : kCueCountDown, countStep_, panOf(float(d.sites[c.site].x)));
                    }
                }
                if (fight && before < c.dur * 0.3f && clipT_ >= c.dur * 0.3f) cue(kCueFlip, 0, panOf(float(d.sites[c.site].x)));
                if (kind == sim::kShrine && before < c.dur * 0.25f && clipT_ >= c.dur * 0.25f)
                    confettiBurst(float(d.sites[c.site].x), float(d.sites[c.site].y) - 6.0f, 40, false, 1.2f);
                if (fight && before < c.dur * 0.45f && clipT_ >= c.dur * 0.45f)
                    confettiBurst(float(d.sites[c.site].x), float(d.sites[c.site].y) - 2.0f, 14, false, 0.7f);
                break;
            }
            case kClipBurn: burnT_[c.site] = t; break;
            case kClipRivalMove: {
                const float tx = c.site > 0 ? float(d.sites[c.site].x) : float(d.sites[0].x) + 10.0f;
                const float ty = c.site > 0 ? float(d.sites[c.site].y) : float(d.sites[0].y) - 8.0f;
                rivalX_ += (tx - rivalX_) * minv(1.0f, cdt * 7.0f);
                rivalY_ += (ty - rivalY_) * minv(1.0f, cdt * 7.0f);
                rivalShown_ = float(c.a) + float(c.b - c.a) * clampv((t - 0.5f) * 2.0f, 0.0f, 1.0f);
                break;
            }
            case kClipRivalJoin: {
                const float ct = clampv((t - 0.35f) / 0.65f, 0.0f, 1.0f);
                shownS_ = float(c.a) + float(c.b - c.a) * (1.0f - (1.0f - ct) * (1.0f - ct));
                break;
            }
            case kClipRaid: {
                // The wave marches down to the army (0-0.45), the crowds clash, the beaten wave flips (0.45-0.75) and joins
                // with the count (0.5-1). A wave too strong only marches in: the loss follows.
                waveClipT_ = c.b < 0 ? t * 0.45f : t;
                const float clashAt = 0.45f * c.dur;
                if (before < clashAt && clipT_ >= clashAt) {
                    cue(kCueClash, 0, panOf(bannerX_));
                    confettiBurst(bannerX_, bannerY_ - 3.0f, 22, false, 0.9f);
                }
                if (c.b >= 0) {
                    waveFlip_[c.site] = clampv((t - 0.45f) / 0.3f, 0.0f, 1.0f);
                    if (before < c.dur * 0.5f && clipT_ >= c.dur * 0.5f) cue(kCueFlip, 0, panOf(bannerX_));
                    const float ct = clampv((t - 0.5f) / 0.5f, 0.0f, 1.0f);
                    const float a = float(c.a), b = float(c.b);
                    shownS_ = a + (b - a) * (1.0f - (1.0f - ct) * (1.0f - ct));
                    if (ct > 0.0f && ct < 1.0f) {
                        const int ticks = clampv(int(std::fabs(b - a)), 1, 24);
                        const int now = int(ct * float(ticks));
                        while (countStep_ < now) {
                            ++countStep_;
                            cue(kCueCount, countStep_, panOf(bannerX_));
                        }
                    }
                }
                break;
            }
            case kClipWin: {
                keepFold_ = clampv(t * 3.0f, 0.0f, 1.0f);
                winT_ = clipT_;
                if (before < 0.35f && clipT_ >= 0.35f) celebrate(false);
                break;
            }
            case kClipLose: loseT_ = clipT_; break;
            default: break;
        }
        if (clipT_ >= c.dur) {
            cdt = clipT_ - c.dur;
            finishClip();
        } else {
            cdt = 0;
        }
    }
    // Scroll a tall page so the host and the rows ahead of it are in view; a new battle opens on the castle and pans down to
    // the army.
    const float maxScroll = maxv(0.0f, float(d.pageH) - fit_.viewH);
    scrollTarget_ = clampv(bannerY_ - fit_.viewH * 0.72f, 0.0f, maxScroll);
    if (waves_ && unfold_ < 1.1f && !reduceEffects) scrollTarget_ = 0.0f;
    scroll_ += (scrollTarget_ - scroll_) * minv(1.0f, dt * 4.0f);
    hostTargets(dt);
    stepFigures(dt);
    stepConfetti(dt);
}

// ---- Figures ---------------------------------------------------------------------------------------------------------

namespace {
// Pawns, riders, giants for a number (GAME_DESIGN 9.3).
void counts(i64 v, int maxPawns, int maxRiders, int maxGiants, int& pawns, int& riders, int& giants) {
    giants = int(minv<i64>(v / 10000, maxGiants));
    riders = int(minv<i64>(v / 100, maxRiders));
    pawns = int(clampv<i64>(v, 1, maxPawns));
}
}  // namespace

void Scene::garrisonOf(int site, int count, u8 team) {
    (void)count;
    const sim::LevelDef& d = *def_;
    const i64 v = team == 2 ? i64(rivalShown_) : sim::number(d, disp_, site);
    int pawns, riders, giants;
    counts(v, 20, 12, 5, pawns, riders, giants);
    const int total = pawns + riders + giants;
    const float cx = team == 2 ? rivalX_ : float(d.sites[site].x), cy = (team == 2 ? rivalY_ : float(d.sites[site].y)) + 2.2f;
    int k = 0;
    for (int ring = 0; k < total && ring < 6; ++ring) {
        const int slots = 8 + 5 * ring;
        const float rx = 6.0f + 2.3f * float(ring), ry = 2.6f + 1.0f * float(ring);
        for (int s = 0; s < slots && k < total; ++s, ++k) {
            if (figCount_ >= kMaxFigures) return;
            Figure& f = figs_[figCount_++];
            f = Figure();
            const float a = 6.283f * (float(s) + 0.5f * float(ring)) / float(slots) + 0.4f;
            f.x = f.tx = cx + rx * std::cos(a);
            f.y = f.ty = cy + ry * std::sin(a);
            f.kind = u8(k < giants ? 2 : (k < giants + riders ? 1 : 0));
            f.team = team;
            f.site = i16(site);
            f.hop = float(k) * 0.7f;
            f.flip = 1;
        }
    }
}

void Scene::spawnGarrisons() {
    const sim::LevelDef& d = *def_;
    // Keep the host and the joiners; garrisons are rebuilt from what is shown.
    int o = 0;
    for (int i = 0; i < figCount_; ++i)
        if (figs_[i].team == 0 || figs_[i].team == 3) figs_[o++] = figs_[i];
    figCount_ = o;
    // The host: as many figures as its number asks for (GAME_DESIGN 9.3), plus the banner's bearer.
    int pawns, riders, giants;
    counts(i64(shownS_ + 0.5f), 40, 30, 12, pawns, riders, giants);
    pawns = maxv(pawns, 3);
    const int want = 1 + pawns + riders + giants;
    int have = 0;
    for (int i = 0; i < figCount_; ++i)
        if (figs_[i].team == 0) ++have;
    // Remove extras (from the back), add new ones at the banner.
    for (int i = figCount_ - 1; i >= 0 && have > want; --i)
        if (figs_[i].team == 0 && !figs_[i].bearer) {
            figs_[i] = figs_[--figCount_];
            --have;
        }
    while (have < want && figCount_ < kMaxFigures) {
        Figure& f = figs_[figCount_++];
        f = Figure();
        f.team = 0;
        f.x = f.tx = bannerX_ + (rng_.f01() - 0.5f) * 3.0f;
        f.y = f.ty = bannerY_ + rng_.f01() * 2.0f;
        f.bearer = have == 0 ? 1 : 0;
        f.hop = rng_.f01() * 6.0f;
        ++have;
    }
    // Their kinds, in order: the bearer, giants, riders, pawns.
    int k = 0;
    for (int i = 0; i < figCount_; ++i) {
        Figure& f = figs_[i];
        if (f.team != 0 || f.bearer) continue;
        f.kind = u8(k < giants ? 2 : (k < giants + riders ? 1 : 0));
        ++k;
    }
    for (int i = 0; i < d.siteCount; ++i) {
        const u8 kind = d.sites[i].kind;
        if (kind != sim::kCamp && kind != sim::kDepot && kind != sim::kKey && kind != sim::kKeep) continue;
        if (!sim::isFree(disp_, i)) continue;
        garrisonOf(i, 0, 1);
    }
    if (disp_.rivalAlive() && rivalShown_ > 0) garrisonOf(sim::rivalSite(d, disp_) >= 0 ? sim::rivalSite(d, disp_) : 0, 0, 2);
    // The waves due next (GAME_DESIGN 6): their crowds wait above the row the army chooses from.
    if (disp_.raidNext < d.raidCount) {
        const int turn = d.raids[disp_.raidNext].turn;
        for (int r = disp_.raidNext; r < d.raidCount && d.raids[r].turn == turn; ++r) spawnWave(r);
    }
}

void Scene::waveSpot(int raid, float& x, float& y) const {
    const sim::LevelDef& d = *def_;
    int twins = 0, index = 0;
    for (int r = 0; r < d.raidCount; ++r)
        if (d.raids[r].turn == d.raids[raid].turn) {
            if (r < raid) ++index;
            ++twins;
        }
    x = twins > 1 ? (index == 0 ? 33.0f : 67.0f) : 50.0f;
    if (waves_) {
        // Between the row the army chooses from and the next one up (rows are 40 units apart from pageH - 50).
        const int row = maxv(0, int(d.raids[raid].turn) - 1);
        y = float(d.pageH) - 50.0f - 40.0f * float(row) - 25.0f;
    } else {
        const sim::Site& e = d.sites[d.raids[raid].entry];
        x = e.x < 50 ? 8.0f : 92.0f;
        y = float(e.y);
    }
}

void Scene::waveCenter(int raid, float& x, float& y) const {
    waveSpot(raid, x, y);
    if (raid == waveClip_) {
        const float t = clampv(waveClipT_ / 0.45f, 0.0f, 1.0f);
        const float e = t * t * (3.0f - 2.0f * t);
        x += (bannerX_ - x) * e;
        y += (bannerY_ - 3.0f - y) * e;
    }
}

void Scene::spawnWave(int raid) {
    const i64 v = def_->raids[raid].strength;
    int pawns = int(clampv<i64>(v, 3, 30)), riders = int(minv<i64>(v / 100, 16)), giants = int(minv<i64>(v / 10000, 6));
    const int total = pawns + riders + giants;
    float cx, cy;
    waveSpot(raid, cx, cy);
    // A block, giants at the back, riders in front of them, pawns in front (down the page).
    const int cols = minv(8, maxv(4, (total + 2) / 3));
    for (int k = 0; k < total && figCount_ < kMaxFigures; ++k) {
        Figure& f = figs_[figCount_++];
        f = Figure();
        f.team = 4;
        f.site = i16(raid);
        f.kind = u8(k < giants ? 2 : (k < giants + riders ? 1 : 0));
        const int row = k / cols, col = k % cols;
        const int inRow = minv(cols, total - row * cols);
        f.ox = (float(col) - float(inRow - 1) * 0.5f) * (f.kind == 2 ? 5.0f : 2.6f);
        f.oy = float(row) * 2.4f - 4.0f;
        f.x = f.tx = cx + f.ox;
        f.y = f.ty = cy + f.oy;
        f.hop = float(k) * 0.9f;
        f.flip = 1;
    }
}

void Scene::hostTargets(float dt) {
    (void)dt;
    // The banner's trail: a point every 0.3 units of its path.
    const int last = (trailHead_ + 255) % 256;
    if (trailLen_ == 0 || std::hypot(bannerX_ - trailX_[last], bannerY_ - trailY_[last]) > 0.3f) {
        trailX_[trailHead_] = bannerX_;
        trailY_[trailHead_] = bannerY_;
        trailHead_ = (trailHead_ + 1) % 256;
        trailLen_ = minv(trailLen_ + 1, 256);
    }
    int k = 0;
    for (int i = 0; i < figCount_; ++i) {
        Figure& f = figs_[i];
        if (f.team != 0) continue;
        if (f.bearer) {
            f.tx = bannerX_;
            f.ty = bannerY_;
            continue;
        }
        if (marching_ && trailLen_ > 2) {
            // A column of three abreast behind the banner, along its trail.
            const int row = k / 3 + 1, col = k % 3 - 1;
            const float back = float(row) * (f.kind == 0 ? 1.15f : 1.6f) / 0.3f;
            int idx = int(back);
            idx = minv(idx, trailLen_ - 1);
            const int a = (trailHead_ - 1 - idx + 512) % 256, b = (trailHead_ - idx + 512) % 256;
            float dx = trailX_[b] - trailX_[a], dy = trailY_[b] - trailY_[a];
            const float dl = std::sqrt(dx * dx + dy * dy);
            if (dl > 1e-4f) {
                dx /= dl;
                dy /= dl;
            } else {
                dx = bannerDirX_;
                dy = bannerDirY_;
            }
            f.tx = trailX_[a] - dy * float(col) * 1.5f;
            f.ty = trailY_[a] + dx * float(col) * 1.5f;
        } else {
            // Gathered round the banner, a little behind it, in squashed rings.
            int ring = 0, base = 0, slots = 6;
            while (k >= base + slots) {
                base += slots;
                ++ring;
                slots = 6 + 5 * ring;
            }
            const int s = k - base;
            const float a = 6.283f * float(s) / float(slots) + 0.5f * float(ring);
            const float rx = 2.6f + 1.9f * float(ring), ry = 1.4f + 0.95f * float(ring);
            f.tx = bannerX_ - bannerDirX_ * (1.2f + 0.6f * float(ring)) + rx * std::cos(a);
            f.ty = bannerY_ - bannerDirY_ * (1.2f + 0.6f * float(ring)) + ry * std::sin(a) + 0.6f;
        }
        ++k;
    }
}

void Scene::stepFigures(float dt) {
    const sim::LevelDef& d = *def_;
    for (int i = 0; i < figCount_; ++i) {
        Figure& f = figs_[i];
        if (loseT_ >= 0 && f.team == 0) {  // the host scatters on the wind
            f.x += f.vx * dt;
            f.y += f.vy * dt;
            f.vy += 18.0f * dt;
            f.alpha = maxv(0.0f, f.alpha - dt * 0.9f);
            continue;
        }
        if (f.team == 1 && f.site >= 0) {
            // A garrison stirs toward the banner while its camp is being fought; flips with the camp.
            const float fl = flipT_[f.site];
            f.flip = fl > 0 && fl < 1 ? std::fabs(1.0f - 2.0f * fl) : 1.0f;
            if (fl >= 0.5f && !(disp_.mine & bit(f.site))) {
                f.team = 3;  // it turned: now it runs to the host
                f.site = -1;
            }
        }
        if (f.team == 4 && f.site >= 0) {
            float cx, cy;
            waveCenter(f.site, cx, cy);
            f.tx = cx + f.ox;
            f.ty = cy + f.oy;
            const float fl = waveFlip_[f.site];
            f.flip = fl > 0 && fl < 1 ? std::fabs(1.0f - 2.0f * fl) : 1.0f;
            if (fl >= 0.5f) {
                f.team = 3;  // beaten: it turns and runs to the host
                f.site = -1;
            }
        }
        if (f.team == 3) {
            f.tx = bannerX_;
            f.ty = bannerY_ + 0.6f;
            const float dx = f.tx - f.x, dy = f.ty - f.y;
            if (dx * dx + dy * dy < 2.5f * 2.5f) f.alpha -= dt * 4.0f;
            if (f.alpha <= 0) {
                figs_[i] = figs_[--figCount_];
                --i;
                continue;
            }
        }
        const float dx = f.tx - f.x, dy = f.ty - f.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        const float maxStep = (f.team == 3 ? 46.0f : 34.0f) * dt;
        const float want = dist * minv(1.0f, dt * 9.0f);
        const float step = minv(want, maxStep);
        if (dist > 1e-4f) {
            f.x += dx / dist * step;
            f.y += dy / dist * step;
        }
        if (dist > 0.2f) f.hop += dt * 16.0f;
        else f.hop = std::fmod(f.hop, 3.14159f);
    }
    (void)d;
}

// ---- Confetti --------------------------------------------------------------------------------------------------------

void Scene::confettiBurst(float x, float y, int n, bool gold, float speed) {
    if (reduceEffects) n = maxv(1, n / 4);
    const sim::LevelDef& d = *def_;
    const WorldPalette& wp = worldPalette(d.world);
    const u32 colors[5] = {col::kScarlet, col::kGold, col::kIvory, gold ? col::kGoldDark : col::kVioletLight, wp.tree2Light};
    for (int i = 0; i < n && confettiCount_ < kMaxConfetti; ++i) {
        Confetti& c = conf_[confettiCount_++];
        const float a = rng_.f01() * 6.283f, v = (8.0f + rng_.f01() * 22.0f) * speed;
        c.x = x;
        c.y = y;
        c.vx = std::cos(a) * v;
        c.vy = std::sin(a) * v * 0.7f - 14.0f * speed;
        c.rot = rng_.f01() * 6.283f;
        c.spin = (rng_.f01() - 0.5f) * 16.0f;
        c.life = 1.2f + rng_.f01() * 0.8f;
        c.age = 0;
        c.c0 = gold ? (rng_.f01() < 0.6f ? col::kGold : col::kIvory) : colors[rng_.next() % 5];
        c.c1 = c.c0 == col::kGold ? col::kGoldDark : col::kIvoryShade;
    }
}

void Scene::celebrate(bool perfect) {
    const float H = float(def_->pageH);
    // Two cannons from the page's lower corners, and a rain of gold for a perfect win.
    const float y0 = minv(H, scroll_ + fit_.viewH) - 4.0f;
    for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < 70 && confettiCount_ < kMaxConfetti; ++i) {
            Confetti& c = conf_[confettiCount_++];
            const float a = side == 0 ? -1.2f + rng_.f01() * 0.5f : -1.94f - rng_.f01() * 0.5f;
            const float v = 50.0f + rng_.f01() * 45.0f;
            c.x = side == 0 ? 6.0f : 94.0f;
            c.y = y0;
            c.vx = std::cos(a) * v;
            c.vy = std::sin(a) * v;
            c.rot = rng_.f01() * 6.283f;
            c.spin = (rng_.f01() - 0.5f) * 18.0f;
            c.life = 2.0f + rng_.f01();
            c.age = 0;
            const u32 cs[4] = {col::kScarlet, col::kGold, col::kIvory, 0x46BE74};
            c.c0 = cs[rng_.next() % 4];
            c.c1 = col::kIvoryShade;
        }
    }
    if (perfect)
        for (int i = 0; i < 120 && confettiCount_ < kMaxConfetti; ++i) {
            Confetti& c = conf_[confettiCount_++];
            c.x = rng_.f01() * 100.0f;
            c.y = scroll_ - 4.0f - rng_.f01() * 30.0f;
            c.vx = (rng_.f01() - 0.5f) * 8.0f;
            c.vy = 10.0f + rng_.f01() * 10.0f;
            c.rot = rng_.f01() * 6.283f;
            c.spin = (rng_.f01() - 0.5f) * 12.0f;
            c.life = 3.0f;
            c.age = 0;
            c.c0 = col::kGold;
            c.c1 = col::kGoldDark;
        }
    if (reduceEffects) confettiCount_ = minv(confettiCount_, kMaxConfetti / 4);
}

void Scene::stepConfetti(float dt) {
    int o = 0;
    for (int i = 0; i < confettiCount_; ++i) {
        Confetti c = conf_[i];
        c.age += dt;
        if (c.age >= c.life) continue;
        c.vy += 42.0f * dt;
        c.vx *= 1.0f - minv(1.0f, dt * 1.6f);
        c.vy *= 1.0f - minv(1.0f, dt * 1.2f);
        c.x += c.vx * dt;
        c.y += c.vy * dt;
        c.rot += c.spin * dt;
        conf_[o++] = c;
    }
    confettiCount_ = o;
}

void Scene::refuse(int site, const char* why) {
    if (site < 0 || site >= sim::kMaxSites) return;
    shake_[site] = 0.24f;
    std::snprintf(why_[site], sizeof why_[site], "%s", why);
    whyT_[site] = 1.2f;
    cue(kCueRefuse, 0, panOf(float(def_->sites[site].x)));
}

void Scene::siteScreen(int site, float& x, float& y) const {
    x = sx(float(def_->sites[site].x));
    y = sy(float(def_->sites[site].y) - 2.0f);
}

int Scene::hit(float x, float y) const {
    if (!def_) return -1;
    const sim::LevelDef& d = *def_;
    int best = -1;
    float bestD = 1e9f;
    const float r = maxv(L_.px(36), 7.5f * fit_.k);
    for (int i = 1; i < d.siteCount; ++i) {
        if (!sim::isFree(target_, i)) continue;
        const float cx = sx(float(d.sites[i].x)), cy = sy(float(d.sites[i].y) - 3.0f);
        float dx = x - cx, dy = y - cy;
        float dd = dx * dx + dy * dy;
        // The tag above the site counts as the site.
        const float ty = sy(float(d.sites[i].y) - 16.0f);
        const float tdx = x - cx, tdy = y - ty;
        if (std::fabs(tdx) < L_.px(40) && std::fabs(tdy) < L_.px(26)) dd = minv(dd, tdx * tdx * 0.25f + tdy * tdy * 0.25f);
        if (dd < r * r && dd < bestD) {
            bestD = dd;
            best = i;
        }
    }
    if (target_.rivalAlive()) {
        const float cx = sx(rivalX_), cy = sy(rivalY_ - 2.0f);
        const float dx = x - cx, dy = y - cy;
        if (dx * dx + dy * dy < r * r && dx * dx + dy * dy < bestD) best = sim::kRivalMove;
    }
    return best;
}

}  // namespace pm
