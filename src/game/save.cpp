#include "game/save.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pm {

int SaveData::totalStars() const {
    int t = 0;
    for (u8 s : stars) t += s;
    return t;
}

bool loadSave(const char* path, SaveData& out) {
    out = SaveData();
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char line[1200];
    bool ok = true;
    SaveData s;
    while (std::fgets(line, sizeof line, f)) {
        char* eq = std::strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* v = eq + 1;
        v[std::strcspn(v, "\r\n")] = 0;
        const char* k = line;
        auto num = [&](int lo, int hi, int& dst) {
            char* end = nullptr;
            const long n = std::strtol(v, &end, 10);
            if (end == v || n < lo || n > hi) ok = false;
            else dst = int(n);
        };
        auto flag = [&](bool& dst) {
            if (!std::strcmp(v, "1")) dst = true;
            else if (!std::strcmp(v, "0")) dst = false;
            else ok = false;
        };
        if (!std::strcmp(k, "version")) num(1, 1000, s.version);
        else if (!std::strcmp(k, "level")) num(1, 100000, s.level);
        else if (!std::strcmp(k, "stars")) {
            const size_t n = std::strlen(v);
            for (size_t i = 0; i < n && i < sizeof s.stars; ++i) {
                if (v[i] < '0' || v[i] > '3') ok = false;
                else s.stars[i] = u8(v[i] - '0');
            }
        } else if (!std::strcmp(k, "sound")) flag(s.sound);
        else if (!std::strcmp(k, "music")) flag(s.music);
        else if (!std::strcmp(k, "haptics")) flag(s.haptics);
        else if (!std::strcmp(k, "fast")) flag(s.fast);
        else if (!std::strcmp(k, "finished")) flag(s.finished);
        else if (!std::strcmp(k, "reduce_effects")) flag(s.reduceEffects);
        else if (!std::strcmp(k, "refresh60")) flag(s.refresh60);
        else if (!std::strcmp(k, "hints")) {
            char* end = nullptr;
            s.hints = u64(std::strtoull(v, &end, 10));
            if (end == v) ok = false;
        } else if (!std::strcmp(k, "endless_best")) num(0, 1000000, s.endlessBest);
        else if (!std::strcmp(k, "endless_distance")) num(0, 2000000000, s.endlessDistance);
    }
    std::fclose(f);
    if (!ok) return false;
    out = s;
    return true;
}

bool writeSave(const char* path, const SaveData& s) {
    char tmp[600];
    std::snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE* f = std::fopen(tmp, "wb");
    if (!f) return false;
    int last = 0;
    for (int i = 0; i < int(sizeof s.stars); ++i)
        if (s.stars[i]) last = i + 1;
    char stars[1001];
    for (int i = 0; i < last; ++i) stars[i] = char('0' + s.stars[i]);
    stars[last] = 0;
    std::fprintf(f,
                 "version=%d\nlevel=%d\nstars=%s\nsound=%d\nmusic=%d\nhaptics=%d\nfast=%d\nreduce_effects=%d\nrefresh60=%d\nfinished=%d\nhints=%llu\n"
                 "endless_best=%d\nendless_distance=%d\n",
                 s.version, s.level, stars, s.sound, s.music, s.haptics, s.fast, s.reduceEffects, s.refresh60, s.finished, (unsigned long long)s.hints,
                 s.endlessBest, s.endlessDistance);
    const bool ok = std::fflush(f) == 0;
    std::fclose(f);
    if (!ok) return false;
#ifdef _WIN32
    std::remove(path);  // rename does not replace on Windows; on Android it does, atomically
#endif
    return std::rename(tmp, path) == 0;
}

}  // namespace pm
