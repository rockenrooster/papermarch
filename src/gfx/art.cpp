#include "gfx/art.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/log.h"

namespace pm {

namespace {

constexpr GLenum kAnisoMax = 0x84FF;  // GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
constexpr GLenum kAniso = 0x84FE;     // GL_TEXTURE_MAX_ANISOTROPY_EXT

bool readText(const char* dir, const char* name, std::vector<char>& out) {
    char path[400];
    std::snprintf(path, sizeof path, "%s/%s", dir, name);
    size_t size = 0;
    void* p = SDL_LoadFile(path, &size);
    if (!p) return false;
    out.assign(static_cast<char*>(p), static_cast<char*>(p) + size);
    out.push_back('\0');
    SDL_free(p);
    return true;
}

// A PNG as RGBA bytes, rows top first; colours multiplied by alpha (the renderer blends premultiplied).
bool readPng(const char* dir, const char* name, std::vector<u8>& rgba, int& w, int& h, bool premultiply) {
    char path[400];
    std::snprintf(path, sizeof path, "%s/%s", dir, name);
    size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (!data) {
        PM_ERR("art: no %s (%s)", path, SDL_GetError());
        return false;
    }
    SDL_Surface* s = SDL_LoadPNG_IO(SDL_IOFromConstMem(data, size), true);
    SDL_free(data);
    if (!s) {
        PM_ERR("art: %s is not a PNG it can read (%s)", path, SDL_GetError());
        return false;
    }
    SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(s);
    if (!c) return false;
    w = c->w;
    h = c->h;
    rgba.resize(size_t(w) * size_t(h) * 4);
    for (int y = 0; y < h; ++y) std::memcpy(&rgba[size_t(y) * size_t(w) * 4], static_cast<const u8*>(c->pixels) + size_t(y) * size_t(c->pitch), size_t(w) * 4);
    SDL_DestroySurface(c);
    if (premultiply)
        for (size_t i = 0; i < rgba.size(); i += 4) {
            const u32 a = rgba[i + 3];
            for (int k = 0; k < 3; ++k) rgba[i + size_t(k)] = u8((u32(rgba[i + size_t(k)]) * a + 127) / 255);
        }
    return true;
}

// A PNG's size from its header (no decoding).
bool pngSize(const char* dir, const char* name, int& w, int& h) {
    char path[400];
    std::snprintf(path, sizeof path, "%s/%s", dir, name);
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (!io) return false;
    u8 b[24] = {};
    const size_t got = SDL_ReadIO(io, b, sizeof b);
    SDL_CloseIO(io);
    if (got != sizeof b || b[1] != 'P' || b[2] != 'N' || b[3] != 'G') return false;
    w = int((u32(b[16]) << 24) | (u32(b[17]) << 16) | (u32(b[18]) << 8) | u32(b[19]));
    h = int((u32(b[20]) << 24) | (u32(b[21]) << 16) | (u32(b[22]) << 8) | u32(b[23]));
    return w > 0 && h > 0;
}

bool fileThere(const char* dir, const char* name) {
    char path[400];
    std::snprintf(path, sizeof path, "%s/%s", dir, name);
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (!io) return false;
    SDL_CloseIO(io);
    return true;
}

u32 hexColor(const char* s) {
    if (s[0] == '#') ++s;
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    return u32(std::strtoul(s, nullptr, 16)) & 0xFFFFFFu;
}

// The next line of `text` (a writable, NUL-terminated buffer): comments and the line end cut off.
char* nextLine(char*& p) {
    if (!*p) return nullptr;
    char* line = p;
    while (*p && *p != '\n') ++p;
    if (*p) *p++ = '\0';
    if (char* hash = std::strchr(line, '#')) *hash = '\0';
    for (size_t n = std::strlen(line); n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'); --n) line[n - 1] = '\0';
    return line;
}

}  // namespace

bool Art::load(const char* dir) {
    destroy();
    if (dir && !std::strcmp(dir, "none")) return false;  // --art none: the paper stickers
    const u64 t0 = SDL_GetTicks();
    // Where the files are: --art, the APK's assets, the browser's preloaded files, beside the program, the repository.
    const char* candidates[4] = {};
    int n = 0;
    if (dir && dir[0]) candidates[n++] = dir;
#if defined(__ANDROID__)
    candidates[n++] = "art";
#elif defined(__EMSCRIPTEN__)
    candidates[n++] = "/art";
#else
    char beside[300] = "";
    if (const char* base = SDL_GetBasePath()) {
        std::snprintf(beside, sizeof beside, "%sart", base);
        candidates[n++] = beside;
    }
#ifdef PM_ART_DIR
    candidates[n++] = PM_ART_DIR;
#endif
#endif
    for (int i = 0; i < n && !dir_[0]; ++i)
        if (fileThere(candidates[i], "sprites.txt")) std::snprintf(dir_, sizeof dir_, "%s", candidates[i]);
    if (!dir_[0]) {
        PM_LOG("art: no assets/art (sprites.txt): the paper stickers");
        return false;
    }
    {
        GLint ext = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &ext);
        for (GLint i = 0; i < ext; ++i) {
            const char* e = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, GLuint(i)));
            if (e && std::strstr(e, "texture_filter_anisotropic")) {
                GLint m = 0;
                glGetIntegerv(kAnisoMax, &m);
                maxAniso_ = float(minv<GLint>(m, 8));
            }
        }
        while (glGetError() != GL_NO_ERROR) {
        }
    }
    std::vector<char> text;
    if (!readText(dir_, "sprites.txt", text) || !loadSprites(text.data())) {
        destroy();
        return false;
    }
    if (readText(dir_, "worlds.txt", text)) loadWorlds(text.data());
    else PM_WARN("art: no worlds.txt in %s", dir_);
    ready_ = true;
    PM_LOG("art: %d anims, %d frames from %s in %d ms", animCount_, frameCount_, dir_, int(SDL_GetTicks() - t0));
    checkGl("Art::load");
    return true;
}

