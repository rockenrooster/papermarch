# PAPERMARCH — Technical Specification

Read `PLAN.md` first. This spec is short on purpose: the engine is GLOWTIDE's, and `..\Glowtide\docs\TECH_SPEC.md`
(called **GT-TS** below) still describes every part that is copied. Where this file is silent, GT-TS and the forked
code hold. Signatures here are sketches: the forked code and the pinned headers are the truth.

| Section | Holds |
|---|---|
| 1 | The fork: what to copy, the rename map, what to delete |
| 2 | Build, scripts, app arguments |
| 3 | Simulation: data, rules, events, undo, the solver |
| 4 | Generator and bake |
| 5 | Renderer |
| 6 | The view: animation timeline |
| 7 | Audio |
| 8 | Game layer: states, save |
| 9 | Verification and performance |
| 10 | Traps |

---

## 1. The fork

Source: `D:\Stuff\DevFolder\Android_Games\Glowtide`, branch `main`, commit `dff405a`. Copy files from its working tree
(check `git status` is clean and the branch is `main` first; never `margin-pass`). Do not copy `.git`, `build`,
`captures`, `keys`, `.netlify`, `third_party`, `docs` (except `LICENSES.md` as a base).

### 1.1 Rename map

| GLOWTIDE | PAPERMARCH |
|---|---|
| `glowtide`, `Glowtide`, `GLOWTIDE` (project, targets, strings, file names) | `papermarch`, `Papermarch`, `PAPERMARCH` |
| namespace `gt`, libraries `gt_core`, `gt_sim`, `gt_audio` | `pm`, `pm_core`, `pm_sim`, `pm_audio` (new: `pm_gen`) |
| CMake, script and environment prefix `GT_` (`GT_ROOT`, `GT_BUILD_NUMBER`, `GT_APP_ID_SUFFIX`, `GT_KEYSTORE*`, `GT_DEV`, …) | `PM_` |
| `dev.glowtide.game`, `GlowtideActivity` | `dev.papermarch.game`, `PapermarchActivity` |
| Log tags `GLOWTIDE`, `GTBENCH`, `GTSIM`, `GTLIFE` | `PAPERMARCH`, `PMBENCH`, `PMSIM`, `PMLIFE` (`FLOW` stays) |
| `rockenrooster/glowtide` (`platform::kReleasesUrl`, the Java updater, the workflow, the scripts) | `rockenrooster/papermarch` |
| Web port 8091, localStorage keys `glowtide:*`, the Netlify project `glowtide` | 8092, `papermarch:*`, `papermarch` |
| Lock file's first word `glowtide-<phase>` | `papermarch-<phase>` |
| Version 0.2.0 | 0.1.0 (CMake `project()` and `android/app/build.gradle` kept equal) |

After the rename, `rg -i "glowtide|\bgt_|GT_|gt::" --glob '!docs/**'` must find nothing but the two lines that credit
the fork (README and LICENSES).

### 1.2 File by file

