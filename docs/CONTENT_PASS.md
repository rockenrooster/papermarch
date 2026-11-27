# PAPERMARCH — Content pass: weapons, power-ups, level variety

The owner, after 0.4.0: "Seems very repetitive we need lots of variety in the levels." "We also need a pot [lot] of weapon
variety and temporary upgrades and power-ups. With 300 levels total we have lots of options for variety so every level
feels drastically different than the last. Once you are done with this content pass we will need to do some
rebalancing."

This is the design. Numbers are starting values for the rebalance, written against today's rules (`GAME_DESIGN.md`
1-6): the musket fires `R = 5 + 2.4 * min(squad, 50)` shots a second, each doing `D = max(1, squad / 50)`.
Everything below is steered with one thumb (left and right), is deterministic, and has a one-line rule for the bot.

## 0. Principles

1. **A level is a recipe, not a longer road.** Each level has a *mode* (what winning means), one *featured* idea
   (a weapon, a hazard or an enemy the level is built round) and at most two *supporting* ideas. Section 4 fixes the
   rotation so neighbours never share a mode or a featured idea.
2. **Every new thing asks a different question of the thumb.** Where to stand (lanes, cones, blast circles), when to
   cross (hammers, tides, flip gates), what to give up (a weapon gate against a +40 gate), what to shoot first
   (bombers, drummers, mines among the horde).
3. **Readable in half a second.** Each thing has one colour family and one silhouette. Weapons are told apart by
   their projectile first (shape, colour, motion), the gun on the soldier second.
4. **The bot must be able to play it.** Each feature lists "Bot:" - the heuristic the bake's planner can use.
5. **Costs are sideways, not just down.** A weapon is better at one job and worse at another, so the choice depends on
   what the road ahead shows.

