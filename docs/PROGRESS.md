# PAPERMARCH — Progress

The owner's words that started the build (2026-11-26): "Ok, now implement. Public repo. You have my permission to do
anything you need (including elevated commands if needed)", then the goal "finish the project". Mid-way: "I want the
menus to be simple ... the map thing in the real game is not wanted at all. Just level after level until game is
finished (300 levels)" and "Next taps are fine". Executed by one agent (Opus 5.5) working directly, no subagents.

## Phase status

| Phase | Status | Notes |
|---|---|---|
| P0 Fork and bootstrap | Done | GLOWTIDE's shell at `dff405a` copied (`e0a8416`), renamed, its game taken out. Desktop, Android (dev APK 3.6 MB) and browser (index.wasm 1.4 MB) build |
| P1 Look proof | Done on the PC | The engine draws the concept frame (`captures/p1/concept.png`). Device checks: no phone was reachable (see "Devices") |
| P2 Rules and first playable | Done | The rules, the solver (= brute force on 2,000 random levels), undo, hints, the dead-end nudge, the clips; hand-made levels 1-8 and 12 hold their stated results |
| P3 Game loop, selector, updater | In progress | Screens, LEVELS, save, flow test (30 checks) done; the public repository and the pipeline next |
| P4 Audio | In progress | The sound bank and the five-layer march are built; renders and metrics next |
| P5 Content | Done (first bake) | 300 levels baked in 2.1 s on 4 jobs; every level proven winnable with its best; 12 race levels kept with a relaxed band |
| P6 Polish | Pending | |
| P7 Release and web | Pending | |

## Forked from GLOWTIDE (2026-11-26, its `main` at `dff405a`)

Copied and renamed (`glowtide` -> `papermarch`, `GT_` -> `PM_`, namespace `pm`, port 8092): the build (CMake, Gradle,
Emscripten), `scripts/`, `tools/`, `web/`, `android/` with the updater (`ApkUpdater`, `ApkInstallReceiver`, the
activity), `.github/workflows/release.yml`, `src/core`, `src/platform`, the GL loader, shaders' build-time validation,
text and the font atlas, the UI layer, the save, the bench, the audio device. The level selector is GLOWTIDE's
`drawLevels`, restyled. New: `src/sim` (rules, solver, levels), `src/gen` (generator, bake), `src/gfx/paper.*` and every
shader but text, `src/game/scene*` (the page, pop-ups, figures, clips), the screens, the mixer's sequencer and the sound
bank, the tools.

## Choices made for the owner (best judgment instead of questions, as on GLOWTIDE)

| Question | Choice | Why |
|---|---|---|
| The flow (the owner's correction, 2026-11-26) | No hub, no world map: launch goes straight to the saved level; WIN has NEXT (and RETRY under three stars); LOSE has UNDO and RETRY; PAUSE has RESUME, RESTART, LEVELS, SETTINGS; after level 300 an end screen | "Just level after level until game is finished"; "Next taps are fine" |
| The title | A cut-paper card over the first level on a cold start; the first touch fades it and plays | No extra screen between launch and play |
| Fonts | GLOWTIDE's Roboto Black atlas (capitals, digits, `+-x÷%`). The mire's tag reads `÷2` (no `½` glyph) | No download, no re-bake |
| Stars | Five-pointed stars added to the UI batch's shader | GLOWTIDE's four-point star reads as a sparkle |
| Intro levels | 16 Gauntlet, 22 key tower, 28 mire, 34 deserting camp, 42 Siege (40 is a boss), 52 fading shrine (50 is a boss), 62 Race (60 is a boss), 75 Last Stand; each with its hint line; generated with the element forced | The design put some intros on boss levels |
| Release signing | `scripts\release-key.ps1 -Generate` makes the key with a random password kept in `keys\` (gitignored, never printed); the four secrets go to GitHub through gh's stdin | The owner gave permission "to do anything you need"; the key must exist before the first public build so every build can update the last |

## Measured

- Solver: the heaviest shipped level needs 305 expansions from its start (the budget's limit is 60,000); level 12's
  full solve takes 66 us on this PC.
- The bake: 300 levels in 2.1 s on 4 jobs (`captures/levels/report.md`).
- Sound bank: 34 ms on this PC.
- Unit tests: 94 checks; the flow test: 30 checks; all pass.

## Devices

On 2026-11-26 no test phone was attached or visible on Wi-Fi (`adb devices`, `adb mdns services` both empty).

## Open questions for the owner

- Look: is it beautiful on your phone, and far enough from GLOWTIDE? (`captures/p1/concept.png` beside the concept.)
- The game: is pick-your-fights the game you meant from the advertisements? Is a move satisfying? Three stars for the
  exact best: good chase or too strict?
- The march: which world's music do you like best, and is the count ladder pleasant after many levels?
