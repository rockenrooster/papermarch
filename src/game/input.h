#pragma once
// Input as the game sees it: pointers in frame pixels (top-left origin) and a few logical keys (TECH_SPEC 3.4).

#include "core/types.h"

namespace pm {

enum class PtrType { Down, Move, Up };

struct PointerEvent {
    PtrType type;
    i64 id;          // finger id; -1 for the desktop mouse
    float x, y;      // frame pixels
    float dx, dy;    // frame pixels since the previous event of this pointer
};

enum class Key { Back, Pause, Left, Right, Fire, Debug };

struct SafeArea {
    float top = 0, bottom = 0, left = 0, right = 0;  // pixels
};

}  // namespace pm
