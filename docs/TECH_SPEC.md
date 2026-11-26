# PAPERMARCH — Technical spec

## 1. Modules

| Path | What |
|---|---|
| `src/core` | types and `minv/maxv/clampv`, PCG32 `Rng` and `hash64`, the clock, logging, PNG writer, arguments, `Tuning` (`--set name=value`), the allocation counter |
| `src/sim` | `battle.*` the rules (no SDL, no GL, no allocation after start); `bot.*` the steering bot and `botPlay`; `levels.cpp` the baked table, its text form, the hint lines; `levels_baked.h` the bake's output (committed) |
| `src/gen` | the generator and the bake (linked into the headless tool and the tests only, never the app) |
| `src/gfx` | GL loader, shaders, render targets, `paper.*` (the renderer), `text.*` (signed-distance text and UI quads), `palette.*` (worlds' colours, contrast) |
| `src/audio` | `synth.*` the sound bank, `mixer.*` voices, the march sequencer, the count ladder, the limiter |
| `src/game` | `app.*` the loop, `game.*` levels, input, the frame; `field.*` the battle on screen; `screens.cpp` the HUD, panels and the flow test; `feedback.*` cues to sounds and haptics; `ui.*`, `layout.*`, `save.*`, `bench.*`, `pictures.cpp`, `audio_out.*` |
| `src/platform` | SDL main, Android (haptics, refresh rate, the updater through JNI), the browser (save in `localStorage`) |
| `src/tools` | `headless.cpp` (play, table, bake, audio renders), `tests.cpp`, `audio_render.*` |
| `shaders` | `sticker.*` paper shapes, `ground.frag` the road, `shadow.frag`, `text.*`, `curl.*` and `page.*` (the page turn), `table.frag`, the preludes |

## 2. Build

One CMake tree for Windows (Ninja, llvm-mingw clang), Android (Gradle 8.13, AGP 8.13.0, NDK 29.0.14206865, SDK CMake
3.22.1; `libmain.so`) and the browser (Emscripten 7.0.1, WebGL 2, one thread). Warnings are errors; no exceptions, no
RTTI. Shaders are `#version 300 es` with a prelude, validated by glslangValidator at build time and embedded. At most 4
parallel jobs everywhere.

## 3. The battle (`src/sim/battle.*`)

`Battle::tick(steerX, go, events)` is one 1/60 s step:

1. Not started and not `go`: nothing. Then `time += dt`.
2. Steer: `x` moves toward `steerX` (clamped to the road less half the squad's radius, at most 1.6) at
   `clamp((target - x)·12, ±18)` units a second.
3. March `speed·dt`, unless the boss is within 8 units (halt) or a boss-less level is at its length.
4. Wake waves whose front is within 62 units (`spawnWave`: ranks of `gap = 2.05·radius`, odd rows offset half a gap,
   each soldier jittered by ±0.25 and ±0.4 gaps); place the boss at the level's length when it is within 62.
5. Gates whose z the squad's centre crossed this step: the one containing `x` applies (`+value` or `×value`, clamped to
   0..99,999), the others become `passed`.
6. Barrels within reach of the squad's front and footprint: run into, they burst and cost max(1, hp/2).
7. Fire: `fireRate·dt` bullets accumulate; each starts at a random point across the squad's front.
8. Bullets: enemies are bucketed by distance ahead (48 one-unit cells, rebuilt each step); each bullet finds the nearest
   growing gate, barrel or enemy along its step and stops there (a gate gains one up to its cap; a barrel or enemy loses
   `damage`), or flies on until its range is spent.
9. Enemies: march down; within 14 units close in on `x` (riders faster); one that touches the footprint
   (`d < 0.6·r + radius`, `|dx| < r + radius`) costs its strength and is gone; the boss instead stops and drains 22
   soldiers a second. Dead enemies are compacted away now and then.
10. Squad ≤ 0: lost. A boss-less level at its length: won. The boss's death: won.

`BattleEvents` reports what the step did (shots, hits, kills with their places, the gate passed with before and after,
a barrel broken or run into, a wave woken, the boss hit or down, won, lost). The rules use floats: the same build on the
same machine gives the same result (the tests replay the bake's numbers); other platforms may differ by a soldier, which
only moves stars, so nothing depends on cross-platform equality.

## 4. Levels

The text form, one string per level: `S<start> L<length×10> V<speed×100> B<bossHp> W<world> X<boss> T<hint> E<seed>
A<star2> C<star3> R<botEnd>`, then `g<x0>,<x1>,<z>,<value>,<kind>,<grows>,<cap>` (x and z ×10), `b<x>,<z>,<hp>,<reward>`,
`w<z>,<x0>,<x1>,<count>,<kind>`. `generate` returns a level that has been through this form, so the bake plays exactly
what the table will hold. `levelCount()`/`level(n)` decode `kBaked[]` once; a fallback level exists if the table is empty.

## 5. The view (`src/game/field.*`)

**Camera.** A pinhole at height h = 20 pitched θ = 34° down. The squad is `camBack` = 6 units ahead of the camera's
foot. With `depth_s = h·sinθ + 6·cosθ` and `up_s = (h·cosθ − 6·sinθ)/depth_s`, the focal length is
`f = roadPx·depth_s/10` (roadPx = min(0.92·W, 0.44·H)) and the principal point `cy = 0.80·H − f·up_s`, so the squad
stands 80% down the screen with the road 92% of the width there. A point (x, z, height y): `t = z − camZ`,
`depth = (h − y)·sinθ + t·cosθ`, `up = (y − h)·cosθ + t·sinθ`, screen `(cx + x·f/depth, cy − up·f/depth)`, scale
`f/depth` pixels a unit. All of these are tunables (`camPitch`, `camHeight`, `camBack`, `camRoad`, `camRoadH`,
`camSquadY`).

**Frame assembly.** Between steps the squad's z and x are extrapolated by the last step's motion times the frame's
alpha (enemies by their speed, bullets by theirs), so the picture is smooth at any refresh rate. Every object adds its
stickers to a pool under one sort key (its road z); the items are sorted far to near and copied into one array. Foot
shadows (flat ellipses) go to a separate list that casts the union shadow. Tags (gate values, barrel rewards and
strength bars, the squad's badge, floating numbers) go to the labels batch, drawn over the stickers. Pools: 5,600
stickers, 3,200 items, 2,600 shadows, 700 particles (a ring), 24 floating numbers. Enemies far away get 2 stickers, near
ones 4 or 5. Scenery is hashed from 4.5-unit cells of the road, so it is the same on every visit.

**Effects.** Particles live in road space: paper bits (gravity, bounce), smoke puffs (grow, fade), falling figures (a
pawn tipping over), sparks, ground rings, confetti (slow fall, flutter). Floating numbers either stay where they were
made (barrels) or follow the squad (gates). Screen shake moves the principal point for a moment.

**Cues.** The view turns events into cues (`kCueShot`, `kCueKill`, `kCueGate`, `kCueBarrel`, `kCueBoss`, ...), rate
limited per kind (shots 13 a second, hits 20, kills 28), each with a pan from its x. `Feedback` plays them.

## 6. The renderer (`src/gfx/paper.*`)

One frame: the ground pass (a full-screen triangle; `ground.frag` casts each pixel's ray onto the road plane with the
camera's uniforms and shades stones, mortar, curbs, verge, the finish line and haze, with `fwidth` to fade the pattern
before it shimmers); the union shadow (the foot-shadow stickers max-blended into a half-resolution R8 target, laid over
once at `shadowStrength`); the stickers in one instanced draw (each a quad whose fragment shader draws one SDF shape:
box, ellipse, triangle, pawn, banner, star, ring, capsule, blob, horse, diamond, arch, with a two-tone fold and an
analytic edge); the labels; the page turn (a snapshot curled away) when a level changes; the UI. About 7 draw calls.

## 7. Audio

`Mixer` on the audio thread: one-shot voices with gain, pan, pitch and delay; the count ladder (a scale step per tick);
a five-layer march sequencer whose layers the game sets each frame (`musicLayers`); a soft limiter. The sound bank is
synthesized once at start-up (about 40 ms). New for the battle: `kSndShot`, `kSndBoom`, `kSndPop`, `kSndThwack`.

## 8. Tools and tests

- `papermarch_tests <dir>`: the battle's rules (gates add and multiply, a barricade shot up, a barrel shot open and one
  run into, a horde too big and one too small, the boss beaten and the boss winning), the text form's round trip, the
  bot on every level of the table (it must win, with exactly the squad the bake recorded), palettes, the save, number
  formatting, the PNG writer.
- `papermarch_headless --play --level N` (the bot's run, event by event), `--sim table` (every level), `--bake-levels`,
  `--render-audio music|battle|effects|win|lose|ui|turn|ladder`.
- The app: `--flow-test --fixed-step` (23 checks: drag to march, steering, a win, NEXT, LEVELS, a defeat, RETRY, pause,
  settings), `--picture level|world|figures|home_update|screen_<play|pause|win|lose|settings|levels|end> [--lint]`,
  `--bot best`, `--scenario bench_idle|bench_battle|bench_play`, `--log-sites` (where to drag and where NEXT is, for the
  browser check).
- `tools\web_check.mjs flow|play|reload`: headless Chrome at an iPhone's size with touch; `play` holds a finger on the
  squad and sweeps it until level 1 ends, taps NEXT, then measures frames a second while the bot plays level 30.
