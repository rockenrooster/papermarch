# P2 — Rules and first playable

Size L. Needs P1 (tasks 1 to 4 need only P0). **This is the game gate: the most important checkpoint of the project.**

**Goal.** The real rules, the solver and undo; tap and drag input; the view's timeline playing real moves. Levels 1 to
8 and 12 are playable start to finish on a phone and in the browser, with a win, a strand, undo, a hint and the
dead-end nudge. The owner plays them and says whether this is the game.

## Read first

`GAME_DESIGN.md` 2 to 8.1 and 10. `TECH_SPEC.md` 3, 6 and 9.4. `docs/concept/check_levels.py`: the planner's toy
model of the rules, which brute-forces the hand-made levels and asserts every result GAME_DESIGN states (run it: it
prints `ok`). The real simulation must agree with it.

## Tasks

1. **The simulation, whole** (TECH_SPEC 3.1 to 3.4). Every element and property of GAME_DESIGN 5.1, the rival, raids,
   the auto-advance, every way to win and lose. It is small; write all of it now, because the solver's proof needs all
   of it. (The pictures of the elements introduced after level 12 come in P5.)
2. **Rule tests**: one a row of GAME_DESIGN 5.1 and a property, and the stated results of the hand-made levels
   (GAME_DESIGN 8.1; level 12: 285 by the obvious line, 396 best).
3. **The solver** (TECH_SPEC 3.5) and **its brute-force test** (2,000 random levels of at most 8 sites, hosts up to
   the cap). `papermarch_headless --solve --level N` prints the best line.
4. **Measure the solver** on the Moto: microseconds an expansion, and a full solve of synthetic 16- and 20-site maps
   (with a rival and raids at 16). Record it; set the bake's cost limit from the 8 ms budget (PLAN 5). If 20 sites
   cannot make it, lower the site limits and say so in `PROGRESS.md`.
5. **Hand-made levels** 1 to 6 and 12 as `LevelDef`s in `src/sim/levels_hand.cpp`, laid out as below; levels 7 and 8 by
   hand in the shapes of `Hourglass` and `Tree` (GAME_DESIGN 8.1), their numbers tuned with `--solve` and
   `--sim table` until random play wins at least 70%. A table of nine levels for now (`bakedLevelCount()` is 9; level
   12 sits ninth until P5).
6. **Input**: hit circles, tap, drag from the banner with the dotted line, the refusal with its reason line.
7. **The view** (TECH_SPEC 6): events to clips for march, fight, flip and join, count-up, village, shrine, post, fork
   burnt, a growing camp's turn; the queue of two moves and the 2.5 × rule; undo's snap. P1's scripted clips become
   this.
8. **The loop, minimal**: start a level, win panel (stars from `best`), lose panel (UNDO, RETRY), NEXT, RESTART. UNDO
   and HINT buttons live. The dead-end nudge. The hint's two-star cap.
9. **`--bot best|greedy|random` and `--moves`**; strips of each clip from real moves; `scripts\review.ps1 -Set p2`.
10. **Phone and browser.** The dev APK on the Moto and the tablet; `web_check.mjs play` wins level 5 by touch. Sites'
    hit circles at least 64 dp on the smallest frame.
11. **Benchmarks**: `bench_play` (`--bot best` through the nine levels, looping) on the Moto and the tablet.

Layouts (north is the Keep; `—` and `|` are roads; a number alone is a camp):

```
 1:  Home — 4 — 9 — KEEP 20                 (a line up the page)

 2:        KEEP 28                3:      KEEP 22            4:        KEEP 40
          /       \                         |                           |
        A 6      B 14                    camp 10                      camp 9
          \       /                         |                        /      \
            Home              village +8 — Home                 camp 5      ×2
                                                                     \      /
                                                                       Home

 5:  camp 3 — Home — camp 8 — [post −6, a bridge] — village +10 — camp 20 — KEEP 40   (winding up the page)

 6:        KEEP 30
          /        \
     camp 15 ~~~~ village +6      (~~~ the fork's rope bridge: take one, the other burns)
          \        /
           camp 6
             |
            Home
```

## Exit checks

- [ ] `scripts\test.ps1` exits 0: rule tests, the solver against brute force, the hand-made results, the flow test's
      first cut (HOME → a level won by `--bot best` → NEXT; a strand on level 4 → UNDO → a win; HINT then a win gives two
      stars).
- [ ] `--sim table` on the nine levels: all winnable, `best` as GAME_DESIGN states, random play's share printed.
- [ ] The solver on the Moto: the heaviest synthetic map inside 8 ms, or the limits lowered and recorded.
- [ ] `--sim tablehash` equal on the PC, the Moto, the tablet and the browser.
- [ ] On the Moto, by `--bot best` with the capture script: each of the nine levels reaches the win panel with three
      stars; by `--moves`, level 4 strands and the nudge shows.
- [ ] A tap during a clip is taken (a scripted double tap makes two moves); a third is refused.
- [ ] `bench_play` inside the contract on the Moto; no regression against P1; zero heap allocations a frame.
- [ ] `web_check.mjs flow` and `play` pass.

## Owner checkpoint (gate)

The dev APK on the Moto (and the address of the browser build if they want it on another phone: that needs the
firewall rule, their call). Ask them to play 1 to 8 and 12, twice. Then:

1. Is this the game you meant from the advertisements? If another kind should lead (the gate run, holding the line),
   say so now: it changes the calendar, not the engine.
2. Is a move satisfying: the march, the flip, the count? Too slow, too fast?
3. Undo is unlimited and losing only happens when stranded. Right, or should a bad tap lose (GAME_DESIGN 7.5)?
4. Three stars for the exact best: good chase, or too strict?
5. The hint and the "no way through" slip: helpful or too much hand-holding?
6. Numbers and tags: big enough with a thumb in the way?

Log the answers; change the tweakables and, if asked, the calendar's weights. **Do not start P3 without this
checkpoint.** While waiting, P4's sound bank and offline renders do not depend on the answer.

## Not in this phase

HOME, SETTINGS, LEVELS, saving, the generator, worlds beyond 1, sound, the final win and lose sequences.

## Notes

- The simulation knows nothing of time. If you find yourself adding a duration or a float to `src/sim`, it belongs in
  the view.
- GAME_DESIGN 3.5 is the rule the solver stands on. When the brute-force test fails, the rule was broken somewhere:
  fix the element, not the test.
- Keep the clips short. A move that takes over 1.5 s at normal speed is a bug in the table of GAME_DESIGN 10.
