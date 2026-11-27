# PAPERMARCH — Game design

The numbers here are the code's (`src/sim/battle.cpp`, `src/gen/gen.cpp`, `src/game/*`); when they change, change this.

## 1. The road

A level is a straight road along z (units; the squad starts at z = 0), x in [-5, 5]. The squad marches up it at the
level's speed (4.6 + 0.008·min(n, 120) units a second, level n). The road's length is 96 + 0.7·min(n, 150) units, 24
more on a boss level. A level without a boss is won when the squad's centre crosses the finish line (a band of checks
painted across the road at its length). On a boss level the ogre stands at the end and the level is won when it falls.

Nothing moves until the player touches the road ("DRAG TO MARCH"). Then:

## 2. The squad

| Property | Value |
|---|---|
| Size | a number, from the level's start (3 on levels 1-5, 3-6 after) to 99,999 |
| Footprint | a disk of radius min(4.4, 0.55 + 0.30·√squad) |
| Steering | the squad follows the finger: x moves toward the target at 12·(distance) units a second, at most 18; it stays on the road. A drag across the road's width on screen moves it 1.25 road widths (`steer`) |
| Fire | the musket: 5 + 2.4·min(squad, 50) shots a second, from random points across the squad's front, each at the nearest enemy that still needs one; a ball flies 42 units a second for 48 units |
| Damage | a musket ball does max(1, squad / 50) (counted in tenths: every soldier past 50 adds a little) |
| Weapon | one at a time, from a weapon gate or crate (or the level's start), until the next or the level's end; it multiplies the musket's fire (below) |

| Weapon | Shots | Damage a ball | Range | Its trick |
|---|---|---|---|---|
| Musket | 1× | 1× | 48 | - |
| Twin rifles (from level 9) | 1× pairs | 0.6× each | 48 | two streams 0.5 apart: 1.2× in all, the best gate grower |
| Scattergun (14) | 0.8× | 5 pellets of 0.45× in a 28° fan | 14 | murderous up close, useless far; only the middle pellet grows a gate |
| Long lance (22) | 0.5× | 1.2× | 60 | the bolt passes through six enemies (stops at the boss, gates, barrels) |
| Rockets (28) | 0.25× | 3× | 45 | slow; bursts on impact: 1.5× to everything within 1.8 |

Drawn: up to 150 painted toy soldiers in a sunflower pattern inside the footprint, bobbing as they march, each with its
weapon's gun and muzzle flash; the squad's banner over them with the full number; the weapon's icon in the HUD.

## 3. Gates

A gate is a panel across part of the road at some z, with a value and a kind:

- **add**, value > 0: blue, `+N`. Passing it adds N.
- **add**, value ≤ 0: a red barricade, `-N`. Passing it takes N (it can take the whole squad).
- **multiply**: violet, `×N`. Passing it multiplies.
- **weapon**: a cream panel in an orange frame with the weapon's icon and name. Passing it equips that weapon; it never
  grows. It stands beside a `+N` gate or another weapon: the player trades soldiers for firepower.

A **weapon crate** (orange bands, the icon over it) stands on the road like a barrel: shot open (or run into, at a
barrel's cost) it equips its weapon. One weapon is on offer on most levels from 9 on, never the same as the level
before; levels 9, 14, 22 and 28 bring each new one in with a hint line.

The squad passes the gate its centre is in when its centre crosses the gate's z; the other gates of that row are left
behind (drawn faded). Gates that **grow** (most add gates and barricades) take one per bullet that hits them, up to
their cap: shooting a `+3` gate makes it a `+5`, shooting a `-20` barricade can make it a `+3`. Multipliers never grow.
Bullets stop at the first growing gate, barrel or enemy in their path.

## 4. Barrels

A gold barrel stands on the road with hit points and a reward (`+N`, shown over it with a bar of its strength left).
Each bullet does the squad's damage; at zero it bursts and the reward joins the squad. A barrel the squad runs into
bursts too and costs max(1, hp / 2) soldiers.

**Power-ups** (docs/CONTENT_PASS.md 2) come on a gold **medallion** spinning over the road (the squad's footprint
touching it takes it) or in a **supply crate** (gold star bands: shot open, or run into at no cost). They stack with
the weapon and each other; taking one again restarts its clock. The HUD shows each running one as its icon in a
draining ring under the progress road.

| Power-up | From level | Carrier | Effect |
|---|---|---|---|
| Rapid fire | 11 | medallion, crate | twice the shots for 6 s (orange-hot tracers) |
| Shield | 18 | medallion | a dome over the squad for 8 s absorbs the next max(10, 30% of the squad) losses |
| Airstrike | 23 | crate | three bombs 12, 20 and 28 units ahead, 0.6 s apart, each 12 musket balls' damage within 3 |
| Power shot | 29 | medallion, crate | twice the damage for 6 s (bigger shots, glowing guns) |
| Freeze | 32 | crate | every enemy within 45 units stands still for 3 s (the boss for 1.5 s), tinted ice blue |

A power-up stands a few units before a horde (its first horde on the level that brings it in, else now and then).

## 5. The horde

Waves wake when the squad is 62 units from their front row; they march down the road toward the squad and, within 14
units, close in on its x. An enemy that touches the squad's footprint is gone and takes its strength in soldiers with
it. Enemies that slip 8 units past are gone.

| Kind | Hit points | Speed | Radius | Strength | Drawn |
|---|---|---|---|---|---|
| Foot | 1 | 3.2 | 0.42 | 1 | red pawn, spiked helmet, spear |
| Rider | 2 | 5.6 | 0.55 | 2 | a red rider on a brown horse |
| Brute | 8 | 2.2 | 0.85 | 6 | a big dark-red pawn with a round shield |
| Boss | the level's | 1.5 | 1.9 | crushes 22 soldiers a second while it touches the squad | the ogre: horns, tusks, a club |
| Archer (from 17) | 1 | stands | 0.42 | 1 | a hooded raider with a bow: from 18 units it looses an arrow every 1.6 s that lands on the squad 0.8 s later and takes a soldier |
| Catapult (from 21) | 40 | stands at the road's side | 1.2 | 0 | a timber engine beyond a horde: from 45 units it hurls a boulder every 3.2 s at where the squad will be; a red circle (1.7) shows 1.2 s before it lands and takes the soldiers in it (at most 14) |
| Giant (from 29) | three seconds of the squad's fire | 1.8 | 1.3 | 25 | a brute twice the size with a club, a health bar over its head; it drops a medallion when it falls |

**The boss's attacks** (from level 30, each world its own: docs/CONTENT_PASS.md 4.3), each shown on the road before it
lands and dodged sideways: the **club** (from 30: every 4.5 s while within 14 units, the half of the road the squad
is in turns red for 1 s, then up to 12% of the squad in it is lost), **boulders** (from 50: every 3 s while further
than 14, a catapult's circle of 2, up to 10%), the **war cry** (from 60: at two thirds and one third of its strength
16 foot, 24 on a hard level, run in from behind it), the **stomp** (from 70: every 5 s a ridge rolls down the road
with a gap 3 wide; up to 10% of those outside it are lost). Worlds 9 and 10 pick two of them.

Waves spawn as a loose crowd (ranks with every soldier a little off its place). Archers stand in ranks behind a horde.

**On the road** (docs/CONTENT_PASS.md 3.2): **mines** (from 13) lie in front of hordes: stepped on they cost 6
soldiers; shot (three musket balls) they burst and do ten musket balls' damage to every enemy within 2.5, so a mine
under a horde is a gift. **Moving gates** (from 19) slide 3 units either way every 3 s on a rail. **Cages** (from 24)
hold prisoners: bullets pass through; the squad touching one frees them (`+N`).

**Modes** (docs/CONTENT_PASS.md 3.3; in every block of ten until the rotation of step 8): the 10th is the boss's; the
5th a **gold rush** from 15 (no enemies, a road of gates, moving gates, barrels and medallions; a barricade never takes
the last 3 soldiers, so it cannot be lost); the 2nd an **arsenal** from 22 (the squad starts with a weapon); the 4th a
**rescue** from 24 (the squad starts with 3, every horde guards a cage). A hard level is always a march or the boss's.
The mode's icon sits in the level chip.

## 6. Levels: the generator and the bake

`src/gen/gen.cpp` lays sections up the road from z = 14 to 24 units before its end. After each section it plays the
road built so far with the bot and measures the squad E the bot really has there; the next section is sized against E:

| Section | From level | What |
|---|---|---|
| +1 column | 1 | 8 to 16+ panels of `+1` (growing to `+3`) down one side, 1.3 units apart |
| Gate pair | 2 | two gates side by side: `+a`/`+b`, or `+a`/`×2` (from 5), or `+a`/`-b` (from 6) |
| Barrels | 4 | two or three barrels; hit points about one to one and a half seconds of the squad's fire |
| Barricade | 6 | two red gates across the whole road, `-a` and `-b`, that grow |
| Triple | 20 | `×3`, `+add` and a cost, in a random order |
| Horde | 1 | after every two or three other sections: foot across the road or one side; riders (from 7) and brutes (from 12) behind them; past 900 foot the rest come as brutes |

The next section is drawn by weight from those the level knows, never the same twice running. Each horde is made as big
as it may be (halving its size up to eight times against the bot) while its own enemies take no more than a share of
the squad, its "bite": 5% at level 1 rising to 35% at level 200 and after (times 0.6 to 1.4 at random, times 1.4 on a
hard level). Only the soldiers its enemies take count (`Battle::waveLost`), not what the bot gains or loses by
steering around it. Big squads mow down hordes of a thousand without loss; small ones bleed. A section the bot cannot
get through is dropped.

**The end fight** sets the level's difficulty. The boss (on every tenth level, on level 8, on every hard level, on 45% of
the others after 8 and 60% after 100) or, without one, a last stand of the horde across the road. The bake finds its
strength m by halving (in log steps, 0.05 to 90): the boss has as many hit points as the squad at the end of the road
deals in m seconds, and crushes a ninth of the squad it meets every second it stands on it (at least 22), so the
fight is over in about nine seconds either way, whatever the squad; the last stand is m times that squad in foot, with
riders and brutes.

**The difficulty curve** (the owner, 2026-11-26: "make sure that it scales up in difficulty, with extra hard levels
throwing in here and there with the later levels just barely theoretically beatable with 100% perfect runs"). The bake
judges each level by its best run: the planner (`src/sim/bot.cpp`, `planLevel`), which plays the level out every way it
knows (how early to line up with a row of gates to shoot it up: 15, 26 or 42 units; which gate of each row of choices
to take, one row after another) and keeps the run that ends with the most soldiers. The share of its peak squad that
best run keeps must be near the level's target `0.02 + 0.6·e^(−n/65)` (between 0.6 and 1.5 times it; levels 1-10 keep
45-100%): about 60% at the start, 30% at level 50, 15% at 100, 5% at 200 and under 3% at 300, where a perfect run
ends with a handful of soldiers. Boss levels ask for 0.8 times their neighbours'. One level in about nine after level 14
(`isSpike`, never two running) is a hard level: a third of the target (at least 1.2%), a boss, bigger bites; `--sim
table` marks them "(hard)".

**Speed** rises with the level too (the owner: "Are you scaling the speed of the levels too?"): the squad marches at
4.6 units a second on level 1 and 7.0 on level 300 (linear), the horde's pace (its march and how fast it closes in,
`Y` in the text form) from 1.0 to 1.35 times its kind's speed; a hard level marches 8% and charges 10% faster. Roads
grow longer after level 150 (0.25 units a level) so a level stays near half a minute.

