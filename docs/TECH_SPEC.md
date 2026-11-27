# PAPERMARCH — Technical spec

## 1. Modules

| Path | What |
|---|---|
| `src/core` | types and `minv/maxv/clampv`, PCG32 `Rng` and `hash64`, the clock, logging, PNG writer, arguments, `Tuning` (`--set name=value`), the allocation counter |
| `src/sim` | `battle.*` the rules (no SDL, no GL, no allocation after start); `bot.*` the steering bot and `botPlay`; `levels.cpp` the baked table, its text form, the hint lines; `levels_baked.h` the bake's output (committed) |
| `src/gen` | the generator and the bake (linked into the headless tool and the tests only, never the app) |
| `src/gfx` | GL loader, shaders, render targets, `paper.*` (the renderer), `art.*` (the art's manifest, atlases, ground textures and world looks from `assets/art`), `text.*` (signed-distance text and UI quads), `palette.*` (worlds' colours, contrast) |
| `src/audio` | `synth.*` the sound bank, `mixer.*` voices, the march sequencer, the count ladder, the limiter |
| `src/game` | `app.*` the loop, `game.*` levels, input, the frame; `field.*` the battle on screen; `screens.cpp` the HUD, panels and the flow test; `feedback.*` cues to sounds and haptics; `ui.*`, `layout.*`, `save.*`, `bench.*`, `pictures.cpp`, `audio_out.*` |
| `src/platform` | SDL main, Android (haptics, refresh rate, the updater through JNI), the browser (save in `localStorage`) |
| `src/tools` | `headless.cpp` (play, table, bake, audio renders), `tests.cpp`, `audio_render.*` |
| `shaders` | `sticker.*` paper shapes, the art's sprites and the gates' panels, `ground.frag` the road, land, sky and backdrop, `shadow.frag`, `bright.frag`, `blur.frag` and `post.frag` (bloom, grade, vignette), `text.*`, `curl.*` and `page.*` (the page turn), `table.frag`, the preludes |
| `assets/art`, `tools/art` | the art (docs/ART_DIRECTION.md): atlases, `sprites.txt`, ground textures, `worlds.txt`; the Blender and Python scripts that make them |

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
   0..99,999; a weapon gate hands over weapon `value`), the others become `passed`.
6. Barrels within reach of the squad's front and footprint: run into, they burst and cost max(1, hp/2) (a weapon crate
   still hands over its weapon; a supply crate costs nothing and gives its power-up). Medallions the footprint touches
   (±0.6) give theirs (`take`: timers in `powerLeft`/`powerFull`, the shield's `shieldPool`, an airstrike's three
   `bombs`). Every loss goes through `lose`, where a running shield absorbs first.
