# P3 — Game loop, selector, updater

Size M. Needs P2 and its owner checkpoint.

**Goal.** A complete game around the nine levels: HOME, the final HUD, PAUSE, WIN, LOSE, SETTINGS, LEVELS, saving,
hint lines, haptics, lifecycle. And, on the owner's yes, the public repository, the release pipeline and the in-app
UPDATE pill, exactly as GLOWTIDE has them.

## Read first

`GAME_DESIGN.md` 7, 11, 13, 14. `TECH_SPEC.md` 5.7, 8, 9.3, 9.4. `..\Glowtide\docs\RELEASE_PIPELINE.md` (the pipeline
you are about to reproduce) and `..\Glowtide\docs\GAME_DESIGN.md` 11 (the flow you inherited).

## Ask the owner first

"May I create the public GitHub repository `rockenrooster/papermarch` and push? The updater needs it. Then please run
`scripts\release-key.ps1` once (it asks you for a new password) **before** the first release build, so every build
can be updated." Tasks 1 to 9 need no answer.

## Tasks

1. **HOME** (GAME_DESIGN 11): the level's map on the table behind the title, the `LEVEL N` chip, total stars,
   SETTINGS, a touch anywhere else starts the level.
2. **The HUD**, final: the level chip with the saved stars, the objective chip by kind, pause; UNDO, the host plate,
   HINT; the hint line and the dead-end slip. Pressed states at 4:1 or better (`tools\pick_contrast.py`).
3. **PAUSE, WIN, LOSE** panels. WIN: stars landing one at a time, `HOST n · BEST m`, NEXT, RETRY under three stars.
   LOSE: the four-word reason, UNDO first, RETRY.
4. **SETTINGS**: sound, music, haptics, fast moves, reduce effects, the version and build line, credits (from
   `LICENSES.md`). The dev-only level stepper GLOWTIDE had (`PM_DEV` builds).
5. **LEVELS**: GLOWTIDE's `Game::drawLevels`, its behaviour unchanged, in paper colours (the current level's button
   gold, the others ivory, stars gold on the cream card).
6. **Save** (TECH_SPEC 8): the stars, the level reached, the settings, the shown hints; through a force-stop, the back
   key and a cold start (GLOWTIDE's three scripts); in the browser through a reload.
7. **Hint lines** for the hand-made levels (GLOWTIDE's mechanism: once each, then remembered).
8. **Haptics** (GAME_DESIGN 13), the back key on every screen, lifecycle (20 cycles in the middle of a clip and on
   every screen), the page re-baked after a context loss.
9. **The layout lint** over every screen at the eight frames; **the flow test** rewritten in full (TECH_SPEC 9.4);
   `scripts\flow-android.ps1`; `scripts\review.ps1 -Set p3 -Lint`.
10. **The repository** (after the owner's yes): `gh repo create rockenrooster/papermarch --public`, `origin`, push
    `main` with the push command of PLAN 3. Before the push, read the whole tree once for anything secret or personal
    (keys, tokens, home-network addresses, the owner's e-mail); `keys/` and `*.keystore` are ignored.
11. **The release key**: the owner runs `scripts\release-key.ps1`. It sets the four `PM_` secrets and starts the first
    build. You never see or handle the password.
12. **The pipeline**: `.github/workflows/release.yml` builds `build-<n>` on each push to `main` that changes more than
    documentation or the browser's own files: `papermarch.apk`, its `.sha256`, the symbols; the 16 KB alignment check.
    Write `docs/RELEASE_PIPELINE.md` from GLOWTIDE's, renamed and true.
13. **The updater, live**: install the first GitHub build on the Moto (it sits beside the `.dev` build); push a second
    commit; HOME shows `UPDATE: BUILD <n>`; the owner taps it and accepts Android's two prompts; the game restarts as
    the new build with its save. `--picture home_update` for the pill's look.

## Exit checks

- [ ] `scripts\test.ps1` exits 0 with the full flow test; the layout lint passes at the eight frames.
- [ ] On the Moto and the tablet: `flow-android.ps1`, `save-android.ps1`, `backkey-android.ps1`,
      `coldstart-android.ps1` (cold start to HOME at most 2 s on the Moto), `lifecycle-android.ps1` all pass.
- [ ] `web_check.mjs flow`, `play` and `reload` pass; the save survives a reload in the browser.
- [ ] `bench_play` inside the contract; no regression against P2.
- [ ] If the owner said yes: the repository exists and is public with nothing secret in it; a signed release was built
      by the workflow; its certificate's SHA-256 is in the release notes and in `RELEASE_PIPELINE.md`; the update from one
      build to the next was done on the Moto by the owner's tap.
- [ ] If the owner said no or has not answered: everything else is done, the workflow sits unused in the tree, and the
      question stays open in `PROGRESS.md`. Do not create the repository.

## Owner checkpoint (soft)

Play through the nine levels from a fresh install: HOME, the selector, a win, a loss, settings. Anything in the way
between launch and the first move? Their answers adjust the screens; P4 starts regardless.

## Not in this phase

Sound, new levels, worlds, the final win and lose sequences, the icon, Netlify.

## Notes

- From the first public push on, every push to `main` that touches the game makes a build and shows every installed
  copy an UPDATE pill. Commit freely, push at working checkpoints, and give documentation-only pushes nothing else to
  carry.
- A GitHub build and a deployed build replace each other only through an uninstall (different keys). Keep testing with
  the `.dev` APK so the owner's GitHub copy and its save are never touched.
- The INTERNET permission is for the update check alone; nothing else may use the network.
