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

## 2026-11-27: 0.10.0, content step 7

Flip gates (39), mystery gates (56), ice (31) and wind (62) strips, fog (67) and night (81), each with its hint (the
introduction's beats the mode's), the bot reading a flip gate's face at its arrival and leaning into the wind
(GAME_DESIGN 3, 5). Drawn without new art: a flip gate squashes as it turns and shows its next face; a mystery gate is
violet-grey with "?" and both outcomes; ice a pale sheen in slices; wind the road arrows turned sideways and streaks;
fog and night are veils placed in the depth order beyond the squad (night also a blue grade and a deep vignette). Mud
is left out. Checks: unit tests 31/31, the table 100/100, the flow test 23/23 on the PC and in the browser, level 1 won
by touch.

## 2026-11-27: 0.11.0, content step 8

Each world's favourites twice as likely (GAME_DESIGN 6), hard levels keeping a siege or giants' road, and long roads
scaled up: another weapon about every two minutes (level 100 offers ten) and a power-up for about every 45 s (level 100
holds twenty). The rotation of modes is the block of ten built in 0.9.0. Checks: unit tests 31/31, the table 100/100, the
flow test 23/23 on the PC and in the browser, level 1 won by touch. The content pass (CONTENT_PASS 6, steps 1-8) is
done; left out for want of art or a ground shader: hammers, pits, narrows, tide, mud, the last stand.

## 2026-11-27: 0.12.0, the march goes on; blows and flying soldiers

The owner: "I thought we were only causing the march for the big bosses. Can you change it to that? Also we need more
visual effects for when my soldiers get hit and boss enemies should swing and hit my soldiers and the soldiers that killed
should go flying". The squad marches through hordes again (hordes at their own pace) and stops only before the boss,
where a giant stands on it, and at fort walls. The boss now clubs in blows every 0.9 s with its slam pose timed to them
(the same rate of loss as before); a giant that reaches the squad stops and clubs it every 1.2 s. Soldiers lost are knocked
flying, spinning, from where they were hit (harder from a club), with sparks; a few more flash red; the count lost rises
over the squad; a big loss flashes the screen's edge; a club's blow throws dirt, a ring and a jolt. Roads are longer to
keep level 100 near 15 minutes (14:48). Re-baked; unit tests 31/31, the table 100/100, the flow test 23/23 on the PC and in
the browser, level 1 won by touch.

## 2026-11-27: 0.12.1, a crowd that looks its size

The owner: "having lots of soldiers stops visually past a certain point ... bullets seems to stop drawing too after a
certain point". The view drew at most 150 soldiers inside a footprint capped at the road's width, the fire stops growing
past 50 soldiers (each ball does more instead), and bullets were added last, so a full sticker pool dropped them. Now up to
500 soldiers (250 with REDUCE EFFECTS), the crowd growing back toward the camera; each shot drawn as a volley of up to four
tracers; bullets added before the horde; the sticker budget doubled to 12,000. Level 20's boss approach with a squad of
1,100: 385 figures, 2,428 stickers, the view 0.33 ms a frame (P95) on the PC, no allocations. Not yet measured on a phone.

## 2026-11-27: 0.13.0, a pause when they are close, hard levels from 6, themes

The owner: "add the squad pause again, but this time it should only stop when the enemies are very close", "the scatter
gun bullet distance is too low", "we need hard levels sprinkled in, even in the low levels. Like every 5-10 levels",
"Pace of levels feels pretty much the same across the first 20 levels ... Every level should feel somewhat different than
the last", "sprinkle in special levels every now and then like fast ones and such be creative". The squad stops when an
enemy walks within 2.5 of its front. Ranges: scattergun 14 to 30, flamethrower 9 to 13, storm coil 22 to 34, mortar 18 to
28 (the musket and twin rifles 48, the lance 60, rockets 45, the cannon 50, the frost blaster 36 unchanged). Hard levels
at 6, 12, 22, 29, 39, 46, 51, 59, 64, 73, 79, 86, 96, each ending with 0.3-1.4% of its peak. Level themes and five
special ones (GAME_DESIGN 6); roads stop at the level's time (level 100 runs 14:03). Checks: unit tests 31/31, the table
100/100, the flow test 23/23 on the PC and in the browser, level 1 won by touch.

## 2026-11-27: 0.14.0, endless

The owner: "And we should also do an endless mode". ENDLESS under the LEVELS grid starts a run that goes stage after
stage until the squad falls, the squad, weapon and power-ups carried over, each stage laid as it is played and a little
longer, faster and fiercer; a boss every third stage; the best stage and distance saved (GAME_DESIGN 7). Campaign rules
changed on the way (re-baked): a barricade the squad runs into costs at most an eighth of the squad, a giant clubs 4 for
every 50 soldiers (was 8), the mortar lobs at the nearest enemy in reach. Checks: unit tests 31/31, the table 100/100,
the flow test 27/27 on the PC and in the browser (ENDLESS starts a run), the layout lint, level 1 won by touch in the
browser.

## 2026-11-27: 0.15.0, the road's hazards