| Path in GLOWTIDE | Action |
|---|---|
| `CMakeLists.txt`, `cmake/*` | Copy, rename; source lists rewritten as files arrive; add `pm_gen` (headless tool and tests only) |
| `scripts/env.ps1`, `fetch-deps.ps1`, `fetch-deps.sh`, `build-desktop.ps1`, `run-desktop.ps1`, `test.ps1`, `build-android.ps1`, `deploy-android.ps1`, `capture-android.ps1`, `bench-android.ps1`, `lifecycle-android.ps1`, `flow-android.ps1`, `save-android.ps1`, `backkey-android.ps1`, `coldstart-android.ps1`, `matrix-android.ps1`, `picture-android.ps1`, `soak-android.ps1`, `release-check-android.ps1`, `release-key.ps1`, `find-wireless-adb.ps1`, `flash-audit.ps1`, `review.ps1`, `build-web.ps1`, `serve-web.ps1` | Copy, rename. `fetch-deps.ps1` clones SDL from `..\Glowtide\third_party\SDL` (same pinned commit) and takes the Khronos headers from the NDK. `review.ps1`: new picture sets (section 9.1). `serve-web.ps1`: drop `-MoveTo` |
| `scripts/simhash-android.ps1` | Copy, point it at `--sim tablehash` (section 9.4) |
| `android/` (Gradle project, manifests, the `github` manifest overlay, `game_mode_config.xml`, styles) | Copy, rename the package and the activity; icons regenerated in P6 |
| `android/.../ApkUpdater.java`, `ApkInstallReceiver.java`, the activity's `checkForUpdate` / `startUpdate` | Copy, rename the package, the repository and the asset names (`papermarch.apk`, `papermarch.apk.sha256`) |
| `.github/workflows/release.yml` | Copy, rename (job, asset names, secrets `PM_KEYSTORE_BASE64` etc.); it stays dormant until the repository exists (P3) |
| `web/shell.html`, `manifest.webmanifest`, icons | Copy, rename; remove the `#import=` save migration; paper-coloured loading screen |
| `tools/bake_font.py`, `contact.py`, `greyscale.py`, `pick_contrast.py`, `picture_compare.py`, `shrink_png.py`, `wav_metrics.py`, `flash_audit.py`, `find_adb_port.py`, `serve_web.py`, `web_check.mjs`, `make_icon.py` | Copy; `make_icon.py` redrawn in P6; `band_table.py` not copied (the bake prints its own tables) |
| `src/core/*` | Copy |
| `src/platform/*` | Copy |
| `src/gfx/gl.*`, `shader.*`, `target.*`, `text.*`, `font_atlas.inc`, `font_metrics.inc`, `debug_draw.*`, `frame_uniforms.h` | Copy |
| `shaders/prelude*.glsl`, `text.*`, `panel.*`, `debug.*`, `fullscreen.vert` | Copy; every other shader is new |
| `src/audio/mixer.*`, `synth.*`, `src/game/audio_out.*` | Copy; replace the pad music (`PadSet`, `musicStep`) with section 7's sequencer; new recipe table |
| `src/game/app.*`, `ui.*`, `save.*`, `input.h`, `bench.*`, `feedback.*` | Copy, adapt |
| `src/game/screens.cpp`, `game.*`, `pictures.cpp` | Copy as the skeleton (screens, modes, the flow test, the picture table), then rewrite the play parts. `Game::drawLevels` needs only `sim::bakedLevelCount()` and the paper colours |
| `src/game/view.*`, `hud.*` | New (sections 5, 6) |
| `src/gfx/renderer.*`, `mesh.*`, `camera.*`, `palette.*` | New (section 5); `palette.*` keeps its shape (a table and a lint) |
| `src/sim/*` | New (section 3). Nothing of GLOWTIDE's simulation survives |
| `src/tools/headless.cpp`, `tests.cpp`, `audio_render.cpp` | Copy as skeletons; `tests_sim.cpp` new |

Record in `PROGRESS.md` ("Forked from GLOWTIDE") what was copied and at which commit.

---

## 2. Build, scripts, app arguments

GT-TS 2 holds: one CMake tree; desktop by llvm-mingw clang and the native CMake; Android by Gradle 8.13 and AGP 8.13.0
with NDK 29.0.14206865; the browser by Emscripten from the shared emsdk; PowerShell scripts wrap all three and
dot-source `scripts\env.ps1` (4 cores, silent, off screen, the JDK, the tool paths).

- **The browser build exists from P0** and is part of every phase's exit (GLOWTIDE added it last and had to split the
  frame loop afterwards; here `App::frame()` is already that shape). One thread: nothing in this game needs a worker.