Colour families (kept apart from the gates' blue, violet and red): **weapons = white panel with an orange frame and a
black weapon icon**; **power-ups = round gold medallions with a coloured icon**; **hazards = black-and-yellow chevrons
and red warning zones on the ground**; **rescues = green**.

## 1. Weapons

One weapon at a time. A pick lasts until the next pick or the end of the level (no timers: timers belong to
power-ups). A level may start with a weapon (`W` in the level text). The squad's count still sets `R` and `D`; a
weapon multiplies them.

"Crowd" and "boss" say what the weapon is worth against a packed horde and against one big target, with the musket
as 1.0. Structures are gates, barrels, crates, cages, walls, catapults.

| # | Weapon | Rate | Damage | Range | Speed | Special | Crowd / boss | Pick it when |
|---|---|---|---|---|---|---|---|---|
| 0 | **MUSKET** (default) | R | D | 42 | 42 | one target per ball | 1.0 / 1.0 | - |
| 1 | **TWIN RIFLES** | 2.0 R | 0.6 D | 42 | 48 | two parallel streams, 0.5 apart | 1.2 / 1.2 | always a little better; the best gate-grower (2x the hits) |
| 2 | **SCATTERGUN** | 0.8 R | 5 pellets of 0.45 D in a 28 degree fan | 14 | 36 | pellets stop at the first thing hit | 1.8 near, 0 far / 1.8 once the boss is close | hordes that reach you; bad for growing far gates |
| 3 | **LONG LANCE** | 0.5 R | 1.2 D | 60 | 75 | a bolt that passes through 6 enemies (stops at structures and the boss) | 3.0 in deep columns / 0.6 | deep waves, shield walls (it pierces shields), archers at range |
| 4 | **ROCKETS** | 0.25 R | 3 D direct, 1.5 D to everything within 1.8 | 45 | 22 | slow, smoky; a gate grows by 1 per rocket | 2.5 / 0.75; 3x against walls | packed hordes, forts; bad for gates and barrels |
| 5 | **FLAMETHROWER** | continuous | every enemy in the cone takes 6 D a second (ticks of 0.6 D, 10 a second) | 9, half-angle 18 degrees | - | ignores shields; lights mines; does not grow gates | 3.0 near / 0.7 | swarms and bombers that get close; the squad must stay near the horde |
| 6 | **CANNON** | 0.12 R (at least 1 a second) | 8 D to everything on its line, 1.2 wide | 50 | 26 | a ball that rolls through everything, pushes brutes back 1.5, breaks crates and cages at once | 2.5 on a line / 1.0; 3x against walls | one lane to clear: you aim by where you stand |
| 7 | **STORM COIL** (chain lightning) | 0.6 R | 0.8 D, then 4 jumps of 0.7x each to enemies within 3 | 22, a lane 3 wide | instant | no projectile: picks the nearest enemy in its lane; ignores shields | 2.0 scattered / 0.5 | riders, bombers, loose crowds; forgiving aim |
| 8 | **MORTAR** | 0.2 R | 2.5 D to everything within 2.6 | lands 18 ahead of the squad, at its x; nothing nearer than 10 is hit | 1.1 s flight | flies over shields, walls, gates and barricades; does not grow gates | 2.5 at its distance / 0.6 | archers and catapults behind a wall; you place the blast by steering |
| 9 | **FROST BLASTER** | R | 0.5 D | 36 | 40 | each hit slows its target to 50% for 2 s (the boss to 75%) | 0.5, but the horde arrives late / 0.5 | when survival matters more than kills: hard levels, fast riders |

Rules that keep them apart: only the musket, twin rifles and frost blaster grow gates at full rate; only rockets,
cannon and mortar damage walls well; only lance, flame, coil and mortar get past shields.

**How the squad gets one**

- **Weapon gate**: a white panel with an orange frame and the weapon's icon, in a row beside ordinary gates
  (`+35 | ROCKETS`) or beside another weapon (`LANCE | SCATTERGUN`). Passing equips it. It does not grow.
- **Weapon crate**: an orange-banded crate with the icon on its lid, on the road like a barrel (hit points like a
  barrel's). Breaking it equips the weapon at once, wherever the squad is. Running into it breaks it too, at the
  barrel's cost.
- **Arsenal levels** start with the weapon already equipped.

**The look** (phase 2: one sprite set per weapon)

| Weapon | Projectile | Muzzle | Impact | On the soldier |
|---|---|---|---|---|
| Musket | cream-orange tracer (today's) | small star | spark | musket |
| Twin rifles | two thin cyan-white tracers | two small stars | cyan spark | a rifle in each hand |
| Scattergun | 5 short fat orange pellets | wide flat flash, smoke puff | 5 small dust hits | a short wide-mouthed blunderbuss |
| Long lance | a long white bolt with a blue trail (2.5 units) | blue ring | a blue line flash through each victim | a long steel tube |
| Rockets | a red-nosed rocket with a wobbling smoke trail | back-blast puff | orange fireball 1.8 wide, ring | a tube on the shoulder |
| Flamethrower | a cone of rolling orange-yellow puffs (additive), dark smoke at its end | - | enemies in it glow orange, then char | a tank on the back and a nozzle |
| Cannon | a black iron ball with a shadow, rolling | big flash and smoke ring, the squad's front row rocks back | dust bursts along the line, things fly aside | one brass cannon on wheels at the squad's front (not per soldier) |
| Storm coil | a jagged white-violet bolt drawn between targets for 0.1 s | violet crackle | violet flash on each target | a brass rod with a ball |
| Mortar | a dark shell that rises and falls, a shrinking target ring where it will land | upward puff | dirt fountain 2.6 wide, ring | a stubby tube pointing up |
| Frost blaster | pale blue snowflake bolts | white puff | ice shards; slowed enemies are tinted blue with frost on them | a blue crystal-tipped gun |

**Bot**: values a weapon gate or crate by the next two sections of road: `score = sum over upcoming waves of
enemies x crowd multiplier + boss or wall hit points x boss multiplier`, converted to soldiers saved, and compares it
with the other gates of the row as it does today. The planner already tries every row choice.

## 2. Power-ups

Timed or instant boosts. They stack with any weapon and with each other; picking the same one again restarts its timer.
Two carriers:

- **Medallion**: a gold disc, 1.2 units across, spinning 1 unit over the road with a soft light column over it. Touch
  it with the squad's footprint.
- **Supply crate**: a wooden crate with a gold star band, 6-20 hit points; shoot it open (running into it also opens
  it, at no cost: it is a gift, not a barrel).

A running power-up shows as an icon with a draining ring in a row under the progress road, and as an effect on the
squad itself.

| # | Power-up | Carrier | Effect | Lasts | Look | Sound |
|---|---|---|---|---|---|---|
| 1 | **RAPID FIRE** | medallion, crate | fire rate x2 | 6 s | orange bolt icon; muzzle flashes double, tracers turn orange-hot | a rising ratchet, then the musket volley at double tempo, a short "ting" at the end |
| 2 | **POWER SHOT** | medallion, crate | damage x2; projectiles drawn 1.5x bigger | 6 s | red fist icon; red glow on the guns | a deep drum hit, each shot gets a low thump layer |
| 3 | **SHIELD** | medallion | the next losses are absorbed: up to max(10, 30% of the squad) soldiers | 8 s or until spent | blue shield icon; a pale blue dome over the squad that flashes and cracks as it absorbs | a glassy bell chord on pick-up; a dull "tonk" per absorbed hit; a shatter when spent |
| 4 | **FREEZE** | crate | every enemy within 45 units stops (the boss for half as long); projectiles in flight too | 3 s | ice-blue snowflake icon; enemies turn pale blue with ice blocks at their feet, a white ring sweeps up the road | a quick descending glass glissando, then a held shimmer; cracks as it ends |
| 5 | **AIRSTRIKE** | crate | three bombs fall across the road 12, 20 and 28 units ahead, 0.6 s apart; each does 12 D to everything within 3 | instant | dark bomb icon; three shadows grow on the road, then three big explosions with rings | a falling whistle per bomb (pitch dropping), three booms, each lower |
| 6 | **MAGNET** | medallion | barrels, supply crates and cages within 25 units ahead open by themselves, one every 0.3 s; recruits fly to the squad | 5 s | red-and-white horseshoe icon; arcs of gold sparks from the squad to each barrel | a wobbling hum, a coin "plink" per barrel |
| 7 | **GIANTS** | medallion | the squad is drawn as a third as many soldiers at 2x size; enemies that touch it die and take nothing; barrels run into cost nothing; the boss's crush is halved | 5 s | green arrow-up icon; a green flash, dust under the feet | a low brass swell; footsteps become deep thuds; a slide-whistle down at the end |
| 8 | **REINFORCEMENTS** | crate | max(8, 15% of the squad's peak) soldiers parachute in over 2 s | instant | blue parachute icon; small parachutes drift down onto the squad | a snare roll and a two-note bugle |
| 9 | **QUICK MARCH** | medallion | march speed x1.6, steering x1.5 | 5 s | white winged-boot icon; speed lines, more dust | the march music's tempo jumps; a whip-crack on pick-up |
| 10 | **HOURGLASS** | medallion, crate | enemies, their projectiles and moving hazards run at 40% speed; the squad and its bullets do not | 5 s | violet hourglass icon; a violet tint at the screen's edges, enemies leave after-images | everything but the guns drops an octave; a slow tick-tock |
| 11 | **DOUBLE GATES** | medallion | the next add gate passed counts twice (a barricade's loss does not) | until used, 12 s at most | gold "x2 +" icon; gates ahead get a gold rim | a cash-register "ka-ching" on pick-up, a second one when used |
| 12 | **FIELD GUN** | crate | a brass cannon rolls beside the squad and fires a cannonball (section 1, row 6) every 1.0 s | 8 s | cannon icon; the cannon sprite, at the squad's right edge | wheels rattling, the cannon's boom |

**Bot**: a medallion is worth a fixed number of soldiers by kind (start: rapid 0.2, power 0.2, shield 0.3, magnet = the
rewards within reach, giants 0.25, quick 0.05, hourglass 0.2, double gates = the next add gate's value; each times the
squad's size where no reward is named); it steers to it when no gate row is nearer and the detour costs less. A crate
is a barrel whose reward is that worth.

## 3. Level variety

### 3.1 New enemies

| # | Enemy | Numbers | How it plays | Look | Bot |
|---|---|---|---|---|---|
| E1 | **ARCHER** | hp 1, stands still in ranks; from 18 units it looses an arrow every 1.6 s that lands on the squad after 0.8 s and takes 1 soldier | pressure from a distance: kill them early with range (lance, mortar, musket) or lose a steady trickle; a scattergun squad bleeds | a red raider with a hood and a tall bow; arrows are thin dark arcs with small shadows | counts each archer in range as -0.6 soldiers a second; prefers the lane that shoots them first |
| E2 | **SHIELD BEARER** | hp 3 behind a shield of 14; speed 2.4; strength 2; bullets from the front hit the shield | the front rank of a wave: the wave arrives nearly whole unless the weapon gets past shields | a red raider behind a tall steel-and-red tower shield with a gold boss; the shield cracks in two stages | treats shield hit points as the wave's; values lance, flame, coil, mortar higher |
| E3 | **BOMBER** | hp 2, speed 6.5, strength 8; when it dies it explodes: 10 D to enemies within 2.2 (other bombers chain) | shoot it while it is inside the horde and it clears a hole; let it arrive and it costs 8 | a small raider carrying a black powder keg with a lit fuse over its head, orange spark; it blinks faster as it nears | targets bombers first when they are within a wave (the nearest-first rule gives way to "a bomber within 2.2 of 4 or more enemies") |
| E4 | **GIANT** (mini-boss) | hp = 35% of the level's boss budget, speed 1.8, radius 1.3, strength 25; drops a medallion when it falls | a wall of meat mid-level: everything behind it catches up while you chew through it | a brute at twice the size with a wooden club and a banner on its back; health bar over its head | as the boss: weighs 8x |
| E5 | **CATAPULT** | hp 40 hits (a structure); every 3.2 s it hurls a boulder at where the squad stands: a red circle (radius 1.7) shows 1.2 s before it lands; soldiers inside are lost (at most 14) | dodge sideways when the circle appears; kill it from range or march past it | a timber siege engine with a red flag, at the road's side (x = +-3.5), its arm swinging; the boulder has a growing shadow | when a circle exists, steers to the nearest x outside it (this beats every other goal) |
| E6 | **DRUMMER** | hp 4, speed as the wave; enemies within 5 units of it move 40% faster | kill-first target: the wave slows when it falls | a raider with a big red-and-gold drum; rings of sound pulse from it; hurried enemies lean forward with speed lines | weighs 4x as a target |

Bosses gain **attacks** (one or two per world, section 4.3); each is telegraphed on the ground and dodged sideways:

| Attack | Numbers | Look | Bot |
|---|---|---|---|
| **CLUB SLAM** | every 4.5 s while within 14 units: one half of the road (left or right, the half the squad's centre is in) turns red for 1.0 s, then the club lands: soldiers in that half are lost, at most 12% of the squad | the ogre raises its club, the half glows red with chevrons, dust and a ring on impact | cross to the other half as soon as the zone shows |
| **BOULDER TOSS** | every 3 s while further than 14 units: the catapult's rule (circle radius 2.0, at most 10% of the squad) | the ogre rips up a rock; shadow and circle | as the catapult |
| **WAR CRY** | at 66% and 33% health: 16 foot (24 on hard levels) run in from behind it | the ogre roars (a ring of red sound), the escort charges | nothing special |
| **STOMP WAVE** | every 5 s: a wave rolls down the road with a gap 3 units wide at a seeded x; soldiers outside the gap are lost, at most 10% | a ridge of dust with a clear gap, an arrow on the road pointing at the gap | steer into the gap |

### 3.2 Hazards and obstacles

| # | Thing | Numbers | How it plays | Look | Bot |
|---|---|---|---|---|---|
| H1 | **ROLLER** | a spiked log 2.4 wide rolling down one lane at 7 units a second; soldiers in its lane are lost (at most 40% of the squad); cannot be shot; its lane is marked from 30 units away | change lanes; two or three in a row make a slalom | a dark timber log with iron spikes, turning; yellow-black chevrons on its lane; dust | leaves the marked lane before it arrives |
| H2 | **HAMMER** | a pendulum at a fixed z whose head (1.8 wide) swings across the road, `x = 4 sin(2 pi t / 2.4)`; soldiers under the head as the squad crosses are lost (at most 25%) | cross where the head is not: watch the swing, pick the side | a wooden frame over the road, a huge striped mallet; its shadow sweeps the road | computes the head's x at the crossing time and stands furthest from it |
| H3 | **MINE** | a disc of radius 0.5; touched: 6 soldiers; shot (3 hits): a blast of 10 D to enemies within 2.5 | avoid them, or shoot them when the horde walks over them: minefields in front of a wave are a gift | a black iron dome with red studs and a blinking red light; orange explosion | avoids; counts a mine within 2.5 of enemies as a free kill target |
| H4 | **PIT / BROKEN BRIDGE** | a rectangle of missing road (one side, 3-5 wide, 5-9 long); the share of the footprint over it falls | be on the solid side; pits alternate sides, sometimes with a gate on the far side as bait | the road's edge crumbles away to the land (or water, clouds, lava by world); broken planks, warning posts | keeps the footprint off it |
| H5 | **NARROWS** | the road is 4 wide for 10-16 units; the footprint stretches along the road instead (no loss), so fewer guns reach the front: fire rate x0.6 while inside | enemies funnel in; a lance or cannon is at its best here | a bridge with railings (world's own: rope bridge, boardwalk, marble span, lava causeway) | nothing; the planner sees the fire-rate cut |
| H6 | **MOVING GATE** | a gate sliding left and right, amplitude 3, period 3 s | line up with where it will be | the gate's posts on little wheels, a rail across the road | predicts its x at the crossing time |
| H7 | **FLIP GATE** | a gate whose value alternates between two shown values every 1.5 s (`+30` / `-10`); the next value shows small in its corner | arrive while it is good: slide to a side lane to pass beside it when the timing is wrong (it covers half the road) | the panel spins on a horizontal axis like a rotating sign, blue one side, red the other | computes its value at the crossing time; passes beside it if bad |
| H8 | **MYSTERY GATE** | shows `?`; holds one of two shown outcomes (`+40 or -15`, seeded); 5 hits reveal it | spend shots to look before you leap | a violet-grey panel with a big `?` and the two outcomes small beneath; it flips to its true face when revealed | the planner knows the outcome; the in-game bot shoots it first if a lane allows |
| H9 | **WIND / CURRENT** | a strip 10-20 long that pushes the squad sideways at 3 units a second | hold against it; gates sit upwind | streaks of air (sand, snow, petals, embers by world) blowing across; arrows on the road | adds the push to its steering |
| H10 | **ICE / MUD** | a strip where steering is a third as quick (ice) or the march is half as fast (mud) | commit to a lane early (ice); the horde gets more time to reach you (mud) | a pale blue sheen with sparkles; brown puddles with splashes | looks 3x further ahead on ice |
| H11 | **TIDE** (lava, flood) | one half of the road is covered for 3 s out of every 6 (a 1 s warning shimmer first); soldiers in it are lost at 20% of the squad a second | be on the dry half as the squad passes; it also sweeps enemies away (a free kill of what stands in it) | water (or lava, or cloud-gap) sliding in from the side with a foam edge | as the hammer |
| H12 | **FORT WALL** | a wall across the whole road with hit points (5-8 s of the squad's fire); the march stops at it until it falls; rockets, cannon and mortar do 3x; archers stand behind it | a damage race while arrows fall; the mortar clears the archers first | a stone wall with a red gate and battlements, cracking in three stages, then collapsing in blocks | fires at it; the planner prefers siege weapons before it |
| H13 | **FOG / NIGHT** | things further than 22 units are hidden (fog) or dark (night); gate numbers show from 16 units | read fast; in the dark, muzzle flashes and explosions light the road | a soft fog wall in the world's haze colour; at night a dark blue grade with a pool of light round the squad, eyes of the horde glowing first | none (the bot knows the level) |
| H14 | **CAGE** (rescue) | prisoners behind bars; bullets pass through it; touch it to free them (reward 10-60); enemies guard it | the opposite of a barrel: you must steer into it, through whatever stands beside it | a green iron cage with blue soldiers waving inside, a green beam of light over it | as a gate worth its reward |

### 3.3 Modes

| # | Mode | Wins when | Recipe | Stars |
|---|---|---|---|---|
| M1 | **MARCH** | the finish line is crossed | today's level: gates, barrels, waves, plus the level's featured idea | soldiers left |
| M2 | **BOSS** | the ogre falls | today's boss level, with the world's boss attacks | soldiers left |
| M3 | **SIEGE** | the last fort wall falls | 2-3 walls with archers and catapults; weapon gates before each | soldiers left |
| M4 | **RUSH** | the finish line is crossed | the march is 1.5x as fast; hazards and gates only, almost no horde; 15-20 s | soldiers left |
| M5 | **RESCUE** | the finish line is crossed | start with 3; cages (big rewards) guarded by waves; few gates | soldiers left (they are the rescued) |
| M6 | **GOLD RUSH** (bonus, cannot be lost) | the finish line is crossed | no enemies; many gates, moving and flip gates, barrels, medallions; red barricades cannot take the last 3 soldiers | squad size against the plan's |
| M7 | **LAST STAND** | 30 s pass | the squad holds a bridgehead and does not march; waves come from the front in growing sizes; supply crates parachute in every 6 s; the player aims by steering | soldiers left |
| M8 | **ARSENAL** | the finish line is crossed | starts with one weapon; the road is built to show it off (its best case, then its worst) | soldiers left |
| M9 | **GAUNTLET** | the finish line is crossed | hazards only: rollers, hammers, mines, pits, tides in a rhythm; a few + gates as bait | soldiers left |
| M10 | **GIANTS' ROAD** | the last giant falls | two or three giants with escorts, a power-up from each | soldiers left |

## 4. The 300-level plan

### 4.1 The recipe of a level

`mode + featured idea + up to two supporting ideas + the world's dressing`. An idea is any numbered row above (weapon
1-9, power-up 1-12, E1-E6, H1-H14). The generator picks from the ideas the level number has unlocked (4.2).

**Rotation rules** (checked by the generator; a level that breaks one is re-drawn):

1. No two neighbouring levels share a mode.
2. No two neighbouring levels share a featured idea, and a featured idea does not come back within 6 levels.
3. A level shares at most one supporting idea with the level before it.
4. A starting weapon or the first weapon offered differs from the previous level's.
5. In every block of ten: one BOSS (the 10th), one GOLD RUSH (the 5th), and at least five different modes among the
   other eight. MARCH fills at most three of the ten.
6. HARD levels (as today, about one in nine) are MARCH, SIEGE, GIANTS' ROAD or BOSS, never GOLD RUSH or RUSH.

The block template the generator starts from, then permutes by world (slots 5 and 10 stay):

| Slot | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| Mode | MARCH (the block's new idea) | ARSENAL | RESCUE | GAUNTLET | GOLD RUSH | MARCH | SIEGE or LAST STAND | RUSH | GIANTS' ROAD | BOSS |

Worlds 1 and 2 use modes only as they unlock (4.2); before a mode is unlocked its slot is a MARCH with a different
featured idea.

### 4.2 Introductions

Levels 1-8 stay as they are (steer, gates, growing, barrels, x2, barricades, riders, the first boss). From 9 on, one
new idea on the level shown, alone on an easy road, with its hint line; from then on it is in the pool.

| Level | New | Hint line |
|---|---|---|
| 9 | weapon gate: TWIN RIFLES | `GRAB A NEW WEAPON` |
| 11 | medallion: RAPID FIRE | `TOUCH MEDALS FOR POWER` |
| 12 | brutes (as today) | `BRUTES TAKE MANY HITS` |
| 13 | mines | `SHOOT MINES NEAR THE HORDE` |
| 14 | SCATTERGUN, weapon crate | `SHOOT CRATES OPEN` |
| 15 | mode GOLD RUSH | `BONUS ROAD: GRAB IT ALL` |
| 16 | barrels cost (as today) | `DON'T RUN INTO BARRELS` |
| 17 | archers | `ARCHERS SHOOT BACK` |
| 18 | SHIELD | `THE SHIELD SAVES SOLDIERS` |
| 19 | moving gates | `GATES ON THE MOVE` |
| 21 | catapults (world 2) | `DODGE THE RED CIRCLE` |
| 22 | LONG LANCE; mode ARSENAL | `THE LANCE GOES THROUGH` |
| 23 | supply crate: AIRSTRIKE | `CALL THE AIRSTRIKE` |
| 24 | cages; mode RESCUE | `FREE THE PRISONERS` |
| 26 | shield bearers | `SHIELDS STOP BULLETS` |
| 27 | rollers | `LEAVE THE MARKED LANE` |
| 28 | ROCKETS | `ROCKETS BLAST CROWDS` |
| 29 | giant; POWER SHOT | `A GIANT BLOCKS THE ROAD` |
| 30 | boss attack: CLUB SLAM | `DODGE THE CLUB` |
| 31 | ice (world 3) | `ICE: STEER EARLY` |
| 32 | FROST BLASTER; FREEZE | `FREEZE THE HORDE` |
| 33 | fort wall; mode SIEGE | `BREAK THE WALL` |
| 34 | hammers; mode GAUNTLET | `MIND THE HAMMER` |
| 36 | MORTAR | `MORTARS FLY OVER WALLS` |
| 37 | bombers | `SHOOT BOMBERS IN THE CROWD` |
| 38 | mode RUSH; QUICK MARCH | `RUN FOR IT` |
| 39 | flip gates | `WAIT FOR THE BLUE SIDE` |
| 41 | FLAMETHROWER (world 4) | `GET CLOSE AND BURN` |
| 42 | drummers | `SILENCE THE DRUMMER` |
| 43 | pits | `STAY ON THE ROAD` |
| 44 | MAGNET | `THE MAGNET OPENS BARRELS` |
| 46 | mode LAST STAND; REINFORCEMENTS | `HOLD THE LINE` |
| 47 | narrows | `THE BRIDGE IS NARROW` |
| 48 | CANNON | `LINE UP THE CANNON` |
| 49 | mode GIANTS' ROAD; GIANTS | `GIANTS STOMP THE HORDE` |
| 50 | boss attack: BOULDER TOSS | `WATCH THE SKY` |
| 51 | tide (world 5) | `MIND THE TIDE` |
| 53 | STORM COIL | `LIGHTNING JUMPS` |
| 54 | HOURGLASS | `SLOW THEM DOWN` |
| 56 | mystery gates (world 6) | `SHOOT TO REVEAL` |
| 58 | DOUBLE GATES | `NEXT GATE COUNTS TWICE` |
| 60 | boss attack: WAR CRY | `THE OGRE CALLS FOR HELP` |
| 62 | wind | `LEAN INTO THE WIND` |
| 64 | FIELD GUN | `A CANNON JOINS YOU` |
| 67 | fog (world 7) | `FOG HIDES THE ROAD` |
| 70 | boss attack: STOMP WAVE | `FIND THE GAP` |
| 81 | night (world 9, shown early in world 8's last level as a storm) | `FIGHT IN THE DARK` |

After level 81 every idea is in the pool. Levels 82-300 mix them; what changes with depth is how many ideas a level
may hold (featured + 2 supporting up to 150, + 3 after) and the worlds' own sets below.

### 4.3 What each world favours

World n = levels 10n-9..10n and every later cycle (levels 101-200 and 201-300 revisit worlds 1-10 with all ideas
unlocked, so the second and third visits to a world differ from the first by their mixes).

| World | Favours (double weight) | Never | Boss attacks | Dressing of the generic hazards |
|---|---|---|---|---|
| 1 GREENMEADOW | twin rifles, scattergun, mines, moving gates, cages | fog, night, tide | club slam | pits = broken paving to grass; wall = wooden palisade |
| 2 SUNBAKED | archers, catapults, lance, wind (sandstorm), fort walls | ice | boulder toss | pits = sinkholes; rollers = stone drums |
| 3 FROSTVALE | ice, frost blaster, freeze, fog (blizzard), shield bearers | tide | club slam, stomp wave (an avalanche ridge) | rollers = giant snowballs; pits = cracked ice to dark water |
| 4 EMBERFALL | bombers, flamethrower, rollers (hay bales), drummers, rescue | ice | war cry, club slam | mines = pumpkin bombs; wall = hay-bale stockade |
| 5 TIDEREACH | tide, narrows (boardwalks), cannon, magnet, rush | fog | boulder toss (anchors), stomp wave (a breaker) | pits = missing planks over water |
| 6 BLOOMGROVE | mystery and flip gates, double gates, cages, storm coil, gold-rush-like marches | night | war cry | rollers = thorn balls; wind = petal gusts |
| 7 MIREFEN | fog, pits (bog holes), mud, storm coil, bombers, giants | wind | stomp wave, war cry | tide = rising bog water; mines = puffball spores (green blast) |
| 8 SKYHOLD | narrows and pits (sky bridges), wind, mortar, hammers (gold pendulums), last stand | mud | boulder toss, club slam | pits open onto clouds; tide = a drifting cloud bank that hides, not kills |
| 9 CINDERPEAK | night, tide (lava), mines, rockets, giants, siege | ice, mud | every attack, two per fight | rollers = burning boulders; pits glow |
| 10 LANTERNFAIR | everything; flip gates, field gun, airstrike (fireworks), gold rush twice a block | - | two attacks, picked per level | fireworks for explosions; hammers = fairground mallets |

### 4.4 A sample block (levels 41-50, Emberfall), to show the rotation

| Level | Mode | Featured | Supporting | Weapon offered |
|---|---|---|---|---|
| 41 | MARCH | flamethrower (new) | barrels, riders | flamethrower gate |
| 42 | MARCH | drummers (new) | mines, rapid fire | twin rifles crate |
| 43 | GAUNTLET | pits (new) | rollers, + gates as bait | none |
| 44 | RESCUE | cages + magnet (new) | bombers | scattergun |
| 45 | GOLD RUSH | flip gates | moving gates, double gates | none |
| 46 | LAST STAND (new) | reinforcements | bombers, drummers, airstrike crates | rockets from the start |
| 47 | MARCH | narrows (new) | shield bearers | lance against rockets (a row) |
| 48 | ARSENAL | cannon (new) | narrows, giants | cannon from the start |
| 49 | GIANTS' ROAD (new) | giants power-up | brutes, power shot | mortar crate |
| 50 | BOSS (hard) | boulder toss (new) + war cry | bombers escort, shield medallion | flamethrower or frost blaster (a row) |

## 5. What each feature needs

Art: sprites in the usual contract (`sprites.txt`), elevations 60/38/20 for things on the road unless noted; "fx" =
effect flipbook, elevation 0. Rules: what `src/sim` must add.

### 5.1 Weapons

| Feature | Sprites and effects (mine) | Rules (yours) |
|---|---|---|
| Weapon as a state | per weapon: `wpn.<name>.march` (the gun as an overlay on the soldier, rendered with the soldier as a hold-out, 8 frames, 3 elevations, with a `muzzle` point); `soldier.march` re-rendered without its musket (the musket becomes `wpn.musket`); `ui.wpn_<name>` icons (40 dp) for gates, crates and the HUD | `Battle::weapon`; a table of rate, damage, range, speed, flags (pierce n, splash r, cone, line, chain, lob, slow); `fire()` and `moveBullets()` per kind; the level text's start weapon |
| Weapon gate | `gate.post_wpn` (white-orange post), panel colours (white `#FFF6E0` body, orange `#FF8A1E` frame, ink icon) for the panel shader, icon drawn from `ui.wpn_*` | gate kind `kGateWeapon` with a weapon id; no growth; the bot's valuation (section 1) |
| Weapon crate | `crate.wpn` (orange bands, 1.3 units, 3 elevations) + the icon on top by the engine; `fx.crate_break` | a barrel with `weapon` instead of `reward` |
| Twin rifles | `fx.tracer_twin`, `fx.hit_cyan` | two bullets per shot, offset |
| Scattergun | `fx.pellet`, `fx.muzzle_wide`, `fx.puff_small` | 5 bullets in a fan, range 14 |
| Long lance | `fx.bolt` (long), `fx.muzzle_ring`, `fx.pierce` | bullet with a pierce count |
| Rockets | `fx.rocket` (8 frames, wobble), `fx.smoke_trail`, reuse `fx.explosion` at 0.8x | slow bullet, splash on impact, 3x against walls |
| Flamethrower | `fx.flame` (16 frames, additive puffs for the cone), `fx.char` (a dark puff on kills); a tint for burning enemies | cone test per enemy per tick |
| Cannon | `squad.cannon.march` (the brass cannon, 8 frames, 3 elevations, `muzzle` point), `fx.cannonball` (rolling, 8 frames) + shadow, `fx.muzzle_big`, `fx.dust_line` | line projectile hitting everything it overlaps once; knock-back; also used by FIELD GUN |
| Storm coil | `fx.lightning` (4 frames of a jagged bolt drawn as a stretched quad between two points, additive), `fx.zap` | instant target pick in a lane, chain to neighbours |
| Mortar | `fx.shell` (up and down), `fx.target_ring` (ground ring), `fx.dirt_burst` | lobbed shot with a flight time and a landing point; ignores blockers |
| Frost blaster | `fx.frost_bolt`, `fx.ice_shards`, `fx.frost_overlay` (ice at an enemy's feet) | slow flag and timer per enemy; blue tint |

### 5.2 Power-ups

| Feature | Sprites and effects | Rules |
|---|---|---|
| Medallion, supply crate | `pickup.medal` (spinning, 8 frames, 3 elevations), `fx.light_column` (additive), `crate.supply` (gold star band), `fx.pickup` (burst); `ui.pow_<name>` x12 (icons, 36 dp, also drawn on the medallion and crate) | `PickupDef` (x, z, kind, carrier, hp); touch test; active-effect timers in `Battle`; events for the view and sounds; HUD timer ring (yours, section 15 style: a 36 dp cream disc, ink outline, a cobalt ring draining clockwise) |
| Rapid fire, power shot | `fx.tracer_hot` (orange), a red glow sprite `fx.gun_glow` | rate and damage multipliers |
| Shield | `fx.shield_dome` (a pale blue dome, 8 frames, additive, scaled to the footprint), `fx.shield_crack`, `fx.shield_break` | absorb pool and timer |
| Freeze | `fx.frost_ring` (ground ring), reuse `fx.frost_overlay` | enemy and projectile stop timer |
| Airstrike | `fx.bomb` (falling, with a growing ground shadow `fx.shadow_grow`), reuse `fx.bigboom` at 0.5x | three timed splashes |
| Magnet | `fx.magnet_arc` (gold sparks along a stretched quad), `fx.recruit_fly` (a small blue soldier tumbling) | auto-open of barrels, crates, cages in range |
| Giants | none new: soldiers drawn at 2x, a third as many; `fx.stomp_dust` | contact rule changes, boss crush x0.5 |
| Reinforcements | `fx.parachute` (a small soldier under a blue-white canopy, 8 frames) | timed additions |
| Quick march | `fx.speed_lines` (additive streaks) | speed multipliers |
| Hourglass | a violet vignette (shader, `#9B4DFF` at 25% on the outer 30%), `fx.afterimage` not needed (draw enemies twice at 40% alpha, offset back) | time scale for enemies, their projectiles and hazards |
| Double gates | `fx.gate_gold_rim` (a gold frame overlay for panels) | a flag consumed by the next add gate |
| Field gun | reuse `squad.cannon` | a timed extra shooter |

### 5.3 Enemies and boss attacks

| Feature | Sprites and effects | Rules |
|---|---|---|
| Archer | `archer.idle` and `archer.shoot` (8 frames each), `fx.arrow` (in flight, drawn along an arc), small ground shadow | static enemy with a ranged attack timer; arrow flight and landing |
| Shield bearer | `shieldman.march`; `shield.front` overlay in 3 damage stages (`.0/.1/.2`) | shield hit points, frontal block, bypass flags per weapon |
| Bomber | `bomber.march` (keg overhead, fuse spark), reuse `fx.explosion` at 0.6x | death blast, chain, strength 8 |
| Giant | `giant.march` (rendered at 2x the brute's resolution), its health bar (yours) | enemy kind with scaled hit points and a pickup drop |
| Catapult | `catapult.idle`, `catapult.fire` (8 frames), `fx.boulder` (tumbling) + `fx.shadow_grow`, `fx.warn_circle` (red ring with chevrons, additive pulse), `fx.dirt_burst` | structure enemy; targeted area attack with a warning time |
| Drummer | `drummer.march`, `fx.sound_ring` | aura speed-up |
| Club slam, boulder toss, war cry, stomp wave | `boss.slam` (8 frames, club overhead and down), `boss.throw` (8 frames), `boss.roar` (8 frames), each with matching add-on frames for worlds 2-10 only if the add-on moves (most are on the head: I will render them); `fx.warn_half` (red chevron zone for half the road, tiled), `fx.wave_ridge` (a dust ridge segment, tiled across the road), `fx.arrow_ground` | boss state machine with timed attacks and their hit tests |

### 5.4 Hazards, obstacles, modes

| Feature | Sprites and effects | Rules |
|---|---|---|
| Roller | `haz.roller.roll` (8 frames; world variants as recolours or small re-renders: stone drum, snowball, hay bale, thorn ball, burning boulder), `fx.lane_chevrons` (a tiled ground decal, black-yellow) | lane object with a speed and a loss rule; marked from 30 units |
| Hammer | `haz.hammer_frame.still` (the gantry), `haz.hammer_head.still` (the mallet, drawn at its swing position with a scale for height), its shadow | pendulum position by time; loss rule on crossing |
| Mine | `haz.mine.still` (2 frames: light off / on), reuse `fx.explosion` at 0.7x | touch loss, hit points, blast |
| Pit / broken bridge | per world a `ground_wN_pit.png` edge strip (256x64, tiled along the pit's rim) and the land texture showing through; `haz.warn_post.still` | road mask rectangles; footprint overlap loss; the ground shader cuts the road (yours) |
| Narrows | per world `wN.rail.still` (a railing segment, an edge prop placed along the narrow part) | road half-width by z; squad footprint stretch; fire-rate factor |
| Moving gate | `gate.rail.still` (a rail segment across the road), wheels drawn on the posts (`gate.post_*` get a wheeled variant `gate.post_add_w` etc.) | gate x as a function of time |
| Flip gate | panel shader: a rotation about the horizontal axis (scale y by cos) between two faces; no new sprites | gate value by time; half-road width |
| Mystery gate | panel colours (violet-grey `#8A84B8`), `ui.question` glyph if the font's `?` is not bold enough | hidden value, reveal by hits |
| Wind | `fx.wind_streak` (additive streaks, world tints) and reuse of the world's ambient particles blown sideways; `fx.arrow_ground` | sideways push by z range |
| Ice / mud | `ground_ice.png`, `ground_mud.png` (256x256 overlays blended on the road), `fx.splash` | steering and speed factors by z range |
| Tide | `haz.tide_edge.play` (a foam edge strip, 8 frames) over a tinted quad (water `#3E9CCC`, lava `#FF7A1A` additive, bog `#3A6A4A`), `fx.warn_half` | half-road timed zone, loss rate, kills enemies in it |
| Fort wall | `haz.wall.still` in 3 damage stages + `haz.wall_gate` (2 units wide segments tiled across the road, 2.4 tall), `fx.wall_collapse` (blocks), world variants later (palisade, hay bales) | structure across the road with hit points; march halts; weapon multipliers |
| Fog / night | fog: the haze `start`/`end` set per level (no asset); night: a grade (`light 0x5A6AB0 0.55`), a light pool sprite `fx.light_pool` (additive, under the squad), `fx.eyes` (two yellow dots for far enemies) | per-level visibility flags (view only) |
| Cage | `cage.still` (green bars, 3 elevations), `cage.prisoner.wave` (8 frames, a soldier waving, drawn inside), `fx.light_column` in green, `fx.cage_open` | a touch-opened barrel that bullets pass through |
| Gold rush | `ui.banner_bonus` (a gold ribbon text is enough), coins as confetti on gates (`fx.coin`, 8 frames) | no-loss floor of 3; no enemies; stars from squad size |
| Last stand | `haz.barricade_line.still` (sandbags the squad stands behind), reuse `fx.parachute` for crates | march speed 0, win on a timer, timed wave and crate spawns, a countdown in the HUD |
| Siege, rush, rescue, arsenal, gauntlet, giants' road | nothing beyond the parts above; a mode chip beside the level chip (`ui.mode_<name>` icons, 24 dp: wall, boot, cage, gun, hammer, giant, coin, clock) | win conditions; generator recipes; rotation rules (4.1) |

### 5.5 Budget

- Sprites a frame: weapon overlays double the squad's quads (150 to 300); flame and shield are under 60 quads; a
  full archer wave adds arrows (at most 1 per archer in flight). Worst case stays under 5,000.
- Art: about 1.5 MB for weapon overlays and icons, 2.5 MB for the six enemies, 1.5 MB for hazards, 0.8 MB for
  effects, 1.2 MB for boss attack frames: about 7.5 MB, to about 24 MB in all (limit 28).

## 6. Order of building (a suggestion: each step is playable and bakeable on its own)

1. Weapons as a state, weapon gates and crates, weapons 1-4 (twin, scattergun, lance, rockets). The biggest change in
   feel for the least new geometry.
2. Medallions and supply crates with power-ups 1-5 (rapid, power, shield, freeze, airstrike).
3. Mines, archers, moving gates, cages; modes GOLD RUSH, RESCUE, ARSENAL.
4. Catapult and the boss attacks (they share the warning-circle rule); giants.
5. Shield bearers, bombers, drummers; weapons 5-9; the remaining power-ups.
6. Rollers, hammers, pits, narrows, tide, walls; modes SIEGE, GAUNTLET, RUSH, LAST STAND, GIANTS' ROAD.
7. Flip and mystery gates, wind, ice, mud, fog, night.
8. The rotation rules and the introduction schedule in the generator; then the rebalance.