void Art::destroy() {
    for (GLuint* t : {&spriteTex_, &roadTex_, &landTex_, &backdropTex_}) {
        if (*t) glDeleteTextures(1, t);
        *t = 0;
    }
    backdropLoaded_[0] = '\0';
    delete[] anims_;
    delete[] elevs_;
    delete[] frames_;
    anims_ = nullptr;
    elevs_ = nullptr;
    frames_ = nullptr;
    animCount_ = elevCount_ = frameCount_ = 0;
    for (WorldArt& w : worlds_) w = WorldArt{};
    for (Atlas& a : atlases_) a = Atlas{};
    atlasCount_ = worldLoaded_ = 0;
    arrayW_ = arrayH_ = 1;
    roadLoaded_[0] = landLoaded_[0] = '\0';
    dir_[0] = '\0';
    ready_ = false;
}

bool Art::loadSprites(const char* src) {
    struct Rec {
        char anim[48];
        int elev, frame, atlas;
        float x, y, w, h, px, py, height;
    };
    struct Def {
        char anim[48];
        float fps;
        bool loop, add;
    };
    struct Pt {
        char anim[48];
        int elev;
        char name[16];
        float x, y;
    };
    std::vector<Rec> recs;
    std::vector<Def> defs;
    std::vector<Pt> pts;
    int lineNo = 0, bad = 0;
    std::vector<char> buf(src, src + std::strlen(src) + 1);
    char* p = buf.data();
    // "kind.anim.elev[.frame]": the last fields are numbers.
    auto split = [](const char* full, int fields, char* anim, int* nums) {
        char tmp[96];
        std::snprintf(tmp, sizeof tmp, "%s", full);
        for (int i = fields - 1; i >= 0; --i) {
            char* dot = std::strrchr(tmp, '.');
            if (!dot) return false;
            char* end = nullptr;
            nums[i] = int(std::strtol(dot + 1, &end, 10));
            if (end == dot + 1 || *end) return false;
            *dot = '\0';
        }
        std::snprintf(anim, 48, "%s", tmp);
        return tmp[0] != '\0';
    };
    while (char* line = nextLine(p)) {
        ++lineNo;
        char key[16] = "";
        if (std::sscanf(line, "%15s", key) != 1) continue;
        bool ok = true;
        if (!std::strcmp(key, "atlas")) {  // atlas <id> <file> [world]
            int id = -1, world = 0;
            char file[64] = "";
            ok = std::sscanf(line, "%*s %d %63s %d", &id, file, &world) >= 2 && id >= 0 && id < kMaxAtlases && world >= 0 && world <= kWorlds;
            if (ok) {
                std::snprintf(atlases_[id].file, sizeof atlases_[id].file, "%s", file);
                atlases_[id].world = world;
                atlasCount_ = maxv(atlasCount_, id + 1);
            }
        } else if (!std::strcmp(key, "anim")) {
            Def d{};
            char loop[16] = "loop", blend[16] = "alpha";
            ok = std::sscanf(line, "%*s %47s %f %15s %15s", d.anim, &d.fps, loop, blend) >= 2;
            d.loop = std::strcmp(loop, "once") != 0;
            d.add = !std::strcmp(blend, "add");
            if (ok) defs.push_back(d);
        } else if (!std::strcmp(key, "sprite")) {
            Rec r{};
            char name[96] = "";
            int nums[2] = {};
            ok = std::sscanf(line, "%*s %95s %d %f %f %f %f %f %f %f", name, &r.atlas, &r.x, &r.y, &r.w, &r.h, &r.px, &r.py, &r.height) == 9 &&
                 split(name, 2, r.anim, nums) && r.w > 0 && r.h > 0 && r.height > 0 && r.atlas >= 0 && r.atlas < kMaxAtlases;
            r.elev = nums[0];
            r.frame = nums[1];
            if (ok) recs.push_back(r);
        } else if (!std::strcmp(key, "point")) {
            Pt t{};
            char name[96] = "";
            int nums[1] = {};
            ok = std::sscanf(line, "%*s %95s %15s %f %f", name, t.name, &t.x, &t.y) == 4 && split(name, 1, t.anim, nums);
            t.elev = nums[0];
            if (ok) pts.push_back(t);
        } else {
            ok = false;
        }
        if (!ok && bad++ < 8) PM_WARN("art: sprites.txt line %d not understood: %s", lineNo, line);
    }
    if (recs.empty() || atlasCount_ == 0) {
        PM_ERR("art: sprites.txt has no sprites or no atlas");
        return false;
    }
    // The atlases share one array as large as the largest: the global ones a layer each, then as many layers as the
    // world with the most atlases of its own needs (each world's are loaded into them while it is played).
    int globals = 0, perWorld[kWorlds + 1] = {}, worldSlots = 0;
    for (int i = 0; i < atlasCount_; ++i) {
        Atlas& a = atlases_[i];
        if (!a.file[0]) continue;
        if (!pngSize(dir_, a.file, a.w, a.h)) {
            PM_ERR("art: no PNG %s/%s", dir_, a.file);
            return false;
        }
        arrayW_ = maxv(arrayW_, a.w);
        arrayH_ = maxv(arrayH_, a.h);
        if (a.world == 0) a.layer = globals++;
        else worldSlots = maxv(worldSlots, ++perWorld[a.world]);
    }
    for (int w = 1; w <= kWorlds; ++w) {
        int k = 0;
        for (int i = 0; i < atlasCount_; ++i)
            if (atlases_[i].file[0] && atlases_[i].world == w) atlases_[i].layer = globals + k++;
    }
    const int layers = maxv(1, globals + worldSlots);
    glGenTextures(1, &spriteTex_);
    glBindTexture(GL_TEXTURE_2D_ARRAY, spriteTex_);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, arrayW_, arrayH_, layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    for (int i = 0; i < atlasCount_; ++i)
        if (atlases_[i].file[0] && atlases_[i].world == 0 && !upload(atlases_[i])) return false;
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    const float W = float(arrayW_), H = float(arrayH_);

    // Frames grouped by anim, elevation, frame.
    std::sort(recs.begin(), recs.end(), [](const Rec& a, const Rec& b) {
        const int c = std::strcmp(a.anim, b.anim);
        return c < 0 || (c == 0 && (a.elev < b.elev || (a.elev == b.elev && a.frame < b.frame)));
    });
    anims_ = new SpriteAnim[kMaxAnims];
    elevs_ = new Elev[kMaxElevs];
    frames_ = new SpriteFrame[minv<size_t>(recs.size(), kMaxFrames)];
    for (const Rec& r : recs) {
        if (frameCount_ >= kMaxFrames) break;
        SpriteAnim* a = animCount_ > 0 ? &anims_[animCount_ - 1] : nullptr;
        if (!a || std::strcmp(a->name, r.anim)) {
            if (animCount_ >= kMaxAnims) break;
            a = &anims_[animCount_++];
            std::snprintf(a->name, sizeof a->name, "%s", r.anim);
            a->elevFirst = elevCount_;
            for (const Def& d : defs)
                if (!std::strcmp(d.anim, r.anim)) {
                    a->fps = d.fps;
                    a->loop = d.loop;
                    a->additive = d.add;
                }
        }
        Elev* e = a->elevCount > 0 ? &elevs_[a->elevFirst + a->elevCount - 1] : nullptr;
        if (!e || e->deg != r.elev) {
            if (elevCount_ >= kMaxElevs) break;
            e = &elevs_[elevCount_++];
            ++a->elevCount;
            e->deg = r.elev;
            e->frameFirst = frameCount_;
        }
        SpriteFrame& f = frames_[frameCount_++];
        f.u0 = r.x / float(W);
        f.v0 = r.y / float(H);
        f.u1 = (r.x + r.w) / float(W);
        f.v1 = (r.y + r.h) / float(H);
        f.layer = float(atlases_[r.atlas].layer);
        f.w = r.w;
        f.h = r.h;
        f.px = r.px;
        f.py = r.py;
        f.height = r.height;
    }
    // Every elevation of an anim plays the same number of frames: the fewest.
    for (int i = 0; i < animCount_; ++i) {
        SpriteAnim& a = anims_[i];
        int fewest = 1 << 30;
        for (int k = 0; k < a.elevCount; ++k) {
            const int next = k + 1 < a.elevCount ? elevs_[a.elevFirst + k + 1].frameFirst
                                                 : (a.elevFirst + a.elevCount < elevCount_ ? elevs_[a.elevFirst + a.elevCount].frameFirst : frameCount_);
            fewest = minv(fewest, next - elevs_[a.elevFirst + k].frameFirst);
        }
        a.frames = maxv(1, fewest);
    }
    for (const Pt& t : pts) {
        const int a = find(t.anim);
        if (a < 0) continue;
        for (int k = 0; k < anims_[a].elevCount; ++k) {
            Elev& e = elevs_[anims_[a].elevFirst + k];
            if (e.deg != t.elev || e.pointCount >= 4) continue;
            std::snprintf(e.pointName[e.pointCount], 16, "%s", t.name);
            e.pointX[e.pointCount] = t.x;
            e.pointY[e.pointCount] = t.y;
            ++e.pointCount;
        }
    }
    PM_LOG("art: %d atlases (%d global, %d layers a world) in a %dx%dx%d array", atlasCount_, globals, worldSlots, arrayW_, arrayH_, layers);
    return animCount_ > 0;
}