- App arguments: GLOWTIDE's set (GT-TS 2.5): `--scenario`, `--level`, `--bot`, `--picture`, `--screenshot`,
  `--frames`, `--strip`, `--strip-step`, `--seconds`, `--warmup`, `--res`, `--dp-scale`, `--safe`, `--set name=value`,
  `--silent`, `--no-audio`, `--offscreen`, `--world N`, `--fps`. `--bot` takes `best` (the solver's line), `greedy`,
  `random`. New: `--moves a,b,c` (a scripted list of site indices, for pictures and strips), `--speed x`.
- Exit codes and log tags as GLOWTIDE's, renamed.
- Headless tool `papermarch_headless`: `--solve --level N` (the best line, move by move), `--sim table` (every baked
  level: winnable, best, random and greedy shares, solver cost), `--sim tablehash`, `--bake-levels` (section 4),
  `--gen --recipe R --seed S` (one level as text), `--render-audio <name> --out x.wav`.

---

## 3. Simulation (`src/sim`)

No SDL, no GL, no audio, **no floating point**, no allocation, no clock. Everything is a plain struct; a move is a pure
function of (level, state). The whole library should be a few hundred lines.

### 3.1 Data

```cpp
namespace pm::sim {
constexpr int kMaxSites = 20, kMaxRoads = 40, kMaxRaids = 6, kMaxRoute = 8;
constexpr i64 kCap = 999'999'999;

enum class Kind : u8 { Home, Camp, Village, Shrine, Post, Mire, Depot, KeyTower, Keep, RivalCamp };
enum class LevelKind : u8 { Conquest, Gauntlet, Siege, Race, LastStand };

struct Site {
    Kind kind; u8 fork;          // fork group, 0 = none
    i16 x, y;                    // lattice position (section 5.2)
    i32 value; i16 perTurn; i32 lo, hi;   // current number = clamp(value + perTurn * turn, lo, hi)
    i16 until;                   // fading shrine: the last turn at full strength, -1 = never fades
    i32 aux;                     // depot: what it takes off the Keep
};
struct Road { u8 a, b, lock; };  // lock: the key tower's index, 0xFF = open
struct Raid { u8 turn, entry; i32 strength; };
struct LevelDef {
    LevelKind kind; u8 recipe, world, boss, siteCount, roadCount, raidCount, routeLen;
    i32 start, rivalStart;
    Site sites[kMaxSites]; Road roads[kMaxRoads]; Raid raids[kMaxRaids]; u8 route[kMaxRoute];
    i64 best; u16 hint;          // filled by the bake: the best final host; the hint line's string
};
struct State {
    i64 s, rival; u32 mine, burnt, rivals;   // bit masks over sites
    u8 turn, rivalStep, raidNext, flags;     // flags: won, lost, rivalAlive, loss reason
};
}
```

### 3.2 Functions

```cpp
State start(const LevelDef&);
i64   number(const LevelDef&, const State&, int site);     // GAME_DESIGN 3.1
u32   reachable(const LevelDef&, const State&);            // bit mask
u32   takeable(const LevelDef&, const State&);             // bit mask
void  take(const LevelDef&, State&, int site, Events*);    // GAME_DESIGN 3.3 and 3.4, the auto-advance included
```

`take` asserts the site is takeable. After the effect it runs the end of the turn (the rival's step, raids due), then,
while nothing is takeable and something is pending, further turns. It sets `won` or `lost`. `Events` may be null (the
solver passes null).

### 3.3 Events

A fixed array of at most 32 a move: `{type, site, a, b}` with types `Marched, Fought, Joined, Recruited, Multiplied,
Paid, Halved, RoadOpened, ForkBurnt, KeepCut, TurnPassed, SiteChanged (a timer's new number), RivalMoved, RivalTook,
RivalFell, RaidStepped, RaidHit, Won, Lost`. The view (section 6) and the audio turn each into clips and sounds. The
simulation never waits for them.

### 3.4 Undo

The game keeps a stack of `State` (at most 64; a level has at most 40 turns). Undo pops. `State` is about 40 bytes.

### 3.5 The solver

`Solve solve(const LevelDef&, const State& from)` returns `{winnable, best, firstMove, expansions}`: whether a win is
reachable from `from`, the largest final host over all winning lines, and the first move of one line that reaches it.

A depth-first search with a memo. The memo's key is everything in `State` but `s` (the masks, `turn`, `rivalStep`,
`raidNext`, the alive flag; `rival` follows from `rivals`), packed into a `u64`; the value is the largest `s` seen at
that key. On reaching a key with an `s` not above the stored one, prune: by GAME_DESIGN 3.5 nothing better can come of
it. Otherwise store and expand, trying moves in descending order of the `s` they give (it makes pruning bite early).

- The memo is a static open-addressing table of 2^18 entries (key, `s`): no allocation. A full table or more than
  `kMaxExpansions` (400,000) expansions returns "unproven"; the bake rejects such a level, so it cannot happen on a
  shipped one.
- **Budget on the floor phone: 8 ms a call** from a level's start (PLAN 5); the bake's acceptance line 6 uses a cost in
  expansions calibrated in P2 (measure microseconds per expansion on the Moto, record it).
- The game calls it once after each move (the dead-end nudge and the hint share the call), not every frame.
- **The check that keeps it honest**: a unit test generates 2,000 random levels of at most 8 sites with every element,
  and compares `solve` with a brute-force search over all orders. One mismatch fails the build.

### 3.6 Determinism

Integers only, so every platform agrees by construction. One check remains (it catches undefined behaviour and
compiler differences): `--sim tablehash` hashes every baked level's solve result; the PC, each phone and the browser
must print the same hash (section 9.4).

---

## 4. Generator and bake (`src/gen`, not linked into the app)

- **Recipes** (GAME_DESIGN 8.3): one function each, `(seed, params) → LevelDef` without numbers. All integer, one RNG
  (`core/rng.h`). Planar by construction: sites sit on a lattice, roads only between lattice neighbours of the
  recipe's template.
- **Numbers**: the forward walk of GAME_DESIGN 8.3, then traps.
- **Acceptance**: GAME_DESIGN 8.4, in the order cheapest first (solver, random and greedy plays, the decision check,
  the fingerprint, the layout lint through `layoutLint()` of section 5.2, which is pure arithmetic and lives in `sim`).
- **The bake**: `papermarch_headless --bake-levels --from 9 --to 300 --jobs 4 --out src\sim\levels_baked.h --report
  captures\levels\report.md`. For each level: the calendar gives the kind and the elements allowed; try seeds until one
  is accepted (at most 400; a level that finds none fails the bake loudly with its closest miss). Hand-made levels
  (`src/sim/levels_hand.cpp`) go through the same acceptance minus the band lines. The whole bake should take seconds
  to a few minutes on 4 cores; record the time.
- **The table** holds complete `LevelDef`s (about 0.5 KB each, 150 KB in all). The app never generates: a later
  change to a recipe cannot change a shipped level, and a re-bake of some levels keeps the others byte for byte
  (`--keep-to`, as GLOWTIDE's bake has).

---

## 5. Renderer (`src/gfx`, new)

OpenGL ES 3.0 core (GT-TS 5.1's rules: shaders validated by glslangValidator at build time and embedded, `mediump`
fragment shaders unless a value needs `highp`). Top-down, 2D, no depth buffer, no 3D camera, no bloom, no per-frame
off-screen pass.

### 5.1 The sticker

One instanced shader draws every paper shape that is not the page: `shaders/sticker.vert/.frag`. An instance is a
quad: position, half size, rotation, a shape id, two colours (the fold's light and shaded halves), a softness (the
edge's width in pixels: 1 for a crisp cut, 6 to 10 for a shadow) and two free parameters (a flip's width scale, a
flag's wave phase). The fragment shader evaluates the shape's signed distance and writes colour with an analytic edge:
no MSAA.

Shapes (each a few lines of distance arithmetic): rounded box, ellipse, isosceles triangle, the pawn (a circle and a
bell), the rider, the giant, a flag (with a sine wave from the parameter), a star, a ring or arc, a capsule (roads,
river segments). Select the shape with an `if` chain on the flat shape id; no constant arrays indexed by a variable
(section 10). Pop-ups are small groups of stickers (a tent is three triangles and a flag; the Keep about sixteen
boxes and triangles), built by `gfx/popups.cpp` from one table.

Instances are sorted by `y` each frame (far to near; a few hundred to at most 1,500: an insertion sort over a nearly
sorted array) and drawn in one call. Tags and their text come after every sticker; then confetti; then the UI.

### 5.2 The map's frame, and the layout lint

The page is 100 × 160 map units (Gauntlet: up to 100 × 420, scrolled). `gfx/camera` here is only a 2D fit: the page
is scaled to fit between the top band (96 dp under the safe area's top) and the bottom band (150 dp), centred, never
wider than the screen minus 12 dp; the table shows wherever the page does not reach. The eight aspect ratios of
GLOWTIDE's matrix (9:22 to 3:4) must all hold.

`sim::layoutLint(const LevelDef&, frame)` is pure arithmetic: every tag's rectangle (from the number's digit count)
clear of every other tag and of the HUD bands; sites at least 56 dp apart on the smallest frame; no road passing
within 10 units of a site that is not its end. The bake and the tests call it; the game never needs to.

### 5.3 The page bake

Once a level, into a page-sized RGBA8 texture (and a half-resolution R8 texture for the pop-ups' shadows):

1. The table's wood is **not** in it (step 1 of 5.4 draws it).
2. The cream page; the land (one or more blobs: `shaders/blob.frag` evaluates a superellipse outline with three sine
   wobbles, as `make_concept.py`'s `blob()`; each blob is drawn twice, first offset and soft as its own shadow, then
   crisp); the terraces the same way.
3. The river and the roads as chains of capsule stickers (the river with a dark upper edge instead of a shadow: it is
   cut out, not glued on), the stitches, the sites' sand patches.
4. Paper grain and the vignette, from a hash of the pixel position.
5. Into the shadow texture: every static pop-up's stickers, offset down and right, softness 8.

All edges are analytic, so there is no blur pass. Budget: the bake may spread over up to three frames (land; roads and
river; shadows); **no frame over 50 ms on the floor phone**. Measure it in P1.

### 5.4 A frame

1. The table: a full-screen quad, a cheap wood shader (two colours, plank lines, grain).
2. The page: one textured quad, with its drop shadow as a soft sticker under it.
3. The pop-ups' shadow texture, multiplied in at a uniform strength (0 → 1 during the unfold).
4. Moving shadows: the figures', flags' and tags' stickers once more, offset, one flat dark colour at 35%, softness 3
   (one extra instanced draw of a subset).
5. Cloud shadows and river glints: a handful of large soft stickers drifting (8% dark; light ripples).
6. All stickers, sorted (pop-ups and figures together, so a pawn can walk behind a tent).
7. Tags and their text (GLOWTIDE's signed-distance text).
8. Confetti: GLOWTIDE's stateless particles (written once at spawn, moved in the vertex shader), each a small two-tone
   rectangle that tumbles (the second colour shows as it turns).
9. The UI layer.

### 5.5 The page turn

At the start of a turn, the finished frame of the old page (steps 2 to 8) is drawn once into a page-sized texture.
For 600 ms the new page is drawn normally, and over it the old page as a strip mesh of 48 segments whose vertex shader
wraps everything past a moving line around a cylinder (the classic page curl: the line sweeps from the bottom right to
the top left, its radius 12% of the page's width); the curl's back face shows the page's cream at 92% brightness, and
a soft dark sticker along the line is its shadow on the new page. With "reduce effects": a 250 ms slide.

The unfold: each static pop-up has a rise time from its `y` (top of the page first, the Keep last, the wave 400 ms
long and 400 ms behind the curl); a rising pop-up scales its height from 0 with a 12% overshoot over 250 ms.

### 5.6 Quality

One level of quality, as GLOWTIDE (GT-TS 5.10): no settings, native resolution. The 60 Hz fallback logic comes with
the fork.

### 5.7 UI

GLOWTIDE's layer (GT-TS 5.11) restyled: cream cards, gold and ivory buttons, the pressed state of GAME_DESIGN 11.
The layout lint and its eight frames come with it.

---

## 6. The view (`src/game/view`): the animation timeline

The simulation's state is always the truth; the view shows a recent past and catches up.

- A **clip** is `{type, start, duration, site, a, b}` on the view's own clock. `take()`'s events are turned into clips
  laid end to end by a table (GAME_DESIGN 10): `Marched` → the banner's path and the column's wave; `Fought` and
  `Joined` → lean in, clash, the flip wave, the count-up; and so on.
- The view keeps **shown** values (the host's number on the plate, each site's colour and garrison, the banner's
  position) and each clip moves them. Tags of sites show the simulation's truth at once only for takeability after the
  queue drains; while clips run, tags show the shown state, so a number never changes before its animation.
- Input: a tap is tested against the simulation's current state (so a fast player is never blocked), the move is made,
  its clips are appended. More than two moves' clips queued: the tap is refused quietly. With a move queued the clock
  runs at 2.5 ×.
- Undo and restart: drop every clip, snap the shown state to the simulation's, play one 200 ms "paper settles" clip.
- The host's column: figure `k` follows the banner's path `k × 0.6` units behind it (a ring buffer of the banner's
  recent positions), three abreast, with a per-figure hop phase.
- The view allocates nothing after a level loads: fixed arrays for clips (64), figures (the caps of GAME_DESIGN 9.3),
  stickers (2,048), confetti (600).

Scripted pictures and strips (section 9.1) drive the same view from `--moves`, at fixed times, so a strip of a battle
is reproducible to the pixel.

---

## 7. Audio

GLOWTIDE's audio (GT-TS 6): an SDL audio stream, the mixer's voices of sounds baked at start-up, one lock-free queue
from the game, a soft limiter. `Mixer::play(sound, gain, pan, pitch, delay)` already takes a pitch ratio and a delay.

- **The sound bank**: one recipe a row (GAME_DESIGN 12.2), baked at start-up with GLOWTIDE's cheap oscillators
  (rotation, multiplied decays: its PROGRESS, "Synthesis cost"). Budget: the whole bank in at most 150 ms on the floor
  phone, on the main thread in the browser.
- **The sequencer** replaces GLOWTIDE's pad music: on the audio thread, a sixteenth-note clock from the world's tempo;
  per layer a pattern of 128 steps (8 bars of 2/4) of `{sound, degree, gain}`; a step starts a voice with the pitch
  ratio of its scale degree in the world's mode. Layers have a gain that slews to 0 or 1 over one bar; the game sends
  only "layers wanted" (from `r`, GAME_DESIGN 12.1) and "world". Patterns are constant tables (the theme, the bass
  line from the chords, the drum rudiments).
- **On the beat**: the game may ask for a sound "at the next sixteenth" (the clash, the stars); the audio thread holds
  it until the step. Nothing the finger does directly is delayed (taps, refusals).
- **The count ladder**: the game sends each tick with its ladder step; the audio thread turns the step into a pitch
  ratio from the current chord's root in the world's scale.
- Voices: raise `kVoices` to 48 if the busiest render (a shrine's count-up over full music) steals voices; measure.
- `--render-audio` names: `ladder`, `battle` (a scripted busy fight over full music), `ui`, `win`, `perfect`, `lose`,
  `turn` (the page turn), `music --world N [--layers k] [--boss]`, `theme-a|b|c`. `tools\wav_metrics.py` gives peak,
  loudness and the share of energy above 300 Hz.

---

## 8. Game layer

- **States**: GLOWTIDE's `Screen` (Home, Play, Pause, Win, Lose, Settings, Levels) and `Mode` (Play, Picture, Bench,
  Flow). `Game` holds the `LevelDef` pointer, the `State`, the undo stack, the last `Solve`, the view.
- **Save** (GLOWTIDE's text file, `key=value` lines, the same loader with its range checks): `version`, `level`,
  `stars` (300 digits), `sound`, `music`, `haptics`, `fast`, `reduce_effects`, `refresh60`, `hints` (the bit mask of
  hint lines already shown). On Android in SDL's preference path; in the browser mirrored to localStorage by
  `platform::persistFile` (keys `papermarch:*`). A level in progress is not saved: a level is a minute.
- **Lifecycle**: as GLOWTIDE (pause on focus loss, GL objects rebuilt from CPU state, the page re-baked).
- **Tweakables**: GLOWTIDE's `Tuning` and `--set`: `star2Pct`, `star3Pct`, `deadNudge`, every timing of GAME_DESIGN
  10, the figure caps, the shadow strengths, the layer thresholds.
- **The updater**: unchanged from GLOWTIDE (`platform::checkForUpdate`, `latestBuild`, `startUpdate`,
  `updateProgress`; HOME's pill). It does nothing in a build without a build number and in the browser.

---

## 9. Verification and performance

### 9.1 Pictures, strips, scenarios

`--picture <name>` renders a scripted, reproducible frame; `--strip N` a sheet of frames; `scripts\review.ps1 -Set`
runs a set and builds contact sheets.

| Set | Pictures |
|---|---|
| `p1` | `concept` (level 12 after two moves, the concept frame's twin), `elements` (every site kind with its tag in each state), `figures` (hosts of 5, 40, 400, 4,000, 40,000 and 400,000), `tags_dense` (a 20-site map), `world_1` to `world_3`, strips `battle`, `shrine`, `page_turn`, `unfold`; the aspect matrix (`concept` at the eight frames) |
| `p2` | Levels 1–8 at their start; strips of each element's clip; `lose_stranded`, `dead_end` |
| `p3` | Every screen at the eight frames, with the layout lint (`-Lint`) |
| `p5` | `worlds` (ten worlds, and the two palette candidates not picked for each), `kinds` (one level of each kind), `bosses`, `levels` (thirty consecutive levels as thumbnails: `-From N`) |
| `p6` | `win`, `perfect`, `boss_win`, `lose` strips for the flash audit; `home_update` |

Scenarios: `bench_idle` (a level at rest), `bench_battle` (PLAN 5), `bench_play` (`--bot best` through consecutive
levels with every clip and page turn).

### 9.2 Benchmarks

GLOWTIDE's (GT-TS 8.3): the panel-side present timestamps, the contract's two numbers, CPU phases, heap allocations a
frame, one JSON line under `PMBENCH`. `bench-android.ps1` handles the lock, the foreground checks and the grading.

### 9.3 The browser check

`node tools\web_check.mjs http://localhost:8092/ captures\web flow|play|reload`: headless, muted Chrome at an iPhone's
size with touch: the flow test, a level played by touch with its frame rate, the save across a reload, the safe area.

### 9.4 Tests and lints (`scripts\test.ps1`; all must pass before a phase closes)

- Rules: one test an element and a property (each row of GAME_DESIGN 5.1, forks, locks, timers, the rival, raids, the
  auto-advance, each way to lose), the hand-made levels' stated results (GAME_DESIGN 8.1 and section 2: 285 and 396).
- The solver against brute force (section 3.5).
- The table: every baked level winnable, `best` equal to a fresh solve, acceptance lines 1, 3, 4 and 5 hold.
- `tablehash` equal on the PC, each phone (`scripts\simhash-android.ps1`) and the browser.
- The palette lint (GAME_DESIGN 9.2); the layout lint on every screen and every level at the eight frames.
- The flow test (GLOWTIDE's, its checks rewritten for this game's screens: HOME → PLAY → a win by `--bot best` → NEXT;
  a strand → UNDO; HINT caps the stars; LEVELS picks a level; SETTINGS toggles persist).
- Shader validation and the "no GL errors" exit code.
- The flash audit on the `p6` strips.

### 9.5 If the contract is missed (in this order, one step at a time, measuring after each)

1. Moving shadows only for figures (drop flags' and tags').
2. Cloud shadows off.
3. Confetti budget 600 → 300.
4. Figure caps down by a third.
5. The page curl's mesh 48 → 24 segments and the captured page at half resolution.
Resolution stays native. Report to the owner before anything further.

---

## 10. Traps

GLOWTIDE's lists hold: its TECH_SPEC 10 and VARIETY_TECH 12. The ones most likely to bite here:

- On this PC `cmake` and `git` on PATH are MSYS2 builds: call the native ones by full path (`env.ps1` does).
- Python patch scripts on Windows write CRLF and mangle backslashes in non-raw strings; a bash heredoc into `python -`
  with no input can hang the shell tool.
- A phone that sleeps locks, and `am start -W` then hangs: the device scripts keep it awake; keep that code.
- On the OnePlus 12R's GPU a shader constant array indexed by a non-constant drew nothing while every check passed:
  use `if` chains (section 5.1).
- `mediump` bites on the tablet's Mali first: distances in the sticker shader are in quad-local units of a few, never
  in pixels of thousands; look at the tablet's capture after every shader change.
- iOS Safari starts audio only inside a gesture and keeps a Home Screen app's storage apart from Safari's
  (`web/shell.html` already resumes the audio on every touch).
- In the browser there is one thread: the sound bank bakes on the main thread, so its budget is a start-up budget.
- `i64` arithmetic with `kCap`: clamp after every effect; `×3` of 999,999,999 must not overflow a narrower type on the
  way (the brute-force test includes hosts near the cap).
