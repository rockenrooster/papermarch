# P1 — Look proof

Size L. Needs P0. **This is the look gate.**

**Goal.** The engine draws the concept frame for real: the table, the baked page with its land, river and roads, the
pop-ups and their shadows, a host and garrisons, tags, the HUD. A battle, a shrine, a page turn and an unfold exist as
scripted strips. It all runs inside the performance contract on the floor phone and in the browser, and the owner has
looked at it on that phone and approved it. No rules yet: the scene follows a script.

## Read first

`PLAN.md` 5 and 9. `GAME_DESIGN.md` 2, 9 and 10. `TECH_SPEC.md` 5, 6 (the clip idea only), 9.1, 9.5 and 10. Open
`docs/concept/concept_gameplay.png` and read `docs/concept/make_concept.py`: its palette, its layer order, its shadow
offsets and its `blob()` are the starting values.

## Tasks

Ordered so that a picture exists after task 3 and gets richer from there. Look at the Moto's and the tablet's capture
after every shader task, not at the end.

1. **Tweakables and palette.** `Tuning` entries for every art number of this phase. The palette table with worlds 1 to
   3 (GAME_DESIGN 9.2) and the palette lint as a unit test.
2. **The fit.** The 2D camera of TECH_SPEC 5.2 and its test over the eight frames.
3. **The table and the bare page.** The wood shader; the page bake's steps 2 and 4 (`blob.frag`, grain, vignette).
   `--picture page_empty`.
4. **The sticker shader** and every shape of TECH_SPEC 5.1. `--picture shapes`: each shape at three sizes, crisp and
   soft, two-tone. This is the picture to compare across the PC, the Moto and the tablet first.
5. **Roads and the river** as capsule chains, stitches, site patches; then the shadow texture and its multiply.
6. **Pop-ups** from one table: tent, houses, shrine, bridge post, gatehouse post, Keep, tree families of world 1 (pine,
   blossom), flag, reeds. Depot and key tower can wait for P5.
7. **Level 12 as data.** Write `sim/level.h`'s structs (TECH_SPEC 3.1; no rules) and hand-made level 12's `LevelDef`
   from GAME_DESIGN 2, with lattice positions that reproduce the concept frame. The scene script reads it: sites A and B
   taken, the host on the road to C.
8. **Figures.** Pawn, rider, giant; the column behind a banner along a road; garrisons around their sites; the caps of
   GAME_DESIGN 9.3. `--picture figures`.
9. **Tags and text.** Every state of GAME_DESIGN 9.4; drawn after all stickers. `--picture elements`.
10. **Moving shadows, cloud shadows, river glints.**
11. **The HUD of the concept frame**, static, on the restyled UI layer (cream cards, gold and ivory buttons, the pressed
    state).
12. **Scripted clips** (the first cut of the view's timeline, driven by a fixed list): march, clash, the flip wave, the
    count-up on the plate; a shrine's unfold-into-two; confetti. Strips `battle` and `shrine`.
13. **The page turn and the unfold.** Strips `page_turn` and `unfold`; the "reduce effects" slide.
14. **Pictures and the review set**: `scripts\review.ps1 -Set p1` (TECH_SPEC 9.1), `concept` at the eight frames.
15. **`bench_battle`** and the contract run on the Moto, then the tablet; the page bake's worst frame; the browser's
    frame rate in `web_check.mjs play` driving the scripted scene. If the contract is missed: TECH_SPEC 9.5, one step
    at a time.
16. **The comparison sheet**: the concept frame, the desktop's `concept` picture and the Moto's capture of it, side by
    side in one image.

## Exit checks

- [ ] `scripts\review.ps1 -Set p1` exits 0: every picture rendered, no GL message, shaders valid.
- [ ] You compared `concept` with the concept frame and wrote the answers in `PROGRESS.md`, each with a number:
      - the constant colours and world 1's, sampled at known pixels, are within 8 levels a channel of GAME_DESIGN 9.2;
      - the tag check passes in all eight frames (a tag's number at least 3% of the frame's height);
      - in a greyscale copy, the host and the garrisons are told apart, and a gold tag from an ink one;
      - under a pop-up's shadow the land is 15% to 40% darker than beside it, and no shadow has a hard step (at least
        four distinct levels across its edge);
      - a sticker's edge is smooth: across any crisp edge at least one pixel holds an in-between value;
      - no pixel of the frame is pure white or pure black (flat paper, not glare);
      - `tags_dense`: no tag covers another tag's number.
- [ ] The palette lint passes for worlds 1 to 3; the fit test passes.
- [ ] **The contract:** `bench_battle` on the Moto after a 180 s soak, 60 s measured: late share at most 5%, worst frame
      at most 50 ms, zero heap allocations a frame; CPU and the sticker, tag and confetti counts recorded. The page bake
      adds no frame over 50 ms.
- [ ] The tablet: the contract at 90 Hz, or at 60 Hz with the reason. Its capture of `concept` shows no defect the
      desktop's does not (by eye, and the mean difference of the two images scaled to 64 pixels wide under 12 levels).
- [ ] The browser check: `play` at 55 frames a second or more at 1170 × 2532; no console error.
- [ ] `scripts\test.ps1` exits 0. APK size recorded.
- [ ] If the contract is still missed after TECH_SPEC 9.5's steps: stop and report the measured options.

## Owner checkpoint (gate)

Put `bench_battle` on the Moto in their hand (it is the look in motion), send the comparison sheet and the four strips.
Ask:

1. Is it beautiful? If not: too flat, too busy, too childish, wrong colours?
2. Is it far enough from GLOWTIDE?
3. The soldiers: pawns as drawn, or another shape? Big enough?
4. The tags: readable at arm's length? Gold and lock clear?
5. Worlds 1 to 3: any they dislike?
6. The page turn: keep it this slow, or faster?

Their answers become the tweakables' defaults and are logged. If they want a different look altogether, change
`make_concept.py` first, agree on the frame, then the engine. Do not start P2's view work on an unapproved look; P2's
tasks 1 to 4 (rules and solver) do not depend on the answer and can start.

## Not in this phase

Rules, input beyond P0's touch test, screen logic, sound, more than level 12's map.

## Notes

- There is one quality level. Build the picture that passes on the Moto.
- "Tags win" is what keeps a crowded map readable: never let a pop-up or a figure draw over a tag.
- If the paper looks dirty rather than soft, the shadows are too dark or too wide before anything else is wrong: the
  concept uses 35% to 55% and 3 to 9 px at 1080 wide.
- Flat means flat: no gradient inside a shape, no specular, no glow. Depth comes only from two-tone folds and shadows.
