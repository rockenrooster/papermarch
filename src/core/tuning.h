#pragma once
// Every tunable number (TECH_SPEC 7.4): GAME_DESIGN 6's gameplay values and the renderer's art values, listed once.
// `--set name=value` overrides one at start-up in the app and the headless tool. Values are floats.

#define GT_TUNING(X)                                                                                                  \
    /* ---- gameplay (GAME_DESIGN 6) ---- */                                                                         \
    X(fireRate, 12.0f)          /* wisps a second */                                                                 \
    X(wispSpeed, 8.0f)          /* m/s up the lane */                                                                \
    X(gloomSpeed, 3.2f)                                                                                              \
    X(bruteSpeed, 2.0f)                                                                                              \
    X(homing, 0.8f)             /* m/s toward the launcher's x */                                                    \
    X(funnelLen, 7.0f)                                                                                               \
    X(funnelX, 2.5f)                                                                                                 \
    X(funnelSpeed, 3.0f)                                                                                             \
    X(fanBase, 0.9f)            /* sideways push after xk: fanBase + fanPerK * k */                                  \
    X(fanPerK, 0.5f)                                                                                                 \
    X(fanDecay, 0.5f)           /* time constant, s */                                                               \
    X(sepRadius, 0.96f)         /* of the two radii */                                                               \
    X(sepSpeed, 6.0f)                                                                                                \
    X(capW, 2400.0f)                                                                                                 \
    X(capG, 1000.0f)                                                                                                 \
    X(capReserve, 64.0f)                                                                                             \
    X(coreHealth, 10.0f)                                                                                             \
    X(fightBase, 0.12f)         /* fight = fightBase + fightPerLevel * min(level, fightLevelCap) */                  \
    X(fightPerLevel, 0.006f)                                                                                         \
    X(fightLevelCap, 30.0f)                                                                                          \
    X(burstMult, 2.0f)                                                                                               \
    X(burstSeconds, 3.0f)                                                                                            \
    X(brutePowerBase, 16.0f)                                                                                         \
    X(brutePowerPerLevel, 0.5f)                                                                                      \
    X(brutePowerMax, 60.0f)                                                                                          \
    X(bruteEvery, 8.0f)                                                                                              \
    X(dragGain, 1.15f)                                                                                               \
    X(dragSmoothing, 0.040f)    /* s */                                                                              \
    X(emitterDelay, 1.5f)                                                                                            \
    X(rampStart, 1.5f)          /* times the target time */                                                         \
    X(rampSlope, 20.0f)                                                                                              \
    /* ---- geodes and boons (VARIETY_DESIGN 2, 3) ---- */                                                           \
    X(geodeSlipZ, 2.5f)         /* an unopened geode below this slips past */                                        \
    X(plateGap, 1.2f)           /* the plate stands this far below its geode */                                      \
    X(plateWidth, 3.0f)                                                                                              \
    X(lanternGap, 0.9f)         /* m between lanterns */                                                             \
    X(lanternMax, 3.0f)                                                                                              \
    X(quickenStep, 0.25f)       /* of the base rate, per QUICKEN */                                                  \
    X(quickenMax, 3.0f)                                                                                              \
    X(brightMax, 2.0f)          /* extra power per wisp at most */                                                   \
    X(wardZ, 14.0f)             /* WARD pops every gloom below this */                                               \
    X(kindleMax, 6.0f)          /* KINDLE: a multiply gate counts one more, up to this */                            \
    X(cometSeconds, 4.0f)       /* COMET: a wisp with this many seconds of its lantern's fire */                     \
    X(hushSeconds, 6.0f)        /* HUSH: the emitters send nothing this long */                                      \
    /* ---- kinds and twists (VARIETY_DESIGN 6, 7) ---- */                                                           \
    X(colossusDelay, 3.0f)      /* s before the colossus steps out */                                                \
    X(colossusPace, 1.25f)      /* it reaches the lantern at this many T */                                          \
    X(colossusSway, 1.5f)       /* m to each side */                                                                 \
    X(colossusSwayPeriod, 6.0f)                                                                                      \
    X(colossusRadius, 1.8f)                                                                                          \
    X(colossusLoseZ, 3.0f)                                                                                           \
    X(colossusStep, 0.8f)       /* s between its steps (the view's trauma) */                                        \
    X(cubRadius, 1.1f)                                                                                               \
    X(cubSpeed, 1.2f)                                                                                                \
    X(cubShare, 0.05f)          /* a cub's health, of the spire's */                                                 \
    X(cubCore, 3.0f)            /* pips a cub takes when it reaches the lantern */                                   \
    X(eclipseSeconds, 6.0f)                                                                                          \
    X(eclipseSpeed, 1.3f)       /* glooms walk this much faster under ECLIPSE */                                     \
    X(goldenSeconds, 8.0f)                                                                                           \
    X(goldenExtra, 2.0f)        /* GOLDEN GATE: the gate counts this many more */                                    \
    X(stampedeRunners, 16.0f)                                                                                        \
    X(stampedeSeconds, 2.0f)                                                                                         \
    X(wardWallShare, 0.06f)     /* WARD WALL's health, of the spire's */                                             \
    X(twinX, 2.6f)              /* the twin spires stand at -twinX and +twinX */                                     \
    X(festivalSeconds, 20.0f)                                                                                        \
    X(holdoutLow, 0.6f)         /* Holdout: the flow grows from this to 2 - this across T */                         \
    /* ---- the lane and the enemy (VARIETY_DESIGN 4, 5) ---- */                                                     \
    X(hopCross, 0.25f)          /* s a hopper takes to jump */                                                       \
    X(runnerSpeed, 5.0f)                                                                                             \
    X(doorStep, 1.5f)           /* m/s a side door's glooms step in */                                               \
    X(doorStepSeconds, 1.0f)                                                                                         \
    X(pulsePeriod, 5.0f)        /* Pulse: half at 1.8 times the flow, half at 0.2 */                                 \
    X(pincerSeconds, 4.0f)      /* Pincer: the two emitters swap this often */                                       \
    X(rushSeconds, 8.0f)        /* Early rush: 1.6 times the flow this long */                                       \
    X(bruteLineEvery, 4.0f)     /* Brute line: a brute this often, with half the flow */                             \
    X(packEvery, 6.0f)          /* Runner packs: a pack this often */                                                \
    /* ---- art and feel ---- */                                                                                     \
    X(glowStrength, 1.0f)                                                                                            \
    X(glowUnit, 0.34f)          /* alpha of a unit's disc in the glow buffer */                                      \
    X(glowUnitSize, 1.7f)       /* radii */                                                                          \
    X(lightUnit, 0.13f)         /* alpha of a unit's disc in the light map */                                        \
    X(lightRadius, 0.9f)        /* m, plus 0.3 per tier */                                                           \
    X(laneLightAdd, 0.55f)      /* how much of the light map is added on top of the lit lane */                      \
    X(camFollow, 0.12f)                                                                                              \
    X(shakeMax, 0.35f)                                                                                               \
    X(traumaDecay, 2.2f)                                                                                             \
    X(hopHeight, 0.25f)                                                                                              \
    X(hopRate, 5.0f)                                                                                                 \
    X(labelMinFrac, 0.03f)      /* a label's minimum height, of the frame */                                         \
    X(particleBudget, 220.0f)   /* particles spawned at most per frame */                                            \
    X(labels, 1.0f)             /* 0 hides world labels (measurements) */                                            \
    X(debugReadout, 0.0f)

namespace gt {

struct Tuning {
#define GT_TUNING_FIELD(name, def) float name = def;
    GT_TUNING(GT_TUNING_FIELD)
#undef GT_TUNING_FIELD
};

extern Tuning tune;

// Sets one value by name; false if the name is unknown or the value is not a number.
bool setTuning(const char* name, const char* value);
// "name=value"
bool setTuningPair(const char* pair);

}  // namespace gt
