#pragma once
// The one-way 60 Hz fallback (TECH_SPEC 5.10): on a panel above 60 Hz, more than 5% of a level's frames late means 60 Hz
// from the next level on, remembered in the save. A frame is late when it is shown two or more periods after the one
// before (the contract's rule, PLAN 5).

namespace gt {

inline bool lateFrame(float intervalSeconds, float refreshHz) {
    const float period = 1.0f / (refreshHz > 1.0f ? refreshHz : 60.0f);
    return intervalSeconds >= 1.5f * period;  // rounds to two or more periods
}

// Decided at the end of a level; at least two seconds of frames, so a level that ends at once proves nothing.
inline bool shouldFallBackTo60(float refreshHz, int frames, int late) {
    return refreshHz > 61.0f && frames >= int(2.0f * refreshHz) && late * 20 > frames;
}

}  // namespace gt
