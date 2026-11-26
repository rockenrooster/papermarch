# PAPERMARCH — Master Plan

Status: written 2026-11-26 by a planning session for a different executor (a coding agent). Nothing is built: only this
`docs/` folder and a stub `CLAUDE.md` exist. The decisions in section 2 are the planner's proposals; the owner has not
confirmed them. Phase P0 starts by getting that confirmation.

| Doc | Purpose | When to read |
|---|---|---|
| `PLAN.md` (this) | Decisions, environment, what is forked from GLOWTIDE, performance contract, phases, working rules | Start of every session |
| `GAME_DESIGN.md` | What the game is: rules, elements, kinds, levels, look, juice, screens, sound | Sections named by the phase |
| `TECH_SPEC.md` | How it is built: the fork, simulation and solver, renderer, animation, audio, tooling | Sections named by the phase |
| `subplans/P0_fork_bootstrap.md` … `P7_release_web.md` | One work order per phase: tasks, files, exit checks, owner checkpoint | The phase you are executing |
| `concept/concept_gameplay.png` | The look target (rendered by `concept/make_concept.py`; its palette and layout numbers are the starting values) | P1, and whenever a screenshot is judged |
| `concept/check_levels.py` | A toy model of the rules: brute-forces the hand-made levels and asserts the results GAME_DESIGN states | P2, to compare with the real simulation |
| `PROGRESS.md` | Create in P0. Phase status, measured numbers per device, deviations, choices made for the owner, open questions | Start of every session; update at the end |

---

## 0. Executor briefing

1. **You cannot see motion or hear audio.** Verify with numbers, screenshots and frame strips. The owner is the eyes,
   ears and thumbs. Never report that something "looks", "sounds" or "feels" good unless the owner said so; report what
   you measured and what still needs their judgment.
2. **No subagents.** The owner asked for this plan to be executed by one coding agent working directly, phase after
   phase. Do not spawn coding, review or research subagents.
3. **Fork, do not rebuild.** GLOWTIDE (`..\Glowtide\`, the owner's previous game) already has the engine shell this game
   needs: the build for Windows, Android and the browser, the app loop, UI, text, save, mixer, the level selector, the
   in-app updater, the release pipeline, the device scripts. Section 4 lists what to copy, what to adapt and what to
   replace. Copy first, rename, delete what this game does not use, and only then write new code. Never edit, build or
   commit inside `..\Glowtide\` or `..\Raveform\` (the shared device lock folder is the one exception, section 3).
4. **Keep it small.** Before writing anything, ask in this order: does it need to exist; is it already in this repo or
   in the fork; does the standard library or the platform do it; can it be one line. No interface with one
   implementation, no configuration for a value that never changes, no scaffolding for later. A deliberate shortcut with
   a known ceiling gets a `// ceiling:` comment naming the limit and the upgrade path.
5. **De-risk first.** Two things can sink this: a look that is not beautiful (P1 gate) and a core that is not fun or is
   not the game the owner meant (P2 checkpoint). Nothing else is built on top until both have the owner's word.
6. **Specs are starting points, the contract is not.** Numbers in `GAME_DESIGN.md` and `TECH_SPEC.md` are initial
   values; measurement and the owner's play override them. Log each deviation in `PROGRESS.md` with the reason. Section
   2 (decisions) and section 5 (performance contract) change only with the owner's explicit OK.
7. **Originality.** PAPERMARCH is an original game in the pick-your-fights genre (an army with a number takes on
   smaller numbers and snowballs). The owner's reference is how advertisements for mobile strategy games play
   (GAME_DESIGN 1); it is a reference for feel only. Do not use any existing game's name, characters, art, sounds, user
   interface, level layouts or trademarks, and do not rebuild any one of them under a new name. The theme (a pop-up
   paper war map), the ivory host, the palette and every name in GAME_DESIGN are ours. It must also look nothing like
   GLOWTIDE: no dark backdrop, no glow, no bloom.
8. **Best judgment over questions.** On GLOWTIDE the owner asked for best judgment instead of questions. Outside the
   gates named in section 7, decide, record the choice in `PROGRESS.md` ("Choices made for the owner") and move on;
   collect what only the owner can judge (fun, ears, looks) in the checkpoint lists.
9. **Permissions.** Downloading anything needs the owner's explicit yes in the current session (name, source, version,
   licence, size first); the plan needs no download. Creating the GitHub repository, pushing, creating the Netlify
   project and every production deploy need the owner's yes (sections 2 and 7). Keystores and passwords are the owner's
   to handle: the owner runs `scripts\release-key.ps1`, never you. No system settings or system environment variables.
