# P6 — Polish

Size M. Needs P3, P4 and P5.

**Goal.** The moments that make it a spectacle are finished and safe: the win, the perfect win, the boss win, the
loss, the page turn on every level change. The icon, the accessibility settings, the full device matrix and the soak
are done. After this phase nothing changes but what the release needs.

## Read first

`GAME_DESIGN.md` 9.5, 10, 14. `TECH_SPEC.md` 5.5, 9.4, 9.5. `..\Glowtide\docs\RELEASE_CHECKLIST.md` (the shape of
what P7 will tick).

## Tasks

1. **The win** (GAME_DESIGN 10): the Keep folds and stands again in the host's colours, the hop wave, two confetti
   cannons, the fanfare with the stars on the beat. **Perfect**: the gold rain and the run. **Boss win**: longer, the
   boss's banner falling as a paper scrap, a second cannon volley.
2. **The loss**: the strand's sally and the host blown away as scraps; a raid's or the rival's win; the panel with UNDO
   first.
3. **The page turn everywhere**: level to level, HOME to a picked level from LEVELS, a world change (the table's
   colour cross-fades under the turning page). The unfold wave with its clicks. Never a hitch: the next page is baked
   before the curl starts.
4. **The lean-in** on fights, the gold dotted road to takeable sites, the tag bob, the bearer's head shake: every
   small motion of GAME_DESIGN 10 that P2 left rough.
5. **Fast moves** and **reduce effects** (GAME_DESIGN 10's last paragraph), both in SETTINGS and saved.
6. **The icon**: `tools\make_icon.py` redrawn (an ivory pawn under a scarlet banner on green paper, cut-paper
   shadows), adaptive layers for Android, the browser's three PNGs.
7. **Credits** from `LICENSES.md`; the version and build line.
8. **Accessibility pass** (GAME_DESIGN 14): greyscale sheets of `elements` and `kinds`; text sizes at the smallest
   frame; every state's second cue present.
9. **The flash audit** on the `p6` strips (win, perfect, boss win, lose, page turn).
10. **The soak**: `scripts\soak-android.ps1 -Serial moto -Minutes 20` on the heaviest level: temperature, memory,
    late share by the minute.
11. **The device matrix**: `scripts\matrix-android.ps1` on the Moto and the tablet (install, flow, picture checks,
    back key, save, cold start, lifecycle, both benches, refresh rate). Ask the owner whether the OnePlus 12R and the
    S25 Ultra are free; run them if so.
12. **The browser, in full**: `web_check.mjs flow`, `play`, `reload`; and ask the owner to try the page on a real
    phone's browser, an iPhone if one is at hand (Safari's WebGL 2 and its audio are the parts no headless check
    covers). That needs the firewall rule for port 8092: their call, the command is in `serve-web.ps1`.
13. **The release build** on a device: `scripts\release-check-android.ps1 -Serial moto` (starts, ignores arguments,
    not debuggable).

## Exit checks

- [ ] `scripts\test.ps1` exits 0; the flash audit passes on all five strips; the layout lint passes.
- [ ] `scripts\review.ps1 -Set p6` exits 0 and you looked at every strip: nothing pops, overlaps a tag or leaves a
      shadow behind a folded pop-up.
- [ ] A page turn on the Moto adds no frame over 50 ms (the bench's worst frame across forty turns).
- [ ] The soak: late share inside the contract in every minute; memory flat within 5 MB; the temperature recorded.
- [ ] The matrix passes on the Moto and the tablet; every other device either passed or is listed as not tested, with
      the owner's word why.
- [ ] `bench_battle` and `bench_play`: inside the contract, no regression against P5, zero heap allocations a frame.
- [ ] The browser check passes; the frame rate is recorded; what the owner saw on a real phone is recorded.
- [ ] APK under 15 MB; cold start at most 2 s on the Moto.

## Owner checkpoint (the sign-off on feel)

The build on their phone, sound on. Ask:

1. Win, perfect, boss win, loss: any too long, too quiet, too much?
2. The page turn after fifty levels: still a pleasure, or should it be faster?
3. The icon.
4. Anything, anywhere, that feels unfinished?

Fix what they name, re-run the checks it touches. P7 starts on their word.

## Not in this phase

New levels, new elements, new worlds. A bug found in a level goes back through the bake, not around it.

## Notes

- Polish is where scope creeps. If an idea is not in GAME_DESIGN 10, write it under "After version 1" in
  `PROGRESS.md` and leave it.
- The win is the product's best three seconds and plays three hundred times: keep it under 2.5 s to the NEXT button,
  and let a tap skip to the panel.