7. Fire: the squad carries one weapon (`weapon`, `WeaponStats`, docs/CONTENT_PASS.md 1; the level's `gun` at the start).
   Hit points are counted in tenths (`kHp`); a musket ball does `damage() = max(10, squad/5)` tenths and the musket
   fires `fireRate()` shots a second; the weapon multiplies both. Against the horde (all but the boss and giants) a blow
   counts as `dmg·10/damage()`: a musket ball's worth whatever the squad's size (`against`, used by `hurt` and the
   `incoming` books). `rate·fireRate·dt` shots accumulate; each starts at a
   random point across the squad's footprint (x ± radius, squeezed inside the road: `footprint`) and goes for the nearest
   enemy within the weapon's range (by distance up the road, then across) whose hit points exceed the damage already
   flying at it (`incoming`), led to where the two meet (the owner, 2026-11-27: "generally hit the closest enemy to the
   squad"); the fire spreads over the front ranks, a brute in front takes all it needs, the boss everything once nothing
   stands before it. With no enemy within range a shot flies straight up the road (and grows the gates and breaks the
   barrels in its lane). Twin rifles fire a pair 0.5 apart; the scattergun a fan of five over ±14°, only its aimed
   middle pellet counted as incoming and growing gates.
8. Bullets: enemies are bucketed by distance ahead (64 one-unit cells, rebuilt each step by `fire`); each bullet moves at
   its weapon's speed along its aim, finds the nearest growing gate or barrel along its step, and the enemies before it:
   a ball stops at the first enemy (or at the gate, which gains one up to its cap, or the barrel); a lance bolt passes
   through up to six enemies, each once (those whose middle it passes this step), and stops at the boss or a structure;
   a rocket bursts where it strikes, half its direct damage to every other enemy within 1.8 (`blast`).
   A bullet flies on until its range is spent; when it strikes, its damage no longer counts as incoming. Compacting
   the dead away remaps the bullets' targets.
9. Enemies: run down at `enemySpeed` (their kind's speed times the level's `pace`, plus the march's speed; 0 for those
   that stand or are frozen; slowed by frost and the hourglass, hurried by drummers); within 14 units close in on `x`
   (riders faster); one that touches the footprint (`d < 0.6·r + radius`, across within the footprint ± radius) costs
   `toll(strength)` (its strength times squad / 50 past 50) and is gone (and is counted against its wave, `waveLost`).
   The squad stands (`halted`) while a horde enemy is within 25 units ahead; the boss instead stops and crushes a ninth of the squad it meets every second (at least the level's
   `drain`, 22), so a boss fight lasts seconds, not a minute, whatever the squad. Dead enemies are compacted away now
   and then.
10. Power-ups: rapid fire doubles the shots, power shot the damage; under freeze the horde within 45 units does not
   move (the boss not for the first half); the clocks run down (`powers`) and bombs land (`blast`, radius 3, 12 musket
   balls' damage).
11. Squad ≤ 0: lost. A boss-less level at its length: won. The boss's death: won.

`BattleEvents` reports what the step did (shots, hits, kills with their places, the gate passed with before and after,
a barrel broken or run into, a wave woken, the boss hit or down, won, lost). The rules use floats: the same build on the
same machine gives the same result (the tests replay the bake's numbers); other platforms may differ by a soldier, which
only moves stars, so nothing depends on cross-platform equality.

## 4. Levels

The text form, one string per level: `S<start> L<length×10> V<speed×100> B<bossHp> W<world> X<boss> T<hint> E<seed>
A<star2> C<star3> R<botEnd>`, optional `K<spike> Y<pace×100> D<drain×10> P<planLook×10> Q<plan,...> G<gun>`, then
`g<x0>,<x1>,<z>,<value>,<kind>,<grows>,<cap>` (x and z ×10; kind 2 is a weapon gate, its value the weapon),
`b<x>,<z>,<hp>,<reward>`, `c<x>,<z>,<hp>,<weapon>` (a weapon crate, in the barrels' list), `s<x>,<z>,<hp>,<power>` (a
supply crate, also there), `w<z>,<x0>,<x1>,<count>,<kind>`, `m<x>,<z>,<power>` (a medallion), `n<x>,<z>` (a mine),
`k<x>,<z>,<prisoners>` (a cage); `M<mode>` the level's mode; `J<bits>` the boss's attacks (1 club, 2 boulders, 4 war cry,
8 stomp); a gate's optional eighth number makes it move (its phase); a wave's optional sixth its hit points each (a
giant's).

Telegraphed blows (`Strike`: a boulder's circle, the club's half of the road, the stomp's gap) wait `t` seconds and then
take the share of the squad's footprint standing in them, at most `cap` (`Battle::attacks`); the bot steps out of them
before anything else. `generate` returns a level that has been through this form, so the bake plays exactly
what the table will hold; the generator also puts everything it lays on the form's grid (`snap`) before the bot plays
it. The generator keeps one run of the road as it lays it (`runner`, 64 units behind the road's end: nothing laid later
can change it) and measures each new section by playing a copy of it from there (`playTo`), so a 15-minute road is built
in tens of seconds. The bake takes that run (the bot's own choices, no plan) to 64 units before the end fight once and
replays only the end for each strength it tries (`bakeLevel`), then plays the kept level from the start to be sure. `levelCount()`/`level(n)` decode `kBaked[]` once; a fallback level exists if the table is empty.

## 5. The view (`src/game/field.*`)

**Camera.** A pinhole at height h = 15 pitched θ = 30° down (ART_DIRECTION 5; it shows the horizon near the top). The
squad is `camBack` = 8.5 units ahead of the camera's foot. With `depth_s = h·sinθ + b·cosθ` and
`up_s = (h·cosθ − b·sinθ)/depth_s`, the focal length is
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

**The art.** With `assets/art` loaded (`Field::setArt`), each figure, prop and effect is one sprite sticker: the frame
of its anim nearest the view's elevation `atan2(h − y, z − camZ)`, sized by the frame's height in road units, placed by
its pivot, with a per-figure size, tint, hue, shade and step; tracers turned along their flight. Between two renderings
(over the middle 60% of the gap between their elevations, `Art::elevBlend`) a sprite is two stickers under one key: the
lower rendering first at full strength while the upper comes in over it, then the upper at full while the lower fades
out under it, so nothing jumps and no body turns see-through (the owner saw the scenery pop, 2026-11-27). Soldiers are
rendered without a gun; the weapon's overlay (`wpn.<name>.march`) is drawn over each one at the same spot, size and
step, and its `muzzle` point places the flash. Shadows are cast on the road along the world's sun (`shadow` in `worlds.txt`): a soft ellipse per thing,
projected from road space, all in the union shadow. Props come from the world's `prop` and `edge` rules (hashed cells),
drifting bits from `ambient`. Gates are a panel sticker (`kShGate`, animated in `sticker.frag`) between two post sprites,
with stakes on a barricade; a taken gate folds down, its row fades. Deaths shrink and fade into a puff; barrels burst
into a blast, its glow, a ring and planks; the boss stomps (rings, dust), takes a blast per tenth of its strength and
dies in three blasts and a big one with a hit-stop and a pull-back of the camera. Without the art the paper stickers
below are drawn instead.

**Effects.** Particles live in road space: paper bits (gravity, bounce), smoke puffs (grow, fade), falling figures (a
pawn tipping over), sparks, ground rings, confetti (slow fall, flutter), flipbooks (an anim played once, after a
delay), drifting bits, figures shrinking away. Floating numbers either stay where they were made (barrels) or follow
the squad (gates). Screen shake moves the principal point for a moment.

**Cues.** The view turns events into cues (`kCueShot`, `kCueKill`, `kCueGate`, `kCueBarrel`, `kCueBoss`, ...), rate
limited per kind (shots 13 a second, hits 20, kills 28), each with a pan from its x. `Feedback` plays them.

## 6. The renderer (`src/gfx/paper.*`)

One frame: the ground pass (a full-screen triangle; `ground.frag` casts each pixel's ray onto the road plane with the
camera's uniforms and shades stones, mortar, curbs, verge, the finish line and haze, with `fwidth` to fade the pattern
before it shimmers); the union shadow (the foot-shadow stickers max-blended into a half-resolution R8 target, laid over
once at `shadowStrength`); the stickers in one instanced draw (each a quad whose fragment shader draws one SDF shape:
box, ellipse, triangle, pawn, banner, star, ring, capsule, blob, horse, diamond, arch, with a two-tone fold and an
analytic edge); the labels; the page turn (a snapshot curled away) when a level changes; the UI. About 7 draw calls.

With the art: the ground pass samples the world's road texture (spanning `road` units across, repeating along) and land
texture (mipmapped, anisotropic where offered), hazes to the world's colour between `haze` start and end, and above the
horizon draws the sky gradient and the backdrop strip (premultiplied, shifted by the squad's x). Sprites sample one
`GL_TEXTURE_2D_ARRAY` (premultiplied at load; additive frames write alpha 0 so they add under the same blend): the
global atlases keep their layers, each world's own atlases (`atlas <id> <file> <world>`) are loaded into the layers after
them when that world is played. The world is drawn into a full-size target, then bloom (a bright pass into a quarter-size
target, two separable blurs), the grade (the world's light at constant brightness, contrast, saturation), a vignette that
spares the HUD row and a red edge flash are applied on the way to the screen; tags and UI are drawn after, unbloomed.

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