bool Art::upload(const Atlas& a) {
    std::vector<u8> rgba;
    int w = 0, h = 0;
    if (!readPng(dir_, a.file, rgba, w, h, true)) return false;
    glBindTexture(GL_TEXTURE_2D_ARRAY, spriteTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, a.layer, minv(w, arrayW_), minv(h, arrayH_), 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    return true;
}

void Art::loadWorlds(const char* src) {
    std::vector<char> buf(src, src + std::strlen(src) + 1);
    char* p = buf.data();
    WorldArt* w = nullptr;
    int lineNo = 0, bad = 0;
    while (char* line = nextLine(p)) {
        ++lineNo;
        char key[16] = "", a[64] = "", b[64] = "";
        if (std::sscanf(line, "%15s", key) != 1) continue;
        bool ok = true;
        float f[8] = {};
        if (!std::strcmp(key, "world")) {
            int n = 0;
            ok = std::sscanf(line, "%*s %d", &n) == 1 && n >= 1 && n <= kWorlds;
            if (ok) {
                w = &worlds_[n - 1];
                *w = WorldArt{};
                w->defined = true;
                const char* name = line;
                for (int skip = 0; skip < 2 && *name; ++skip) {  // past "world" and the number
                    while (*name && *name != ' ' && *name != '\t') ++name;
                    while (*name == ' ' || *name == '\t') ++name;
                }
                std::snprintf(w->name, sizeof w->name, "%s", name);
            }
        } else if (!w) {
            ok = false;
        } else if (!std::strcmp(key, "road")) {
            ok = std::sscanf(line, "%*s %47s %f %f", w->roadFile, &w->roadWidth, &w->roadTile) == 3 && w->roadWidth > 0 && w->roadTile > 0;
        } else if (!std::strcmp(key, "land")) {
            ok = std::sscanf(line, "%*s %47s %f", w->landFile, &w->landTile) == 2 && w->landTile > 0;
        } else if (!std::strcmp(key, "haze")) {
            ok = std::sscanf(line, "%*s %63s %f %f", a, &w->hazeStart, &w->hazeEnd) == 3;
            if (ok) w->haze = hexColor(a);
        } else if (!std::strcmp(key, "sky")) {
            ok = std::sscanf(line, "%*s %63s %63s", a, b) == 2;
            if (ok) {
                w->skyTop = hexColor(a);
                w->skyHorizon = hexColor(b);
            }
        } else if (!std::strcmp(key, "light")) {
            ok = std::sscanf(line, "%*s %63s %f", a, &w->lightStrength) == 2;
            if (ok) w->light = hexColor(a);
        } else if (!std::strcmp(key, "bloom")) {
            ok = std::sscanf(line, "%*s %f %f", &w->bloomThreshold, &w->bloomStrength) == 2;
        } else if (!std::strcmp(key, "vignette")) {
            ok = std::sscanf(line, "%*s %f", &w->vignette) == 1;
        } else if (!std::strcmp(key, "boss")) {
            ok = std::sscanf(line, "%*s %f %63s", &w->bossHue, a) == 2;
            if (ok) w->bossTint = hexColor(a);
        } else if (!std::strcmp(key, "bossaddon")) {
            ok = std::sscanf(line, "%*s %63s", a) == 1;
            if (ok) w->bossAddon = findKind(a);
            if (ok && w->bossAddon < 0) PM_WARN("art: worlds.txt line %d: no sprite %s", lineNo, a);
        } else if (!std::strcmp(key, "grade")) {
            ok = std::sscanf(line, "%*s %f %f", &w->contrast, &w->saturation) == 2;
        } else if (!std::strcmp(key, "backdrop")) {
            ok = std::sscanf(line, "%*s %47s %f", w->backdropFile, &w->backdropHeight) == 2;
        } else if (!std::strcmp(key, "shadow")) {
            const int got = std::sscanf(line, "%*s %f %f %f %63s", &w->shadowDx, &w->shadowDz, &w->shadowAlpha, a);
            ok = got >= 3;
            if (got == 4) w->shadowColor = hexColor(a);
        } else if (!std::strcmp(key, "prop")) {
            ok = std::sscanf(line, "%*s %63s %f %f %f %f %f %f", a, &f[0], &f[1], &f[2], &f[3], &f[4], &f[5]) == 7 &&
                 w->propCount < WorldArt::kMaxProps;
            const int anim = ok ? findKind(a) : -1;
            if (ok && anim >= 0) w->props[w->propCount++] = {anim, f[0], f[1], f[2], f[3], f[4], maxv(0.5f, f[5])};
            else if (ok) PM_WARN("art: worlds.txt line %d: no sprite %s", lineNo, a);
        } else if (!std::strcmp(key, "edge")) {
            ok = std::sscanf(line, "%*s %63s %f %f", a, &f[0], &f[1]) == 3 && w->edgeCount < WorldArt::kMaxEdges;
            const int anim = ok ? findKind(a) : -1;
            if (ok && anim >= 0) w->edges[w->edgeCount++] = {anim, maxv(0.5f, f[0]), f[1]};
            else if (ok) PM_WARN("art: worlds.txt line %d: no sprite %s", lineNo, a);
        } else if (!std::strcmp(key, "ambient")) {
            ok = std::sscanf(line, "%*s %63s %f", a, &f[0]) == 2 && w->ambientCount < WorldArt::kMaxAmbient;
            const int anim = ok ? findKind(a) : -1;
            if (ok && anim >= 0) w->ambient[w->ambientCount++] = {anim, f[0]};
            else if (ok) PM_WARN("art: worlds.txt line %d: no sprite %s", lineNo, a);
        } else {
            ok = false;
        }
        if (!ok && bad++ < 8) PM_WARN("art: worlds.txt line %d not understood: %s", lineNo, line);
    }
}

GLuint Art::loadGround(const char* file, int wrap) {
    std::vector<u8> rgba;
    int w = 0, h = 0;
    if (!readPng(dir_, file, rgba, w, h, wrap == 2)) return 0;
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap == 0 ? GL_REPEAT : (wrap == 2 ? GL_REPEAT : GL_CLAMP_TO_EDGE));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap == 2 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    if (maxAniso_ > 1.0f) glTexParameterf(GL_TEXTURE_2D, kAniso, maxAniso_);
    return t;
}

