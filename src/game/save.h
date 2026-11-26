#pragma once
// The save (TECH_SPEC 7.3): save.txt in the preferences path, key=value lines, written atomically (tmp + rename). A
// missing or damaged file means defaults and never stops the game.

#include "core/types.h"

namespace gt {

struct SaveData {
    int version = 1;
    int level = 1;              // the level HOME shows
    u8 stars[1000] = {};        // best stars per level (index = level - 1)
    bool sound = true, music = true, haptics = true, autoFire = false, reduceEffects = false;
    bool refresh60 = false;     // the one-way 60 Hz fallback (TECH_SPEC 5.10)
    u32 hints = 0;              // bit per hint shown
    int totalStars() const;
};

bool loadSave(const char* path, SaveData& out);  // false (and defaults) if missing or damaged
bool writeSave(const char* path, const SaveData& s);

}  // namespace gt
