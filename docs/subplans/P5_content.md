# P5 — Content

Size L. Needs P3. (P4's music is extended here.)

**Goal.** The whole game: every element and kind on screen, seventeen recipes, a bake that proves and balances 300
levels, ten worlds with their palettes, dressing and music, ten bosses. No level is shipped unproven; no two levels in
a row feel the same.

## Read first

`GAME_DESIGN.md` 5, 6, 8, 9.2, 12.1, 15. `TECH_SPEC.md` 4, 5.2, 9.1, 9.4.

## Tasks

Work in this order; each step ends with pictures looked at and a commit.

1. **The remaining elements on screen**: growing and deserting badges and their turn clips, the fading shrine's
   countdown, the mire, the depot (and the Keep's number dropping), the key tower and the locked road's gate, fork
   bridges in threes. Their sounds (GAME_DESIGN 12.2). `--picture elements` grows; a strip each.
2. **The kinds on screen**: the Gauntlet's tall page and its scroll (the next two rows always in view); the Siege's
   growing Keep; the Race's rival host, its dotted route with pips, its step clip, the fight at Home; the Last Stand's
   raid tokens with countdown badges and the Warlord. The objective chip a kind. `--picture kinds`.
3. **Introduction levels** for each element and kind at the levels of GAME_DESIGN 5.2, by hand (`levels_hand.cpp`),
   each with its hint line, none losable in its first two moves.
4. **Recipes** (`src/gen`): the eight Conquest ones first, then Gauntlet, Siege, Race, Last Stand. For each:
   `--gen --recipe R --seed S` and a sheet of twenty seeds from above (`review.ps1 -Set levels`). A recipe whose twenty
   seeds look like one map needs more freedom before it is used.
5. **Numbers and traps** (GAME_DESIGN 8.3) and **acceptance** (8.4), with the bake's report: per level its kind,
   recipe, sites, `best`, random play's win and three-star shares, greedy's result, the solver's cost, the seeds tried.
6. **Bake 9 to 40**, read the report, look at the sheet of those levels, play five by `--bot random` strips. Fix the
   generator before going wider.
7. **The calendar** (GAME_DESIGN 8.2, or as the owner reweighted it at P2) and **the full bake** to 300 on 4 jobs.
   Record its time. Band tables at the end of the report.
8. **Worlds 4 to 10**: three palette candidates each on one sheet; the lint picks (or the owner does at the
   checkpoint); the tree and dressing families of GAME_DESIGN 9.2's table as sticker groups; `--picture worlds`.
9. **Music for worlds 4 to 10** (mode, tempo, lead), the boss cue. Renders and their metrics rows as in P4.
10. **Bosses**: ten named Keeps (GAME_DESIGN 15), each a variant of the castle (a different silhouette from the same
    stickers), the two-kind maps of GAME_DESIGN 6.6 by recipe, a larger tag. `--picture bosses`.
11. **Checks on devices**: `tablehash` on the Moto, the tablet and the browser; `bench_play` on the heaviest level
    (most stickers) and `bench_battle`; the solver's worst call over the table on the Moto.

## Exit checks

- [ ] `scripts\test.ps1` exits 0, now including the table tests: all 300 winnable, `best` equal to a fresh solve, the
      layout lint on every level at the eight frames, the no-repeat rule, the palette lint for ten worlds.
- [ ] The report's band tables hold GAME_DESIGN 8.4's bands; every level that needed more than 200 seeds is listed with
      why.
- [ ] Counts per hundred levels: each kind and each element appears at least as often as the calendar promises; no
      recipe makes more than 15% of any hundred.
- [ ] Greedy loses where the bands say it should; random play's three-star share is inside its band.
- [ ] The solver's worst call on a shipped level on the Moto is inside 8 ms.
- [ ] You looked at the sheets of 300 thumbnails (ten sheets of thirty) and wrote down every level that looked like its
      neighbour, tangled or empty, and re-baked those (`--from N --to N --keep-to 300`).
- [ ] `tablehash` equal on the PC, both devices and the browser.
- [ ] Both benches inside the contract on the Moto and the tablet; no regression against P4; APK size recorded.
- [ ] `web_check.mjs flow`, `play` (a Gauntlet level: the scroll under touch) and `reload` pass.

## Owner checkpoint (soft)

A build with everything. Ask them to play 1 to 40, a boss (30), 100 to 110 and 200 to 210. Then:

1. The difficulty: where did it get hard, and was that the right place?
2. Variety: did any two levels feel the same? Which kind do they want more or less of?
3. Level length: too long anywhere (the Gauntlet's nine rows, the bosses)?
4. Three stars at the exact best, two at 70%: keep, or loosen?
5. The ten worlds: any palette they dislike? (Send the sheet with the candidates not picked.)

Their answers change the calendar's weights, the bands or the star thresholds, and a re-bake follows. Record your own
choices if they have not answered when the checks pass, and go on to P6.

## Not in this phase

The stretch elements (war drum, fog, troop types), the final win and lose sequences, the icon, release.

## Notes

- The solver makes this phase honest. Never loosen acceptance to get a level through: a level that cannot pass tells
  you the recipe or the walk is wrong.
- A trap is only fair if the player can see it. Every trap the generator plants must be visible from the start of the
  level (no hidden numbers in version 1).
- Numbers under 100,000 on sites, friendly values: a tag with `48,735` on it is a bug in the rounding.
- If the bake is slow, it is the random plays (400 a seed), not the solver: run them only on seeds that passed the
  cheaper lines.
