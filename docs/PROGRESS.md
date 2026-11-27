# PAPERMARCH — Progress

The owner's words that started the build (2026-11-26): "Ok, now implement. Public repo. You have my permission to do
anything you need (including elevated commands if needed)", then the goal "finish the project". Mid-way: "I want the
menus to be simple ... the map thing in the real game is not wanted at all. Just level after level until game is
finished (300 levels)" and "Next taps are fine". After two screenshots of the advertisement: "I'm still not seeing
anything close to it" and "I think you designed the wrong thing I was thinking of". The game was rebuilt as the
real-time squad shooter of those screenshots the same day (commit `fe44b21` on top of the turn-based `5b8bb05`).

## 2026-11-26 evening: the new look and the difficulty curve

The owner: "Visuals and graphics need lots and lots of work, it needs to be visually impressive. Hand off any creative
and design or asset creation work to a fable subagent", then "make sure that it scales up in difficulty, with extra hard
levels throwing in here and there with the later levels just barely theoretically beatable with 100% perfect runs".

- **Art (Fable, `docs/ART_DIRECTION.md`):** painted toy soldiers in a sunlit diorama, rendered in Blender by
  `tools/art/` and shipped as sprites in `assets/art/` (16.3 MB). Stage 1: all units, the ogre, gates, barrel,
  effects. Stage 2: ten worlds (road, land, props, backdrop, sky, grade, ambient) and one ogre recoloured per world with
  an add-on. Stage 3: the toy-box UI (section 15), the squad's banner and cheer, the app icon. Released as 0.3.0
  (world 1) and 0.4.0 (everything).
- **Engine (this session):** `src/gfx/art.*` loads the manifest, the atlases (one texture array; each world's own
  atlases loaded while it is played) and the world looks; sprites with elevation variants in the sticker pass; textured
  road and land with anisotropy, sky and backdrop; cast shadows along the sun; bloom, grade and vignette; the animated
  gate panels; deaths, blasts, the boss's stomp and fall, confetti, sparkles, drifting leaves; the new camera
  (30°, 15 high, 8.5 back).
- **Difficulty:** the planner (`planLevel`) is the "perfect run"; hordes are sized against the squad the bot really
  has; the end fight is dialled so the best run keeps a share of its peak that falls to under 3% by level 300, with a
  hard level about one in nine (a third of the margin, a boss, "HARD LEVEL! EVERY SOLDIER COUNTS"). The march speeds up
  from 4.6 to 7.0 units a second and the horde from 1.0 to 1.35 times its pace over the 300 levels. Hordes are sized by
  what their own enemies take (late roads carry 1,000-1,300 enemies) and the ogre crushes a ninth of the squad it meets
  each second, so levels run 24-48 s. GAME_DESIGN 6.
- **Checks (0.4.0):** unit tests 31/31, the table 300/300 replayed exactly, the flow test 23/23, the layout lint on every
  screen; the browser: level 1 won by touch, NEXT, the save, 55.8 frames a second on level 30 at 1170 x 2532 in
  headless Chrome. Not yet on a phone (none attached): its frame rate, memory (the atlas array is five 2048 x 2048
  layers, about 107 MB with mipmaps; the art loads in about 0.2 s on the PC) and load time are unmeasured. If memory
  is short on a phone: the UI atlas (2048 x 512) and small prop atlases could share a layer, or the array could use
  half-size mipmaps.

## 2026-11-27: the owner's first play of 0.4.0, and the content pass

The owner: "Bullets don't do very far. Send it anymore bullets until converge on a single Target on a boss. Seems very
repetitive we need lots of variety in the levels. And when I have lots of guys and move them all the way to the left or
all the way to the right a clip out of the path instead of bunching up. We also need a pot of weapon variety and
temporary upgrades and power-ups ... Once you are done with this content pass we will need to do some rebalancing."
Then "generally hit the closest enemy to the squad. Tough enemies seem to be ignored" and "lod pop in issues with the
background objects on the sides".

