# P0 — Fork and bootstrap

Size M. Needs nothing.

**Goal.** GLOWTIDE's engine shell, renamed and with GLOWTIDE's game taken out, builds and runs as PAPERMARCH on the PC,
on a phone and in the browser. It shows an empty cream page on a wooden table, takes a touch, and every tool of the
fork works: screenshots, strips, benchmarks, the test executable, the device scripts, the browser check.

## Read first

`PLAN.md` (all). `TECH_SPEC.md` 1 and 2. `..\Glowtide\CLAUDE.md` (the commands you are about to inherit) and
`..\Glowtide\docs\PLAN.md` 3 (the PC's traps, the devices).

## Ask the owner first (one message, then start: tasks 1 to 5 need no answer)

1. PLAN 2's decisions: the name PAPERMARCH, the paper look of `docs/concept/concept_gameplay.png`, pick-your-fights as
   the lead kind, the fork. Anything to change?
2. `git init` inside `Papermarch\` (local only until P3)?

## Tasks

1. **Check the ground.** `..\Glowtide` is on `main`, clean, at `dff405a` (if it has moved on, record the commit you
   fork from). The tools GLOWTIDE PLAN 3.1 names answer with those versions. `D:\Stuff\DevFolder\emsdk` has
   `upstream\emscripten\emcmake.exe`.
2. **Copy** per TECH_SPEC 1.2. `git init`, commit "GLOWTIDE's shell at dff405a" before changing a byte, so the diff of
   everything after is readable.
3. **Rename** per TECH_SPEC 1.1 (files, identifiers, strings, the package folder). The `rg` of 1.1 finds nothing.
   Version 0.1.0 in CMake and Gradle.
4. **Take the game out.** Delete `src/sim/*`, `src/game/view.*`, `hud.*`, the renderer, meshes, camera, every shader
   not on TECH_SPEC 1.2's keep list, the bake and recipe commands of the headless tool, GLOWTIDE's pictures and its
   flow checks. `Game` keeps its screens and modes as stubs. What draws: the table (a flat brown for now), a cream
   rounded page (a UI rectangle), `PAPERMARCH P0` in the font, a dot where the screen is touched.
5. **Desktop.** `scripts\build-desktop.ps1`, `scripts\run-desktop.ps1 -Screenshot captures\p0.png`: silent, off screen,
   exit 0; open the PNG. `scripts\test.ps1` passes with what is left (core tests, the PNG check).
6. **Third-party.** `scripts\fetch-deps.ps1` clones SDL from `..\Glowtide\third_party\SDL` at the pinned commit and
   verifies it. No download.
7. **Android.** `$env:PM_APP_ID_SUFFIX = '.dev'`, `scripts\build-android.ps1 -Config dev`,
   `scripts\deploy-android.ps1 -Serial moto` (the lock, the foreground check), `scripts\capture-android.ps1`. Log
   `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`, the surface size and the refresh rates into `PROGRESS.md`. Then the tablet.
8. **The browser.** `scripts\build-web.ps1`, `scripts\serve-web.ps1` on 8092 (localhost is enough: do not add a
   firewall rule), `node tools\web_check.mjs http://localhost:8092/ captures\web reload`: the page loads, the canvas has
   the device's pixel size, no console error, a value written to the save survives a reload.
9. **Benchmark plumbing.** `scripts\bench-android.ps1 -Serial moto -Scenario bench_idle -Seconds 30 -Warmup 10`: present
   timestamps are available, a `PMBENCH` JSON line comes back, zero heap allocations a frame.
10. **Lifecycle.** `scripts\lifecycle-android.ps1 -Serial moto`: 20 background and foreground cycles, no crash, no GL
    error.
11. **Docs.** Rewrite `CLAUDE.md` with the commands that really exist now (GLOWTIDE's table, renamed, minus what was
    deleted), the owner's rules and the conventions. Create `docs/PROGRESS.md` (phase status, "Forked from GLOWTIDE",
    "Choices made for the owner", "Measured device facts", "Benchmarks", "Deviations", "Open questions for the owner").
    `docs/LICENSES.md` (SDL, the font). A short `README.md`.

## Exit checks

- [ ] One command builds and runs on the PC; one builds, installs and launches on the Moto; one builds the browser
      site. All three show the same page.
- [ ] `scripts\test.ps1` exits 0. The rename `rg` finds nothing.
- [ ] `bench_idle` on the Moto and the tablet: late share and worst frame recorded (they should be trivially inside the
      contract; if not, the fork is broken: fix before P1).
- [ ] The lifecycle run passes. The browser check passes.
- [ ] `CLAUDE.md`, `PROGRESS.md`, `LICENSES.md` exist and are true. Committed.
- [ ] No file was changed in `..\Glowtide` or `..\Raveform` (`git status` there is as you found it), except lock files
      that came and went.

## Owner checkpoint

None beyond the two questions. If they are unanswered when the checks pass, record PLAN 2 as kept and `git init` as
done locally (the owner's standing preference is best judgment over waiting), and start P1.

## Not in this phase

Any rule, any paper art beyond the flat page, any sound, the GitHub repository, Netlify.

## Notes

- Copying is the work here; resist improving GLOWTIDE's code on the way in. A file you do not understand yet is copied
  whole and trimmed when its phase comes.
- Keep the device scripts' lock, foreground and keep-awake code exactly as it is.
- The stub `CLAUDE.md` in the repository's root was written by the planner; replace it.