10. **The owner's standing rules.** Short chat replies. Prefer `rg`. Small focused changes. No destructive git or shell
    commands unless asked. Do not revert unrelated changes. A manual backup before major or breaking changes (use
    `D:\Stuff\DevFolder\_backups\Papermarch\`). Builds and tests only when needed to verify. Measure before optimizing.
    UI readable, space-efficient, with clear high-contrast pressed states.
11. **PC rules.** Everything run on this PC is **silent** (`--silent`), **off screen** (`--offscreen`) and uses **at
    most 4 CPU cores** (`scripts\env.ps1` confines the process tree, as GLOWTIDE's does). The owner is at the machine.
12. **The test devices are shared** with other agent sessions running at the same time. Section 3 is to the letter.

---

## 1. What we are building

PAPERMARCH is a one-thumb, portrait Android and browser game. Each level is one page of a pop-up paper war map. Your
host, a little crowd of ivory paper soldiers under a scarlet banner, carries a number. Every site on the map carries a
number too. Tap a site your host can reach: if your number is at least theirs, the garrison flips to your colour and
joins you, and your number grows by theirs. Pick the fights in the right order, snowball from a dozen soldiers into a
horde, and take the Keep. A level lasts twenty to sixty seconds. Then the page turns and the next map pops up.

That is the whole game. No base building, no timers to wait on, no currencies, no upgrades to grind, no
advertisements, no pop-ups: the owner asked for the game the advertisements show, not the game those products turn out
to be.

Pillars, in priority order:

1. **The snowball.** Growth is visible, countable and loud: 10 becomes 24 becomes 144 becomes 408, the column of
   soldiers gets longer, the count-up climbs a scale, the music adds a layer.
2. **One thumb, no friction.** Playing within two seconds of launch. Tap a site, watch a second of spectacle, tap the
   next. Undo is one tap and unlimited. Retry and next level are one tap.
3. **Beautiful and readable.** Bold cut paper in daylight colours with soft shadows; every number legible at arm's
   length; which sites can be taken now is obvious without reading.
4. **A real choice every move.** Which fight first, when to spend the multiplier, whether to let a growing camp ripen,
   which fork to burn. Winning is easy; three stars takes the best order, and the solver knows what "best" is.
5. **Never drops a frame.** 60 FPS at native resolution on the floor phone, measured in every phase.

---

## 2. Decisions (proposed by the planner; the owner confirms or changes them in P0)

| Topic | Decision | Reason |
|---|---|---|
| The reference | The planner could not find a description of the advertisements the owner means (three web searches, 2026-11-26), so the design covers the three things such advertisements show with one rule set: **pick your fights** on a map (the core, most levels), **the gate run** (the Gauntlet kind: left or right, + or ×) and **hold the line** (the Last Stand kind) | Whichever one the owner meant is in the game; the P2 checkpoint asks which should lead |
| Core | Turn-based, deterministic, integers only. A move is one tap. Too-strong sites cannot be tapped into a loss; you lose only by running out of moves or to a raid or rival you did not outgrow | The advertisements are a sequence of taps; determinism makes an exact solver possible, and the solver replaces bots, balance guesswork and "impossible level" reports |
| Name | PAPERMARCH (working title) | One web search on 2026-11-26 found no game or app with this name (not a trademark clearance) |
| Theme | A pop-up paper war map on a wooden table, in daylight. The player's host is ivory with scarlet and gold; the enemy is ink navy with one accent colour a world. Ten worlds, ten palettes | Original; drastically unlike GLOWTIDE's glowing twilight (the owner's condition); flat paper with soft shadows is cheap to draw and bold; see `concept/concept_gameplay.png` |
| Orientation, input | Portrait only. One thumb: tap a site (or drag from the banner to it) | The genre's advertisements are portrait and one-handed |
| Engine | GLOWTIDE's, forked by copy: C++20, SDL 3.4.20, OpenGL ES 3.0, one CMake tree, Gradle for the APK, Emscripten for the browser. A new 2D renderer and a new simulation; everything else adapted | Proven on this PC and these devices, fully code-driven, already has the selector, updater and web port the owner asked to reuse |
| Targets | Android arm64 (minSdk 29, targetSdk 36) and the browser (WebAssembly, WebGL 2, built and checked in every phase from P0). Windows desktop build for development only | The owner asked for an APK and a web version |
| Level selector, updater | GLOWTIDE's, copied: HOME's `LEVEL N` chip opens LEVELS (every level open, stars under each); a GitHub release per push to `main` and the in-app UPDATE pill | The owner's words: "use the same level selector and updater" |
| Repository | `git init` locally in P0. A **public** GitHub repository `rockenrooster/papermarch` only when the owner says yes (asked in P3); the release key is made by the owner **before** the first public build, so every build can be updated | The updater needs public releases; GLOWTIDE's first 23 builds could not be updated because the key came late |
| Web hosting | Netlify project `papermarch` on the owner's free account, **once, at the very end (P7), on the owner's explicit yes**. Until then: this PC (port 8092) and free draft deploys | The owner's words: "published to netlify when fully complete". A production deploy costs 15 of the account's 300 monthly credits, shared with three other sites |
| Performance | 60 FPS P95 at native resolution on the Motorola One 5G Ace; the same picture at a higher refresh rate on stronger devices. One quality level, no graphics settings | The owner's standard on GLOWTIDE and RAVEFORM |
| Scope | 300 levels: 8 hand-made, the rest generated, every one proven winnable by the solver with its best score known. Ten worlds. Persisted: level reached, stars, settings. No meta game | "Plays like the ads" |
| Money, network | No advertisements, purchases, analytics or accounts. The INTERNET permission only for the update check, as in GLOWTIDE | Sideloaded APK and a static web page |
| Assets | Procedural: shapes, pop-ups, effects, sounds and music are generated in code. The font is GLOWTIDE's baked atlas (Roboto Black from the Android SDK, Apache 2.0, corners rounded at bake time) | No art pipeline, no download, tiny APK |
| Dependencies | SDL and nothing else in the app | As GLOWTIDE |

---

## 3. Environment

`..\Glowtide\docs\PLAN.md` section 3 is the verified description of this PC, its toolchain traps and the test devices
(checked 2026-11-19; the browser toolchain was added 2026-11-24). It holds for this project. Read it in P0 and re-check
the versions it names (`scripts\env.ps1` does most of it). What differs or was added since:

| Item | Fact |
|---|---|
| Emscripten | 7.0.1, the emsdk in `D:\Stuff\DevFolder\emsdk`, shared with RAVEFORM and GLOWTIDE: **only read it** |
| Ports on this PC | 8080 RAVEFORM's web server, 8091 GLOWTIDE's. PAPERMARCH uses **8092**. Opening it to phones needs a Windows Firewall rule, which takes an administrator and the owner's yes; headless checks use `localhost` and need none |
| Netlify | The CLI is `npx -y netlify-cli@27.20.1`, already signed in (Windows Credential Manager), telemetry off. Draft deploys are free; `--prod` costs 15 credits and needs the owner's yes each time |
| Test devices | Floor phone: Motorola One 5G Ace `ZY22BXMJG3` (the contract is graded here). Tablet: Galaxy Tab A11+ `R5GL362BEHX`. **Ask the owner before using the OnePlus 12R** (`a0e622a5`; left out of testing since 2026-11-20) **or the Galaxy S25 Ultra** (`R5CYA2CTX2Z`; it has a Secure Folder, so `pm` and `adb install` need `--user 0`). Pairing codes are the owner's to type |
| Git | `C:\Program Files\Git\cmd\git.exe` with `-c http.sslBackend=schannel`; for a push the full command is in `..\Glowtide\CLAUDE.md` (Conventions) and GLOWTIDE's memory of it: `-c http.sslBackend=schannel -c http.sslcainfo= -c credential.helper= -c "credential.helper=!gh auth git-credential" push origin main` |
| Fork point | GLOWTIDE `main` at commit `dff405a` (2026-11-25). Copy from the working tree of the `main` branch; never from `margin-pass` |

**Shared test devices** (the owner's rule; other sessions use the same phones). GLOWTIDE PLAN 3.3 applies word for
word with this project's names:

1. Nothing device-wide: no `adb kill-server` or `start-server`, no reboot, no `adb logcat -c` (read with `logcat -d
   --pid=<our pid>`), no settings changes, and no clear, uninstall or force-stop of any package but
   `dev.papermarch.game` (and its `.dev` twin).
2. Every adb command passes `-s <serial>`.
3. Before launching, benchmarking, sending key events or taking a screenshot, check the foreground activity. If an app
   that is neither the launcher nor ours is in front, someone else is using the device: wait and re-check, or use
   another device. Never start our activity over someone else's app.
4. Locks: before using a device, write `D:\Stuff\DevFolder\Android_Games\Raveform\captures\locks\<hardware
   serial>.lock` containing `papermarch-<phase> <date and time>`, and delete it when the run ends. A lock under 30
   minutes old whose first word is not yours means the device is busy. GLOWTIDE's device scripts already do this; keep
   that code when you copy them. Never stop a device script mid-step: a stale lock blocks every session.
5. Keep device runs short; after any run `adb -s <serial> shell am force-stop dev.papermarch.game`.
6. A benchmark is valid only if PAPERMARCH kept focus for the whole run. Numbers come from shared devices: say so.
7. During long adb work keep the screen awake yourself the way GLOWTIDE's scripts do; do not ask the owner.
8. Phones keep their sound; only PC runs are silent.

---

## 4. Architecture, and what comes from GLOWTIDE

```
            ┌──────────────────── main thread ─────────────────────┐      ┌─ audio thread (SDL callback) ─┐
 touch ───▶ │ events → input → sim (one move at a time, integers)   │ notes│ sequencer → voices → limiter  │──▶ device
            │   → sim events → view timeline (march, battle, flip)  │ ───▶ │ (no locks, no allocation)     │
            │   → build sticker instances → GL draw → swap          │      └───────────────────────────────┘
            └───────────────────────────────────────────────────────┘       one lock-free queue, game → audio
```

The simulation is instant: a move changes the state in microseconds and emits events. The view plays those events as
a timeline of short clips (march, clash, flip-and-join, count-up) over about a second. Rules and pictures never share
state beyond the event list, so the solver, the tests and the bake run the same rules with no view at all.

| Layer | Choice |
|---|---|
| Language | C++20, clang on every platform, no exceptions, no RTTI, warnings are errors in our code |
| Platform | SDL 3.4.20; Android through `SDLActivity`; the browser through Emscripten (one thread) |
| Simulation | `src/sim`: no SDL, no GL, no audio, no floating point. The level, the rules, undo, the solver, hints |
| Generator and bake | `src/gen` (linked into the headless tool and the tests only): recipes, numbers, acceptance. The app ships complete baked levels, not seeds |
| Renderer | OpenGL ES 3.0. Top-down and 2D. The static page (land, river, roads, pop-ups' shadows) is baked into a texture once a level; everything that moves is an instanced "sticker" (a quad whose fragment shader draws one paper shape with an analytic edge). No bloom, no 3D camera |
| Audio | GLOWTIDE's mixer and synth primitives; new recipes; a small step sequencer whose layers follow the host's strength |
| UI | GLOWTIDE's layer (rounded rectangles, signed-distance text, dp units, safe area), restyled as paper |
| Tooling | GLOWTIDE's: scenarios, scripted pictures, frame strips, the flow test, the layout lint, the headless tool, the offline audio renderer, the device scripts, the browser check |

What happens to each part of GLOWTIDE (TECH_SPEC 1 has the file-level list and the rename map):

| GLOWTIDE | Here |
|---|---|
| `scripts\`, `cmake\`, `CMakeLists.txt`, `android\`, `web\`, `.github\workflows\release.yml`, `tools\*.py`, `tools\web_check.mjs`, `.gitignore`, `.gitattributes` | **Copy and rename** (`glowtide` → `papermarch`, `GT_` → `PM_`, port 8091 → 8092). Delete what belongs to GLOWTIDE's levels (bake and recipe commands, `simhash`, the `-MoveTo` save migration) |
| `src\core\`, `src\platform\` (haptics, frame timestamps, the updater, the browser glue), Java `ApkUpdater`, `ApkInstallReceiver`, the activity | **Copy and rename** |
| `src\gfx\gl*`, `shader*`, `target*`, `text*`, the font atlas and metrics, `debug_draw*` | **Copy** |
| `src\audio\mixer*`, `synth*`, `src\game\audio_out*` | **Copy**; the recipes and the music are new (GAME_DESIGN 12) |
| `src\game\app*`, `ui*`, `save*`, `input*`, `bench*`, `feedback*`, `pictures*`, `screens.cpp` (HOME, PAUSE, results, SETTINGS, **LEVELS**, the flow test) | **Copy and adapt**: same flow, new look, new HUD, new checks |
| `src\gfx\renderer*`, `mesh*`, `camera*`, `palette*`, every shader but the text and panel ones | **Replace** (TECH_SPEC 5) |
| `src\sim\` (world, bots, bake, recipes, calendar, fingerprint) | **Replace** with a far smaller simulation (TECH_SPEC 3 and 4) |
| `src\game\view*`, `hud*`, `game*` | **Replace**, keeping `Game`'s screen and mode skeleton |

Repository layout:

```
Papermarch/
  CLAUDE.md            real commands and conventions (rewrite in P0)
  docs/                PLAN.md, GAME_DESIGN.md, TECH_SPEC.md, subplans/, concept/, PROGRESS.md, LICENSES.md, RELEASE_PIPELINE.md
  src/core/ sim/ gen/ gfx/ audio/ game/ platform/ tools/
  shaders/  assets/font/  tools/  android/  web/  scripts/  cmake/
  third_party/SDL/     local clone of the pinned SDL (git-ignored; clone it from ..\Glowtide\third_party\SDL, no download)
  captures/            screenshots, strips, benchmark results (git-ignored)
```

---

## 5. Performance contract

The definition is GLOWTIDE's (its PLAN 5): over a 60-second run after a 3-minute warm soak, frames timed on the panel
side from the display's present timestamps, a frame is **late** when it was shown two or more refresh periods after the
one before it. The contract is two numbers: the **late share**, at most 5%, and the **worst frame**, at most 50 ms.

The graded scene is `bench_battle` (TECH_SPEC 9.1): a 20-site map with every garrison at its drawn cap, two full hosts
in a flip wave, confetti at its budget, a page turn every eight seconds. This game is far lighter than GLOWTIDE; the
contract is here to catch regressions and the two expensive moments, the page bake and the page turn.

| Device | Refresh target | Required |
|---|---|---|
| Motorola One 5G Ace | 60 Hz | Late share ≤ 5%, worst frame ≤ 50 ms — **the gate** |
| Galaxy Tab A11+ | 90 Hz when it holds, else 60 Hz | The same at 90 Hz, or at 60 Hz with the reason recorded |
| The browser (headless Chrome at 390 × 844 CSS px, 3×) | — | At least 55 frames a second in `tools\web_check.mjs play`; reported, not a gate |

| Budget on the floor phone | Value |
|---|---|
| Main thread CPU | ≤ 4 ms P95 |
| GPU | ≤ 9 ms P95 |
| A level's page bake | Spread so that no frame exceeds 50 ms (TECH_SPEC 5.3) |
| Solver, one call on the heaviest shipped level | ≤ 8 ms on the floor phone (it runs after each move, for the hint and the dead-end nudge) |
| Heap allocations per frame | 0 in steady state |
| Memory / APK | < 200 MB RSS / < 15 MB |
| Start-up | Cold start to an interactive HOME ≤ 2 s on the floor phone |

Every phase ends with the benchmark on the floor phone and the tablet and the browser check; numbers go in
`PROGRESS.md`. A regression blocks the phase: the late share up by more than two points, or main-thread CPU P95 up by
more than 10%, against the previous phase's run.

---

## 6. Assets

Everything is procedural and lives in code or build-time scripts: paper shapes (fragment shader), the page (baked from
the level), confetti, every sound and the music, the app icon (`tools/make_icon.py`, redrawn for this game).

The font is GLOWTIDE's baked atlas, copied with its metrics; `docs/LICENSES.md` gets its row (Roboto Black, the Android
SDK, Apache 2.0) and SDL's. The credits screen is generated from that file, as in GLOWTIDE.

---

## 7. Phases

Sizes are relative (S, M, L). Every phase ends with: the benchmark on the floor phone and the tablet, the browser
check, an update to `PROGRESS.md`, a short note to the owner saying what to try, and the owner checkpoint. While waiting
for the owner at a soft checkpoint, record your own choice and continue; at a **gate**, continue only with work that
does not depend on the answer.

| Phase | Size | Needs | Outcome | Subplan |
|---|---|---|---|---|
| P0 Fork and bootstrap | M | — | The fork builds and runs on the PC, a phone and the browser with GLOWTIDE's game removed: an empty page, the UI layer, pictures, strips, benchmarks | `subplans/P0_fork_bootstrap.md` |
| P1 Look proof — **gate** | L | P0 | The concept frame drawn by the engine, a battle strip and a page-turn strip, inside the contract on the floor phone. The owner approves the look | `subplans/P1_look_proof.md` |
| P2 Rules and first playable — **gate** | L | P1 | The simulation, the solver, undo, tap input, the march and battle clips. Levels 1–8 playable on a phone and in the browser | `subplans/P2_rules_first_playable.md` |
| P3 Game loop, selector, updater | M | P2 | HOME, HUD, pause, results, settings, LEVELS, save, hints, haptics, lifecycle; the public repository, the release pipeline and the UPDATE pill (on the owner's yes) | `subplans/P3_game_loop_selector_updater.md` |
| P4 Audio | M | P2 | The sound set, the count ladder, the layered march, three worlds' music | `subplans/P4_audio.md` |
| P5 Content | L | P3 | Every element, the five kinds, the generator, the solver-driven bake, 300 levels, ten worlds, bosses | `subplans/P5_content.md` |
| P6 Polish | M | P3, P4, P5 | The page turn and pop-up unfold everywhere, win and lose sequences, the icon, accessibility, the device matrix, the soak | `subplans/P6_polish.md` |
| P7 Release and web | S | P6 | The release checklist, the final APK through the pipeline, the browser build on Netlify on the owner's yes | `subplans/P7_release_web.md` |

Order: P0 → P1 → P2 → P3 → P4 → P5 → P6 → P7, one phase at a time (no subagents, so nothing runs in parallel).

Gates and checkpoints (the full exit lists are in the subplans):

- **P0, ask once:** the section 2 decisions; `git init`.
- **P1 is the look gate.** The owner looks at the phone next to the concept frame: is it beautiful, and is it nothing
  like GLOWTIDE? Palette, pawn shape and tag style are chosen here. If the contract is missed, bring measured options.
- **P2 is the game gate, the most important checkpoint.** The owner plays levels 1–8: is this the game they meant, is a
  move satisfying, is undo right, should losing be harsher? Do not build P3–P5 on a core the owner has not played.
- **P3, ask:** create the public repository and push? The owner then runs `scripts\release-key.ps1`.
- **P4 soft checkpoint:** rendered WAV files and the live build.
- **P5 soft checkpoint:** play 1–40 and a boss; difficulty, variety, level length; the star thresholds.
- **P6:** sign-off on feel, then the final device matrix.
- **P7, ask:** the one production deploy to Netlify.

---

## 8. Working agreements

- **Verification loops.** Looks: a scripted picture on the PC with `--screenshot`, then open the PNG; on a device
  `scripts\capture-android.ps1`. Motion: `--strip N` tiled into one sheet. Rules and balance: the solver and the unit
  tests; every shipped level is proven winnable with its best score. Sound: offline renders to WAV, measured with
  `tools\wav_metrics.py`, then the owner's ears. Performance: only numbers from the devices count.
- **PC first, then phone, then browser.** Develop against the Windows build; run on the floor phone, the tablet and the
  headless browser check before calling anything done.
- **Tooling is part of the product.** Pictures, strips, the flow test, the lints and the headless tool are what make an
  executor that cannot see or hear effective. They come with the fork: keep them working from P0.
- **Content is data.** Levels, palettes, tuning numbers, sound recipes and strings live in tables. Tunable numbers are
  overridable from the command line (`--set name=value`, GLOWTIDE's `Tuning`).
- **Owner-facing steps.** Say plainly when you need them: accept an install or debugging prompt, look, listen, play,
  say yes to the repository, run the release key script, say yes to the Netlify deploy.
- **Checkpoints are real.** Do not mark a gate passed without the owner's word. Report failures with the numbers.
- **Git.** Commit at working checkpoints with plain messages. No remote and no push until the owner's yes in P3; after
  that, pushes to `main` build releases (`docs/RELEASE_PIPELINE.md`, written in P3), so documentation-only and
  browser-only commits carry `[skip ci]` or touch only ignored paths. The repository is public: nothing secret or
  personal in a commit; `keys/` is git-ignored.

---

## 9. Risks and fallbacks

| # | Risk | Level | Mitigation / fallback |
|---|---|---|---|
| 1 | The planner guessed the wrong advertisement | High | Section 2's first row: one rule set covers the three candidates as level kinds; P2 asks the owner which should lead, and the calendar (GAME_DESIGN 8.2) is one table to reweight |
| 2 | "Beautiful" is subjective and the executor cannot judge it | High | The concept frame is the target; P1 ends with phone captures beside it and palette variants; juice is specified in numbers (GAME_DESIGN 10) |
| 3 | A turn-based number puzzle is too dry | High | The spectacle carries each move (flip-and-join, count ladder, layered music); P2's gate is played before content exists; levers that add tension without new systems are listed in GAME_DESIGN 7.5 |
| 4 | Too easy (undo, no losing taps) or too fussy (exact optimum for three stars) | Medium | `star2Pct`, `star3Pct`, the dead-end nudge and the hint's cost are tweakables; the bake reports how many random plays win and reach three stars for every level |
| 5 | Generated maps look alike or tangle (roads crossing, tags overlapping) | Medium | Layout recipes are planar by construction; a layout lint checks tag overlap and road clearance at the eight aspect ratios; the no-repeat rule (GAME_DESIGN 8.4) |
| 6 | The solver's state space on big maps with timers | Medium | Limits: 20 sites, 16 with a rival or raids; layered search with one best strength a state (TECH_SPEC 3.5); measured in P2 on the floor phone; fallback: lower the limits, never ship an unproven level |
| 7 | GPU driver differences (Adreno 619, Mali, WebGL 2 on iOS Safari) | Medium | ES 3.0 core only, shaders validated at build time, GLOWTIDE's precision rules and its traps list (its TECH_SPEC 10 and VARIETY_TECH 12); devices and the browser checked every phase |
| 8 | Synthesized march music is thin or grating; the executor cannot hear | Medium | Offline renders with measured loudness, three variants of the theme for the owner to pick from, every layer can be muted by a tweakable |
| 9 | Paper shadows cost too much each frame | Low | Static shadows are baked into the page texture once a level; moving things throw a hard offset silhouette, one extra instanced draw |
| 10 | Shared devices | Medium | Section 3; the app detects focus loss; invalid runs are repeated |
| 11 | Photosensitivity | Safety | No full-screen flashes; confetti and count-ups only; a "reduce effects" setting; GLOWTIDE's flash audit kept |
| 12 | Scope creep into a strategy game | Medium | Section 10's out-of-scope list; pillar 2; briefing rule 4 |

---

## 10. Assumed defaults (the owner can change any of these)

- Working title PAPERMARCH; `applicationId` `dev.papermarch.game` (the dev APK `dev.papermarch.game.dev`, beside a
  GitHub build); versionName starts at 0.1.0.
- Portrait only. English only, strings in one table.
- Level 1 cannot be lost; levels 2–8 teach one thing each; from level 9 the baked table takes over.
- Stars: one for winning, two at 70% of the best possible final host, three for the best possible. A hint caps the level
  at two stars. Nothing is bought or unlocked with stars. Every level is open in the selector, as in GLOWTIDE.
- Out of scope for version 1, not scheduled: troop types, fog of war, hero abilities beyond the war drum, upgrades or
  currencies, daily rewards, advertisements, purchases, leaderboards, cloud saves, multiplayer, an endless mode, a level
  editor, iOS as an app, localisation, gamepad support, landscape, quality tiers.
- Stretch, built only if the owner asks after playing (GAME_DESIGN 5.3): the war drum, fog and watchtowers, troop types.

---

## 11. Sources used while planning

- GLOWTIDE's `CLAUDE.md`, `docs/PLAN.md`, `docs/RELEASE_PIPELINE.md`, `docs/PROGRESS.md` (the selector, the browser
  port, the choices table), `src/game/screens.cpp` (`Game::drawLevels`), `src/platform/platform.h`, read 2026-11-26.
- Three web searches for the advertisements (2026-11-26): nothing describing them was found; the design rests on the
  genre's common mechanics. One web search for the name.
- API signatures and library behaviour in the specs come from the planner's knowledge and from GLOWTIDE's code. Check
  them against the forked code and the pinned headers rather than against this text.