- **0.4.1:** bullets fly 48 units; each shot goes for the nearest enemy the shots in flight will not already kill, led
  to where they meet (a brute in front takes all it needs; the boss takes everything once nothing stands before it); a
  big squad at a road edge bunches up inside the road; sprites cross-fade between their renderings instead of popping
  (TECH_SPEC 5); soldiers carry their gun as an overlay (Fable's new renders, the first of the content pass). Re-baked:
  300/300, the tests pass.
- **The content pass** (`docs/CONTENT_PASS.md`, Fable's design: ten weapons, twelve power-ups, six enemies, boss
  attacks, fourteen hazards, ten modes, a rotation so neighbours differ) is built in its eight steps, each baked, tested
  and released; then the rebalance. Fable made the assets for steps 1 and 2.
- **0.5.0, step 1 (weapons):** the squad carries one weapon: musket, twin rifles, scattergun, long lance, rockets
  (GAME_DESIGN 2). Weapon gates (cream and orange, the icon and name) stand beside a `+N` gate or another weapon;
  weapon crates are shot open. Most levels from 9 offer one, never the one the level before offered; 9, 14, 22 and 28
  bring each in with a hint. Hit points are now counted in tenths, and a musket ball's damage grows with every soldier
  past 50. Each weapon has its gun on the soldiers, its projectile, muzzle flash and sound; rockets smoke and burst;
  the lance's bolts flash through what they pierce; the HUD shows the weapon carried.
- **0.6.0, step 2 (power-ups):** rapid fire, power shot, shield, freeze and airstrike on gold medallions and supply
  crates (GAME_DESIGN 4), brought in on levels 11, 18, 23, 29 and 32, then placed before hordes now and then. The
  shield is a dome that cracks as it absorbs; frozen enemies stand tinted with ice at their feet; bombs fall with
  growing shadows; the HUD shows running power-ups in draining rings.
- **0.7.0, step 3:** mines (13), archers and their arrows (17), moving gates on rails (19), cages of prisoners (24);
  the modes gold rush (15, then every 5th level: a bonus road that cannot be lost), arsenal (22, every 2nd: a weapon
  from the start) and rescue (24, every 4th: start with 3, free the prisoners), each with its hint and its icon in
  the level chip (GAME_DESIGN 5).
- **0.7.0, step 4:** catapults at the road's side (21) hurling boulders at red circles; giants with health bars that
  drop medallions (29); the boss's attacks shown on the road and dodged sideways: the club (30), boulders (50), the war
  cry (60), the stomp's ridge with a gap (70), each world its own (GAME_DESIGN 5). Released with step 3 as 0.7.0.
  Fable's art for steps 3-4 (ART_DIRECTION 17) is in: archers, catapults, giants, mines, cages, wheeled posts and
  rails, the warnings and the ogre's attack poses with each world's add-on; the art array is now 11 layers of
  2048 x 2048 (about 180 MB with mipmaps: the first thing to measure on a phone).
- **2026-11-27, the owner: "No more fable agent".** The Fable art agent was stopped while starting step 5 (its
  partial effect generators are in `tools/art/make_content.py`, not run). Steps 5-7 go on without new Blender art:
  reused and tinted sprites, effects drawn in the view, and the existing generators.

## 2026-11-27: 0.8.0, 100 longer levels, the squad stands for hordes, step 5

The owner: "I'm thinking we may need to change it to where the enemies come to the squad rather than the enemies and
squad moving", chose "halt for hordes", then "the levels can be longer as the get higher too so they feel more epic",
"Like the last level can be like 15 mins total. And the last level can be 100 instead."

- **The squad stands** while a horde is within 25 units ahead and marches on once it is beaten; the horde runs at it at
  its own pace plus the march's speed (GAME_DESIGN 5). Standing, the squad's fire held every horde back for good, so
  three rules came with it: horde enemies have three times their kind's hit points; against the horde a ball counts as
  one musket ball whatever the squad's size (`Battle::against`); one that arrives takes its strength times squad / 50
  past 50 (`Battle::toll`). A horde's bite now grows smoothly with its size and is the same share of any squad.
- **A level ends when every enemy is beaten** or at the finish line (the owner: "level should be finished when finish
  line is crossed or all enemies are gone"); the last stand stands 50 beyond the road's last section, so it comes
  only once everything else is passed.
- **Hordes on the way are sized for a weaker player** (half the bot's soldiers on level 4, 85% on level 100): whoever
  misses some gates still gets through, and the end fight sets each level's margin. Levels 1-3 hold at most 20 foot a
  horde: three soldiers win them (the browser's touch test sweeps the squad about and wins level 1).
- **100 levels**, each longer than the last: 0:26 at level 1, 1:03 at 20, 2:53 at 50, 7:33 at 80, 15:35 at 100 (the
  bot's runs). Speed and difficulty curves rescaled to 100 (the margin `0.02 + 0.6·e^(−n/22)`: 2% at the end); a
  ceiling per level stops long roads snowballing to 99,999 (GAME_DESIGN 6). The LEVELS page jumps ±10.
- **The bake** for long roads: the generator keeps one run of the road as it lays it and measures each section from a
  copy; it lays everything on the text form's grid first (a horde's cost can turn on a tenth); the bot's own run is each
  level's proof (a planned run met hordes sized for another squad and fell); the end fight is replayed alone for each
  strength tried; a seed whose end fight is next to nothing is passed over. 100 levels in 3 minutes on 4 jobs.
- **Step 5** (CONTENT_PASS 1-3): shield bearers (26), bombers (37), drummers (42); the frost blaster (32), mortar (36),
  flamethrower (41), cannon (48) and storm coil (53); quick march (38), magnet (44), reinforcements (46), giants (49),
  hourglass (54), double gates (58) and the field gun (64), each with its hint line. Drawn from Fable's finished renders
  (packed without an agent): the new figures, a shield before each bearer, a drummer's beat in rings, blasts for kegs
  and shells, lightning bolts in pieces, the flame's jet over its cone with scorch marks, shells arcing to target rings,
  the cannon rolling at the squad's front and the field gun at its side, the magnet's arc, quick march's lines,
  reinforcements floating down, soldiers half again as tall under GIANTS, gold rims under gates that will count twice.
  Soldiers stop stepping while the squad stands.
- **Checks:** unit tests 31/31, the bot wins all 100 levels with the baked squads, the flow test 23/23, the layout lint
  clean; the browser: the flow test 23/23, level 1 won by touch, NEXT, the save (index.wasm 1.7 MB, down from 2.8: the
  decoded levels are allocated at start, not a static table). The browser's 32 frames a second here is the PC's: it
  is in a Remote Desktop session at 32 Hz (the same with the art off on level 1).

## 2026-11-27: 0.9.0, content step 6

Rollers (27): a boulder down a lane marked in red once the squad is within 30, taking whoever stands in it (at most 40%);
fort walls (sieges, 37): the march stops at them, the horde waits behind, archers on the ramparts, siege weapons do
three times, "WALL DOWN!" when one falls; the modes gauntlet (34), siege (37), rush (38) and giants' road (49) in the
block of ten (GAME_DESIGN 5), rescue moving to slot 3 from 34, each mode's icon and hint. Without new art: the boulder
and the red lane tiles are reused, walls are stone paper stickers with battlements, cracks and a strength bar.
Hammers, pits, narrows, tide and the last stand are left out (they need art or a ground shader the game does not have).
Checks: unit tests 31/31, the table 100/100, the flow test 23/23 on the PC and in the browser, level 1 won by touch.

## Status

| Part | Status |
|---|---|
| The battle (rules, bot, generator, 100 baked levels) | Done; the bot wins every level with the squad the bake recorded |
| The view (road, squad, gates, barrels, hordes, boss, effects) | Done on the PC and in the browser; not yet seen on a phone |
| Screens, flow, LEVELS, save, updater pill | Done; flow test 23/23; layout lint clean on 8 screens at 4 sizes |
| Audio | New battle sounds and cues; offline renders measured (below); the owner's ears pending |
| Android | The dev APK builds (4.9 MB); GitHub releases from `build-6` on (0.2.0, signed) are the shooter; the icon is a paper soldier on the road |
| Browser | Builds (index.wasm 2.8 MB); headless Chrome: the flow test 23/23, level 1 won by a touch sweeping the squad, NEXT, the save kept and read back, a notch's insets, 56 frames a second while the bot plays level 30 |
| Netlify | Not deployed: once, when the owner says the game is finished |

## Measured (this PC, 2026-11-26)

- The bake: 300 levels in about 16 s on 4 jobs (`captures/levels/report.md`); 0 failed.
- Unit tests 31 checks, the table 300/300, the flow test 23/23 (69 s with `--fixed-step`).
- Audio renders (`tools\wav_metrics.py`): the battle mix -21.9 LUFS, peak -7.2 dBFS, no clipping; the effects -24.7
  LUFS; the march -24.8 LUFS. Shot, hit and kill gains were raised after this measurement.
- Browser: 55.7-56.1 frames a second in headless Chrome at 1170 × 2532 on level 30.
- Desktop `bench_battle` on level 211 (the biggest hordes, 1,064 figures, 2,900 stickers at most): sim 0.04 ms, view
  0.25 ms, submit 0.07 ms a frame (P95), no heap allocation, 6 draw calls. The off-screen window's swap is throttled by
  Windows, so frame intervals on the PC mean nothing; the phones decide the contract.
- The view adds the squad, gates and barrels before the horde, so a full sticker pool (6,000) drops bullets and bits
  first; a horde over 400 or 700 alive draws its far members with fewer details.

## Choices made for the owner

| Question | Choice | Why |
|---|---|---|
| The game | A real-time squad shooter up a road, as in the owner's screenshots: drag to steer, automatic fire, gates that grow when shot, barrels, barricades, hordes, an ogre boss | The owner's screenshots and words |
| The look | Kept the cut-paper style (soldiers as paper pawns with folds and soft shadows) on a perspective stone road in daylight, ten worlds | Original, bright, nothing like GLOWTIDE; reads like the advertisement's layout |
| Steering | Relative drag: the squad moves by the finger's movement (1.25 road widths per road width dragged), anywhere on the screen | The finger never hides the squad |
| Difficulty | The bake's bands (section 6 of GAME_DESIGN): early levels are won with half the squad or more, late ones with 10-40% | A bot that plays like a careful player sets the bar |
| Stars | Three at 90% of the bot's end squad, two at 45% | Beating the bot is the chase |
| Version | 0.2.0 for the shooter | A different game from 0.1.0's prototype |

## Devices

On 2026-11-26 no test phone was attached or visible on Wi-Fi (`adb devices` empty). The phone checks (the benchmark,
captures, haptics, the updater on a real install) wait for a device.

## Open questions for the owner

- Is this the game from the advertisement now? What should change first?
- Steering: too fast or too slow (`--set steer=`)? Camera: more road ahead (`camPitch`, `camHeight`)?
- Difficulty and length (20-40 s a level): right?
- Sound: shots, explosions and the march on your phone.
- Netlify: say when it is finished and it goes live once.
