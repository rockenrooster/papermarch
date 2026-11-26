#pragma once
// Every tunable number (TECH_SPEC 8): the feel and art values, listed once. `--set name=value` overrides one at
// start-up in the app and the headless tool. Values are floats.

#define PM_TUNING(X)                                                                                                  \
    /* ---- rules and stars (GAME_DESIGN 7) ---- */                                                                  \
    X(star2Pct, 70.0f)          /* two stars at this share of the best final host */                                 \
    X(deadNudge, 1.0f)          /* 1: say so when no line wins any more */                                           \
    /* ---- feel (GAME_DESIGN 10) ---- */                                                                            \
    X(marchSpeed, 90.0f)        /* map units a second */                                                             \
    X(fastFactor, 2.5f)         /* the clock with a move queued, or FAST MOVES */                                    \
    X(turnSeconds, 0.65f)       /* the page turn */                                                                  \
    X(tagScale, 1.0f)                                                                                                \
    X(winPanelDelay, 1.6f)      /* seconds after the win before the panel */                                         \
    /* ---- look (GAME_DESIGN 9) ---- */                                                                             \
    X(shadowStrength, 0.42f)                                                                                         \
    X(grain, 0.045f)                                                                                                 \
    /* ---- music (GAME_DESIGN 12.1): the host's share of its goal at which each layer joins ---- */                 \
    X(layer1, 0.10f)                                                                                                 \
    X(layer2, 0.30f)                                                                                                 \
    X(layer3, 0.60f)                                                                                                 \
    X(layer4, 1.00f)                                                                                                 \
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