The content pass's hazards that waited for art are built without it (GAME_DESIGN 5): hammers from 34 (a wrecking
ball from a gantry, the boulder's sprite on a chain), pits from 43, the narrows from 47, the tide from 51, mud from 69,
each with its hint, two or three times as many on a gauntlet, world favourites three times as likely, and in endless
from stage 3. The bot keeps to the tide's dry half and a pit's far side and times the hammer. Hard levels still keep
about 1% (0.3-1.5%); levels run 0:23 at 1 to 14:03 at 100; the endless bot falls on stage 3-14, about 7 (as before).
Hints are counted past 64 (`hints2` in the save). Checks: unit tests 31/31, the table 100/100, the flow test 27/27 on
the PC and in the browser, the layout lint, level 1 won by touch in the browser. The last stand is left out.

## 2026-11-27: 0.16.0, the last stand

The content pass's last mode (CONTENT_PASS M7) on levels 57, 77 and 97 (slot 7, by turns with the siege): the road
runs 0.8 of the level's time, then the squad holds at its end ("HOLD THE LINE", the mode's own chip from Fable's UI art,
no finish line) with two supply crates before it, and five growing waves come at it at once; level 57's stand lasts
about 30 s (won at 3:21). Re-baked (57, 77, 97, and 68 through the theme sequence); unit tests 31/31, the table 100/100,
the flow test 27/27 on the PC and in the browser, the layout lint, level 1 won by touch in the browser. The content
pass (docs/CONTENT_PASS.md) is now built in full.

## 2026-11-27: 0.16.1, a pause that holds; the release APK on an emulator

The owner: "need to have a backoff window when the squad pauses, so it doesn't look jittery when enemies die and the squad
moves and then pauses again because enemies are still close". The stop was decided afresh every step and the march went
from full speed to none at once. Now a stop holds 0.6 s after the last close enemy (with 1.5 more reach meanwhile) and the
march eases in (0.35 s) and out (0.12 s); a unit check plays level 20 and asserts no stand is shorter than 0.6 s (it
stands twice, the shortest 1.8 s). Re-baked: hard levels keep 0.5-1.4%, level 100 runs 13:57; the endless bot falls on
stage 3-12. Checks: unit tests 32/32, the table 100/100, the flow test 27/27 on the PC and in the browser, the layout
lint, level 1 won by touch in the browser.

The 0.16.0 release APK (build 31, signed) on an Android 15 emulator (x86_64 with ARM translation, software GPU, no sound;
no phone was attached): installs; cold start 1.25 s to the window (the first frame 6.7 s after main under the emulator's
software GPU); the title card; the updater sees build 31; level 1 won by touch (6 soldiers, one star); NEXT to level 2;
home and back pauses and resumes; killed and relaunched it opens on level 2 (the save kept); 60 MB total PSS. Frame rate,
heat and sound wait for a real phone.

## 2026-11-27: 0.16.2, the first phone; giants a small squad can beat

The owner put build 32 on the OnePlus 12R (Adreno 740) and played, and the log was read from the PC: the first frame
0.83 s after main, the art loaded in 0.48 s, 91 MB of graphics and 21 MB of native memory, thermal status 0 (battery
28 C), no errors. Level 1 won with 108 (two stars); level 49 (a giants' road) lost four times: at 9% with 96 (twice), 19%
with 301 and 29% with 2,133. Giants had three seconds of the bot's own fire (102 there) and clubbed a flat 4 even from a
squad of 18, so a run that missed the x3 was clubbed flat. Now a giant has three seconds of the weaker squad's fire the
hordes are sized for (0.5-0.85 of the bot's) and below 50 soldiers a blow takes the same share as above (at least 1).
The owner then asked to make sure the bot completes every level: re-baked, `papermarch_headless --sim table` replays all
100 from the start and wins each with exactly its baked squad (13 hard ones keep 0.5-1.4%). `papermarch_headless --play
--level N --steer X` replays a level holding the squad at X (a player who never steers; such a player loses 49). Checks:
unit tests 32/32, the table 100/100, the flow test 27/27 on the PC and in the browser, the layout lint, level 1 won by
touch in the browser.

## Status

| Part | Status |
|---|---|
| The battle (rules, bot, generator, 100 baked levels, endless) | Done; the bot wins every level with the squad the bake recorded; the content pass is built in full |
| The view (road, squad, gates, barrels, hordes, boss, hazards, effects) | Done on the PC, in the browser, on an Android 15 emulator and on the owner's OnePlus 12R |
| Screens, flow, LEVELS, ENDLESS, save, updater pill | Done; flow test 27/27; layout lint clean on every screen at 4 sizes |
| Audio | Battle sounds and cues; offline renders measured (below); the owner's ears pending |
| Android | Signed releases on GitHub from every push that changes the game (`build-N`); build 32 installed and played on the OnePlus 12R |
| Browser | Builds (index.wasm 1.7 MB); headless Chrome: the flow test 27/27, level 1 won by touch, NEXT, the save; served on the LAN at port 8092 |
| Netlify | Not deployed: the owner said "just local for now" (2026-11-27) |

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

- Play notes and the rebalance: what feels too hard, too easy or too samey (levels run 0:26 at level 1 to about 14
  minutes at 100; hard levels keep about 1%)?
- Steering: too fast or too slow (`--set steer=`)? Camera: more road ahead (`camPitch`, `camHeight`)?
- Sound: shots, explosions and the march on your phone.
- A phone to check speed, memory and heat on (none attached since 2026-11-26).
- Netlify: say when, and it goes live once.