The level keeps its best run's plan (`P` how early to line up, `Q` each row's gate) and end squad (`R`): the tests replay
the plan and must end with exactly that squad, the proof that every level can be won. Three stars at 90% of it, two at
45%. Levels are stored as text (`S L V B W X T E A C R K Y D P Q` then `g`, `b` and `w` items) and played through that text
form during the bake, so the table is exactly what was tested. `papermarch_headless --sweep N` prints how level N's end
fight moves its margin.

Hint lines (once each, on the level that introduces the idea): 1 "DRAG TO STEER YOUR SQUAD", 2 "MARCH THROUGH THE BLUE
GATES", 3 "SHOOT A GATE TO GROW IT", 4 "SHOOT BARRELS TO FREE RECRUITS", 5 "×2 DOUBLES YOUR SQUAD", 6 "SHOOT RED
BARRICADES UP", 7 "RIDERS CHARGE FAST", 8 "BRING DOWN THE BOSS", 12 "BRUTES TAKE MANY HITS", 16 "DON'T RUN INTO
BARRELS".

The bot: at the next row of gates within 15 units (or the plan's distance) it goes for the plan's gate, else the gate
that leaves the biggest squad (or a gap if every gate hurts); otherwise to a barrel within 26 units when no enemy is
within 12 (stepping aside if about to run into one); otherwise toward the horde, the nearest weighing most and the boss
twenty times. In the app it plays each level's plan (pictures, benchmarks, `--bot best`).

## 7. Stars and progress

One star for a win, two at the table's two-star squad, three at its three-star squad. The best stars per level are
saved, as is the next level to play. Every level is open in LEVELS. After level 300: "ALL 300 LEVELS CONQUERED!".

## 8. Screens

- **Play**: top left the `LEVEL N` chip (opens LEVELS) and the level's best stars; top right pause; under them the road
  progress bar (the squad's dot, a gold star at the finish or a red disk for the boss), which becomes the boss's
  strength bar when the ogre is in sight. A hint line under it on introducing levels. Before the march: "DRAG TO MARCH"
  with a hand swinging under the squad.
- **Victory**: VICTORY! (PERFECT! with three stars), three stars landing one by one, "SOLDIERS LEFT N", NEXT (FINISH on
  level 300), RETRY under three stars. Confetti.
- **Defeated**: "YOU MARCHED N% OF THE ROAD", RETRY, LEVELS.
- **Pause**: RESUME, RESTART, LEVELS, SETTINGS (and the UPDATE pill on GitHub builds).
- **Settings**: SOUND, MUSIC, HAPTICS, REDUCE EFFECTS; hold to reset progress; version and credits.
- **Levels**: GLOWTIDE's selector: five a row, as many rows as fit, stars under each, -100/PREV/NEXT/+100, BACK.
- **Title**: on a cold start a paper card "PAPERMARCH — GROW YOUR SQUAD. CRUSH THE HORDE." over the first level; the
  first touch fades it.

Between levels the page curls away (GLOWTIDE's paper turn; a fade with REDUCE EFFECTS).

## 9. Look

The look is `docs/ART_DIRECTION.md` (the owner, 2026-11-26: "Visuals and graphics need lots and lots of work, it needs
to be visually impressive"; art direction and assets by a Fable art director): painted toy soldiers in a sunlit
diorama, rendered in Blender and shipped as sprites (`assets/art`, made by `tools/art`). A camera 15 units above the
road, pitched 30° down, the squad 8.5 units ahead of its foot and 80% down the screen; the road at the squad spans 92%
of the screen's width; the horizon, a sky and a backdrop show near the top. Without `assets/art` the paper look below
is drawn: the ground per pixel (`shaders/ground.frag`), paving stones, curbs, a verge, trees of the world's family and
banners as paper stickers with soft foot shadows.

Juice: enemies flash white when hit and fall over when killed (with paper bits); gates flash and grow; a passed gate
shatters into its colour and its number rises over the squad; a barrel bursts into gold shards, smoke, sparks and a
ring, with its reward floating up; soldiers lost fall over in blue; the boss's fall is a big burst with a shake. REDUCE
EFFECTS keeps a third of the bits and no shake.

## 10. Sound

Synthesized at start-up (`src/audio/synth.cpp`). The battle: a toy musket's shot (a volley sound at most 13 a second,
louder with more guns), a thwack per hit, a paper flip per kill (a clash for brutes), the count ladder climbing while a
gate is shot up, a pop, coins and a ladder run for a good gate, two knocks and a falling ladder for a bad one, a wood
knock per barrel hit and an explosion, coins and pops when it bursts, a horn and a drum when a horde wakes, drum, horn
and timpani when the boss appears, explosions and a crash when it falls, the fanfare and choir for a win, the falling
horn for a defeat. The march music has five layers that join as the battle heats up: 1 before the march, 2 marching, 3
past half the road, 4 while enemies are alive, 5 with the boss or after a win. Each world has its own mode, tempo and
lead instrument.
