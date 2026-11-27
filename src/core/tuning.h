#pragma once
// Every tunable number (TECH_SPEC 8): the feel and art values, listed once. `--set name=value` overrides one at
// start-up in the app and the headless tool. Values are floats.

#define PM_TUNING(X)                                                                                                  \
    /* ---- the camera over the road (src/game/field.cpp) ---- */                                                    \
    X(camPitch, 30.0f)          /* degrees below the horizon (ART_DIRECTION 5) */                                    \
    X(camHeight, 15.0f)         /* road units */                                                                     \
    X(camBack, 8.5f)            /* how far ahead of the camera's foot the squad marches */                           \
    X(camRoad, 0.92f)           /* the road's width at the squad: this share of the screen's width, */               \
    X(camRoadH, 0.44f)          /* and at most this share of its height */                                           \
    X(camSquadY, 0.80f)         /* the squad's place on the screen, from the top */                                  \
    /* ---- feel ---- */                                                                                             \
    X(steer, 1.25f)             /* road widths the squad moves for a drag across the road's width on screen */       \
    X(soldiersShown, 500.0f)    /* soldiers drawn at most (the badge counts them all) */                             \
    X(turnSeconds, 0.65f)       /* the page turn between levels */                                                   \
    X(tagScale, 1.0f)                                                                                                \
    X(winPanelDelay, 1.3f)      /* seconds after the win before the panel */                                         \
    /* ---- look ---- */                                                                                             \
    X(shadowStrength, 0.38f)                                                                                         \
    X(grain, 0.045f)                                                                                                 \
    X(musicGain, 1.0f)                                                                                               \
    X(debugReadout, 0.0f)

namespace pm {

struct Tuning {
#define PM_TUNING_FIELD(name, def) float name = def;
    PM_TUNING(PM_TUNING_FIELD)
#undef PM_TUNING_FIELD
};

extern Tuning tune;

// Sets one value by name; false if the name is unknown or the value is not a number.
bool setTuning(const char* name, const char* value);
// "name=value"
bool setTuningPair(const char* pair);

}  // namespace pm
