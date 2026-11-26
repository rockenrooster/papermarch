# PAPERMARCH — notes for Claude sessions

Read `docs/PLAN.md` first (its section 0 is the briefing), then `docs/PROGRESS.md` (what is built, measured numbers,
choices made for the owner, open questions). `docs/GAME_DESIGN.md` and `docs/TECH_SPEC.md` by section; the phase work
orders are `docs/subplans/P0..P7`. The look target is `docs/concept/concept_gameplay.png`.

## Commands (PowerShell 7, repo root; every script dot-sources `scripts\env.ps1`: 4 cores, silent, the tool paths)

| Task | Command |
|---|---|
| Third-party sources (SDL cloned from RAVEFORM's local checkout at the pinned commit; Khronos headers from the NDK) | `scripts\fetch-deps.ps1` |
| Build desktop (`dev` = RelWithDebInfo, `debug` = Debug + GL debug context) | `scripts\build-desktop.ps1 [-Config dev\|debug] [-Clean]` |
| Run desktop (silent and off screen unless `-Sound` / `-Watch`) | `scripts\run-desktop.ps1 [-Config dev] [-NoBuild] [-AppArgs '...'] [-Screenshot x.png] [-Frames N] [-Res WxH] [-Small]` |
| Every check without a device: unit tests (rules, solver = brute force, hand-made levels, the table, palettes, save), the table, the flow test, the layout lint on every screen | `scripts\test.ps1 [-NoBuild] [-Quick]` |
| Picture sets and contact sheets | `scripts\review.ps1 -Set p1\|screens\|levels\|worlds\|strips [-From N -Count 30] [-Levels 12,40] [-Lint]` |
| The best line of a level | `build\desktop-dev\papermarch_headless.exe --solve --level 12` |
| Every level: kind, recipe, best, random and greedy plays, solver cost, the hash | `papermarch_headless --sim table [--from A --to B]`, `--sim tablehash` |
| One generated candidate and its grade | `papermarch_headless --gen --level 90 --recipe 2 --seed 7` |
| The bake (writes `src\sim\levels_baked.h` and a report; seconds) | `papermarch_headless --bake-levels --from 1 --to 300 --jobs 4 --seeds 600 --out src\sim\levels_baked.h --report captures\levels\report.md` |
| Offline audio and its numbers | `papermarch_headless --render-audio music\|battle\|ladder\|win\|lose\|turn\|ui\|effects --out x.wav [--world N] [--layers K] [--boss]`; `python -I -B tools\wav_metrics.py x.wav` |
| Build the APK (`$env:PM_APP_ID_SUFFIX = '.dev'`: `dev.papermarch.game.dev`, installs beside a GitHub build) | `scripts\build-android.ps1 [-Config dev\|debug\|release] [-Offline]` |
| Install and launch on a device (lock, foreground checks) | `scripts\deploy-android.ps1 -Serial moto\|tablet\|<serial> [-NoBuild] [-AppArgs '...']` |
| Benchmark on a device | `scripts\bench-android.ps1 -Serial moto -Scenario bench_idle\|bench_battle\|bench_play [-Seconds 60] [-Warmup 180]` |
| Screenshot of a device (PAPERMARCH in front only) | `scripts\capture-android.ps1 -Serial moto [-Out x.png]` |
| The browser build (Emscripten from `D:\Stuff\DevFolder\emsdk`, shared: only read it) -> `build\web\site\` | `scripts\build-web.ps1 [-Clean]` |
| Serve it on port 8092 (RAVEFORM 8080, GLOWTIDE 8091) | `scripts\serve-web.ps1 [-Build]` |
| The browser build in a headless, muted Chrome at an iPhone's size with touch | `node tools\web_check.mjs http://localhost:8092/ captures\web flow\|play\|reload` |
| The release key (once; `-Generate` keeps a random password in `keys\`, gitignored) | `scripts\release-key.ps1 [-Generate] [-SecretsOnly] [-NoBuild]` |
| The release APK on GitHub (`docs/RELEASE_PIPELINE.md`) | `gh workflow run release.yml -R rockenrooster/papermarch`, `gh run watch -R rockenrooster/papermarch` |
| Push (the PATH git is MSYS2's; the global gitconfig names a missing CA bundle) | `& 'C:\Program Files\Git\cmd\git.exe' -c http.sslBackend=schannel -c http.sslcainfo= -c credential.helper= -c "credential.helper=!gh auth git-credential" push origin main` |

App arguments: `--scenario play|bench_idle|bench_battle|bench_play|sim_hash`, `--level N`, `--bot best|greedy|random`,
`--picture concept|level|world|figures|home_update|screen_<play|pause|win|lose|settings|levels|end>`, `--moves 1,2,5`,
`--screenshot x.png`, `--frames N`, `--strip K`, `--strip-step S`, `--res WxH`, `--dp-scale`, `--safe top,bottom`,
`--set name=value` (src/core/tuning.h), `--silent`, `--no-audio`, `--offscreen`, `--flow-test`, `--lint`, `--fresh`,
`--log-sites`, `--reduce-effects`, `--world N`. On Android they come from the `args` intent extra (debuggable builds).
Desktop exit codes: 0 ok, 1 start-up failed, 2 bad arguments, 3 GL errors, 4 benchmark invalid, 5 capture failed,
6 flow test failed, 7 a lint failed. Log tags: `PAPERMARCH`, `PMBENCH`, `PMSIM`, `PMLIFE`, `FLOW`.

## Owner's rules (they hold for every run)

- **No subagents**: one agent works directly.
- **PC runs are silent and off screen** (`--silent --offscreen`), on **at most 4 CPU cores**. Phones keep their sound.
- **Shared devices** (PLAN 3): every adb command with `-s <serial>`; nothing device-wide; check the foreground app
  first; take the lock file in `D:\Stuff\DevFolder\Android_Games\Raveform\captures\locks\`; force-stop only
  `dev.papermarch.game` (and `.dev`).
- Never edit, build or commit inside `..\Glowtide\` or `..\Raveform\` (GLOWTIDE is the fork's source: read and copy).
- The repository is **public**: nothing secret or personal in a commit; `keys\` is gitignored. Pushes to `main` that
  change the Android game build a release (docs-only and browser-only pushes do not; `[skip ci]` skips one).
- Netlify (the owner's free account, credits shared with other sites): draft deploys are free; a production deploy only
  when the game is finished (the owner's words) and once.
- No destructive git or shell commands. Short chat replies.

## Conventions

- C++20, no exceptions, no RTTI, warnings are errors. `src/sim` is integers only: no floats, no clock, no allocation.
  Every element keeps GAME_DESIGN 3.5 (a larger host is never worse): the solver's pruning rests on it.
- OpenGL ES 3.0; shaders `#version 300 es` with the prelude, validated by glslangValidator at build time and embedded.
  No constant arrays indexed by a variable in shaders (the 12R trap). No heap allocation in the steady-state frame.
- Levels are data: `src/sim/levels_hand.cpp` (hand-made), `src/gen` (the generator), `src/sim/levels_baked.h` (the
  bake's output, committed). Change a recipe, then re-bake and look at the sheets.