bool Art::useWorld(int n) {
    if (!ready_) return false;
    n = clampv(n, 1, kWorlds);
    if (worldLoaded_ != n) {  // this world's own atlases into the shared layers
        const u64 t0 = SDL_GetTicks();
        int loaded = 0;
        for (int i = 0; i < atlasCount_; ++i)
            if (atlases_[i].file[0] && atlases_[i].world == n && upload(atlases_[i])) ++loaded;
        if (loaded > 0) {
            glBindTexture(GL_TEXTURE_2D_ARRAY, spriteTex_);
            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
            PM_LOG("art: world %d's %d atlases in %d ms", n, loaded, int(SDL_GetTicks() - t0));
        }
        worldLoaded_ = n;
    }
    const WorldArt& w = world(n);
    if (!w.defined || !w.roadFile[0] || !w.landFile[0]) return false;
    if (std::strcmp(roadLoaded_, w.roadFile)) {
        if (roadTex_) glDeleteTextures(1, &roadTex_);
        roadTex_ = loadGround(w.roadFile, 1);
        std::snprintf(roadLoaded_, sizeof roadLoaded_, "%s", w.roadFile);
    }
    if (std::strcmp(landLoaded_, w.landFile)) {
        if (landTex_) glDeleteTextures(1, &landTex_);
        landTex_ = loadGround(w.landFile, 0);
        std::snprintf(landLoaded_, sizeof landLoaded_, "%s", w.landFile);
    }
    if (std::strcmp(backdropLoaded_, w.backdropFile)) {
        if (backdropTex_) glDeleteTextures(1, &backdropTex_);
        backdropTex_ = w.backdropFile[0] ? loadGround(w.backdropFile, 2) : 0;
        std::snprintf(backdropLoaded_, sizeof backdropLoaded_, "%s", w.backdropFile);
    }
    return roadTex_ && landTex_;
}

