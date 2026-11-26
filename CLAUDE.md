# PAPERMARCH — notes for Claude sessions

A real-time squad shooter in cut paper (the owner's advertisement screenshots, 2026-11-26): the squad marches up a road
firing; the player drags it through gates, shoots barrels and barricades, crushes red hordes and an ogre boss. Read
`docs/PLAN.md` first, then `docs/PROGRESS.md`; `docs/GAME_DESIGN.md` (rules and numbers) and `docs/TECH_SPEC.md` (code)
by section.

## Commands (PowerShell 7, repo root; every script dot-sources `scripts\env.ps1`: 4 cores, silent, the tool paths)

| Task | Command |
|---|---|
| Third-party sources (SDL from RAVEFORM's local checkout at the pinned commit; Khronos headers from the NDK) | `scripts\fetch-deps.ps1` |
| Build desktop (`dev` = RelWithDebInfo, `debug` = Debug + GL debug context) | `scripts\build-desktop.ps1 [-Config dev\|debug] [-Clean]` |
| Run desktop (silent and off screen unless `-Sound` / `-Watch`) | `scripts\run-desktop.ps1 [-Config dev] [-NoBuild] [-AppArgs '...'] [-Screenshot x.png] [-Frames N] [-Res WxH] [-Small]` |
| Every check without a device: unit tests, the table, the flow test, the layout lint on every screen | `scripts\test.ps1 [-NoBuild] [-Quick]` |
| A battle picture N/60 s into a level (the bot plays) | `scripts\run-desktop.ps1 -Config dev -NoBuild -AppArgs '--picture level --level 12 --fixed-step --fresh' -Frames 300 -Screenshot captures\shots\x.png -Res 540x1170` |
| The bot's run of a level, event by event | `build\desktop-dev\papermarch_headless.exe --play --level 12` |
| Every level: length, waves, enemies, boss, the bot's peak and end, stars | `papermarch_headless --sim table [--from A --to B]` |
| The bake (writes `src\sim\levels_baked.h` and a report; about 16 s) | `papermarch_headless --bake-levels --from 1 --to 300 --jobs 4 --seeds 60 --out src\sim\levels_baked.h --report captures\levels\report.md` |
| Offline audio and its numbers | `papermarch_headless --render-audio music\|battle\|effects\|win\|lose\|ui\|turn\|ladder --out x.wav [--world N] [--layers K]`; `python -I -B tools\wav_metrics.py x.wav` |
| Build the APK (`$env:PM_APP_ID_SUFFIX = '.dev'`: `dev.papermarch.game.dev`, installs beside a GitHub build) | `scripts\build-android.ps1 [-Config dev\|debug\|release] [-Offline]` |
| Install and launch on a device (lock, foreground checks) | `scripts\deploy-android.ps1 -Serial moto\|tablet\|<serial> [-NoBuild] [-AppArgs '...']` |
| Benchmark on a device | `scripts\bench-android.ps1 -Serial moto -Scenario bench_idle\|bench_battle\|bench_play [-Seconds 60] [-Warmup 180]` |
| Screenshot of a device (PAPERMARCH in front only) | `scripts\capture-android.ps1 -Serial moto [-Out x.png]` |
| The browser build (Emscripten from `D:\Stuff\DevFolder\emsdk`, shared: only read it) -> `build\web\site\` | `scripts\build-web.ps1 [-Clean]` |
| Serve it on port 8092 (RAVEFORM 8080, GLOWTIDE 8091); it listens on IPv4 only | `scripts\serve-web.ps1 [-Build]` |
| The browser build in a headless, muted Chrome at an iPhone's size with touch (absolute out folder) | `node tools\web_check.mjs http://127.0.0.1:8092/ $PWD\captures\web flow\|play\|reload` |
| The release key (exists; `-Generate` keeps a random password in `keys\`, gitignored) | `scripts\release-key.ps1 [-Generate] [-SecretsOnly] [-NoBuild]` |
| The release APK on GitHub (`docs/RELEASE_PIPELINE.md`) | `gh workflow run release.yml -R rockenrooster/papermarch`, `gh run watch -R rockenrooster/papermarch` |
| Push (the PATH git is MSYS2's; the global gitconfig names a missing CA bundle) | `& 'C:\Program Files\Git\cmd\git.exe' -c http.sslBackend=schannel -c http.sslcainfo= -c credential.helper= -c "credential.helper=!gh auth git-credential" push origin main` |

App arguments: `--scenario play|bench_idle|bench_battle|bench_play|sim_hash`, `--level N`, `--bot best`,
`--picture level|world|concept|figures|home_update|screen_<play|pause|win|lose|settings|levels|end>`, `--screenshot
x.png`, `--frames N`, `--fixed-step`, `--strip K`, `--strip-step S`, `--res WxH`, `--dp-scale`, `--safe top,bottom`,
`--set name=value` (src/core/tuning.h: camera, steering, soldiers shown, shadows), `--silent`, `--no-audio`,
`--offscreen`, `--flow-test`, `--lint`, `--fresh`, `--log-sites`, `--reduce-effects`, `--world N`. On Android they come
from the `args` intent extra (debuggable builds). Desktop exit codes: 0 ok, 1 start-up failed, 2 bad arguments, 3 GL
errors, 4 benchmark invalid, 5 capture failed, 6 flow test failed, 7 a lint failed. Log tags: `PAPERMARCH`, `PMBENCH`,
`PMSIM`, `PMLIFE`, `FLOW`.

## Owner's rules (they hold for every run)

- **No subagents** unless the owner asks: one agent works directly.
- **PC runs are silent and off screen** (`--silent --offscreen`), on **at most 4 CPU cores**. Phones keep their sound.
- **Shared devices** (PLAN 3): every adb command with `-s <serial>`; nothing device-wide; check the foreground app
  first; take the lock file in `D:\Stuff\DevFolder\Android_Games\Raveform\captures\locks\`; force-stop only
  `dev.papermarch.game` (and `.dev`).
- Never edit, build or commit inside `..\Glowtide\` or `..\Raveform\` (GLOWTIDE is the fork's source: read and copy).
- The repository is **public**: nothing secret or personal in a commit; `keys\` is gitignored. Pushes to `main` that
  change the Android game build a release (docs-only and browser-only pushes do not; `[skip ci]` skips one).
- Netlify (the owner's free account, credits shared with other sites): draft deploys are free; a production deploy only
  when the game is finished (the owner's words) and once.
- No destructive git or shell commands. Short chat replies. Backups before major changes in
  `D:\Stuff\DevFolder\_backups\Papermarch\`.

## Conventions

- C++20, no exceptions, no RTTI, warnings are errors. `src/sim` has no SDL, GL, clock or allocation after `init`; it
  steps at a fixed 60 Hz. The view (`src/game/field.*`) only reads the battle.
- OpenGL ES 3.0; shaders `#version 300 es` with the prelude, validated by glslangValidator at build time and embedded.
  No constant arrays indexed by a variable in shaders (the 12R trap). No heap allocation in the steady-state frame.
- Levels are data: `src/gen` (the generator), `src/sim/levels_baked.h` (the bake's output, committed). Change the
  generator or the rules, then re-bake (the tests replay the bake's numbers and fail on a stale table) and look at
  pictures.