const WorldArt& Art::world(int n) const { return worlds_[clampv(n, 1, kWorlds) - 1]; }

int Art::find(const char* name) const {
    for (int i = 0; i < animCount_; ++i)
        if (!std::strcmp(anims_[i].name, name)) return i;
    return -1;
}

int Art::findKind(const char* kind) const {
    const int exact = find(kind);
    if (exact >= 0) return exact;
    const size_t n = std::strlen(kind);
    for (int i = 0; i < animCount_; ++i)
        if (!std::strncmp(anims_[i].name, kind, n) && anims_[i].name[n] == '.') return i;
    return -1;
}

int Art::elevIndex(const SpriteAnim& a, float elevDeg) const {
    int best = 0;
    float bestD = 1e9f;
    for (int k = 0; k < a.elevCount; ++k) {
        const float d = std::fabs(float(elevs_[a.elevFirst + k].deg) - elevDeg);
        if (d < bestD) {
            bestD = d;
            best = k;
        }
    }
    return best;
}

float Art::elevBlend(int a, float elevDeg, float& lo, float& hi) const {
    const SpriteAnim& A = anims_[a];
    lo = hi = elevDeg;
    float below = -1e9f, above = 1e9f;
    for (int k = 0; k < A.elevCount; ++k) {
        const float d = float(elevs_[A.elevFirst + k].deg);
        if (d <= elevDeg && d > below) below = d;
        if (d >= elevDeg && d < above) above = d;
    }
    if (below < -1e8f || above > 1e8f || above - below < 1.0f) return -1.0f;
    lo = below;
    hi = above;
    const float band = 0.3f * (hi - lo), u = clampv((elevDeg - (0.5f * (lo + hi) - band)) / (2.0f * band), 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}

const SpriteFrame& Art::frameAt(int a, float elevDeg, int index) const {
    const SpriteAnim& A = anims_[a];
    int i = index;
    if (A.loop) {
        i %= A.frames;
        if (i < 0) i += A.frames;
    } else {
        i = clampv(i, 0, A.frames - 1);
    }
    return frames_[elevs_[A.elevFirst + elevIndex(A, elevDeg)].frameFirst + i];
}

const SpriteFrame& Art::frame(int a, float elevDeg, float t) const {
    return frameAt(a, elevDeg, int(std::floor(t * anims_[a].fps)));
}

bool Art::point(int a, float elevDeg, const char* name, float& x, float& y) const {
    const SpriteAnim& A = anims_[a];
    const Elev& e = elevs_[A.elevFirst + elevIndex(A, elevDeg)];
    for (int i = 0; i < e.pointCount; ++i)
        if (!std::strcmp(e.pointName[i], name)) {
            x = e.pointX[i];
            y = e.pointY[i];
            return true;
        }
    return false;
}

}  // namespace pm
