# PAPERMARCH — Game design

The numbers here are the code's (`src/sim/battle.cpp`, `src/gen/gen.cpp`, `src/game/*`); when they change, change this.

## 1. The road

A level is a straight road along z (units; the squad starts at z = 0), x in [-5, 5]. The game is 100 levels (the owner,
2026-11-27: "the last level can be 100", "like 15 mins total"), each longer than the one before: a level is meant to
last 30·30^((n−1)/99) seconds (`levelSeconds`: half a minute at level 1, about 2:40 at 50, 15 minutes at 100). The squad
marches up it at the level's speed (4.6 units a second at level 1 to 7.0 at 100); the road is that time at that speed,
times 0.9 for the time spent fighting (0.5 on a rush), 24 more on a boss level. A level without a boss is won when the squad's centre crosses the finish line (a band of checks
painted across the road at its length) or once every wave has come and every enemy is beaten (the owner, 2026-11-27:
"level should be finished when finish line is crossed or all enemies are gone"; a gold rush, with no enemies, runs to
its line). On a boss level the ogre stands at the end and the level is won when it falls.

Nothing moves until the player touches the road ("DRAG TO MARCH"). Then:

## 2. The squad

| Property | Value |
|---|---|
| Size | a number, from the level's start (3 on levels 1-5, 3-6 after) to 99,999 |
| Footprint | a disk of radius min(4.4, 0.55 + 0.30·√squad) |
| Steering | the squad follows the finger: x moves toward the target at 12·(distance) units a second, at most 18; it stays on the road. A drag across the road's width on screen moves it 1.25 road widths (`steer`) |
| Fire | the musket: 5 + 2.4·min(squad, 50) shots a second, from random points across the squad's front, each at the nearest enemy that still needs one; a ball flies 42 units a second for 48 units |
| Damage | a musket ball does max(1, squad / 50) to the boss, giants, gates and barrels (counted in tenths: every soldier past 50 adds a little); to the horde it is one musket ball whatever the squad's size (`Battle::against`) |
| Weapon | one at a time, from a weapon gate or crate (or the level's start), until the next or the level's end; it multiplies the musket's fire (below) |

| Weapon | Shots | Damage a ball | Range | Its trick |
|---|---|---|---|---|
| Musket | 1× | 1× | 48 | - |
| Twin rifles (from level 9) | 1× pairs | 0.6× each | 48 | two streams 0.5 apart: 1.2× in all, the best gate grower |
| Scattergun (14) | 0.8× | 5 pellets of 0.45× in a 28° fan | 30 | murderous up close, spread thin far; only the middle pellet grows a gate |
| Long lance (22) | 0.5× | 1.2× | 60 | the bolt passes through six enemies (stops at the boss, gates, barrels) |
| Rockets (28) | 0.25× | 3× | 45 | slow; bursts on impact: 1.5× to everything within 1.8 |
| Frost blaster (32) | 1× | 0.5× | 36 | what it hits goes at half speed for 2 s (the boss three quarters) |
| Mortar (36) | 0.2× | 2.5× | 28 | a shell lobbed over everything to land 28 ahead 1.1 s later: 2.5× to everything within 2.6 |
| Flamethrower (41) | 10 ticks a second | 0.6× a tick | 13 | no projectiles: everything in a cone 13 long, widening 18° each side, burns; mines in it go off |
| Cannon (48) | 0.12× (at least one a second) | 8× | 50 | a ball 1.2 wide rolls through everything on its line: brutes and giants knocked back, barrels and cages opened, mines set off, gates grown |
| Storm coil (53) | 0.6× | 0.8× | 34 | lightning: the nearest in a lane 3 wide, then four jumps of 3 to the next, 0.7× each |

A shield bearer's shield takes the musket's, twin's, scattergun's, rockets' and frost blaster's balls from the front; the
lance, flame, coil, mortar, cannon and every blast get past it.

Drawn: up to 500 painted toy soldiers (250 with REDUCE EFFECTS) in a sunflower pattern inside the footprint, bobbing
as they march, each with its weapon's gun and muzzle flash; past about 150 the crowd grows back toward the camera (up to
2.4 times as deep behind its front) and packs tighter (the owner, 2026-11-27: lots of soldiers must look like lots). Each
shot of a big squad is drawn as a volley of up to four tracers (one per 50 soldiers) fired from across its front and
converging where it goes (a ball carries squad / 50 musket balls); more muzzles flash. The squad's banner over them
carries the full number; the weapon's icon is in the HUD.

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

A **flip gate** (from 39) covers half the road and turns over every 1.5 s between a sum and a cost (the face it turns to
next shows small over it); it never grows: arrive while it is blue, or pass beside it. **Mystery gates** (from 56) come
in pairs showing "?" and the same two outcomes (`+40 OR -15`); one holds each. Five hits show a gate's true face, and
then it grows like any other.

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
| Quick march | 38 | medallion | the march 1.6 times as fast and the steering half again as quick for 5 s (lines streaming past) |
| Magnet | 44 | medallion | barrels, crates and cages within 25 open by themselves for 5 s (an arc from the squad) |
| Reinforcements | 46 | crate | max(8, 15% of the peak) soldiers parachute in over 2 s |
| Giants | 49 | medallion | for 5 s the soldiers stand half again as tall: what touches the squad is trampled and takes no one, barrels are smashed for free, the boss crushes half as fast |
| Hourglass | 54 | medallion, crate | the enemy, its arrows and the boss's blows at 40% for 5 s |
| Double gates | 58 | medallion | the next add gate counts twice (12 s at most; a gold rim at the feet of the gates it may be) |
| Field gun | 64 | crate | a cannon at the squad's side fires every second for 8 s (8 musket balls' damage, through everything on its line) |

A power-up stands a few units before a horde (its first horde on the level that brings it in, else now and then).

## 5. The horde

Waves wake when the squad is 62 units from their front row; they march down the road at their own speed (times the
level's pace) and, within 14 units, close in on its x. **The squad marches through the hordes** and stops only when
one is almost on it: an enemy that walks within 2.5 of its front (the owner, 2026-11-27: "it should only stop when the
enemies are very close"), and before the boss, where a giant stands on it, and at a fort wall (in 0.8.0-0.11.0 it stood
for every horde from 25 away; in 0.12 only for the big ones). An enemy that
touches the squad's footprint is gone and takes its strength in soldiers with it, times squad / 50 past 50 soldiers
(`Battle::toll`: a horde takes the same share of any squad, as the squad's balls grow past 50); arrows and mines too.
Enemies that slip 8 units past are gone. Soldiers lost are knocked flying from the squad's front, a few more flash red,
and the count lost rises over it every half second; a loss of a twentieth or more flashes the screen's edge.

The generator gives every horde enemy three times its kind's hit points (below): with the squad's fire (at most 125
musket balls a second, 48 units ahead) a horde that falls to one ball each never reaches it; at three its size sets what
it costs (a squad of 96 takes nothing from 400 foot and nearly everything from 800), and hordes run to hundreds.

**The boss** stops on the squad and clubs it: a blow every 0.9 s, each taking what a ninth of the squad a second adds up
to (at least 22 a second), the club coming down in time and the soldiers it hits sent flying. **A giant** that reaches the
squad stops and clubs it every 1.2 s (4 for every 50 soldiers) until it falls; the squad halts while one stands on it.

| Kind | Hit points | Speed | Radius | Strength | Drawn |
|---|---|---|---|---|---|
| Foot | 1 | 3.2 | 0.42 | 1 | red pawn, spiked helmet, spear |
| Rider | 2 | 5.6 | 0.55 | 2 | a red rider on a brown horse |
| Brute | 8 | 2.2 | 0.85 | 6 | a big dark-red pawn with a round shield |
| Boss | the level's | 1.5 | 1.9 | clubs a ninth of the squad a second (at least 22) while it touches it, a blow every 0.9 s | the ogre: horns, tusks, a club |
| Archer (from 17) | 1 | stands | 0.42 | 1 | a hooded raider with a bow: from 18 units it looses an arrow every 1.6 s that lands on the squad 0.8 s later and takes a soldier |
| Catapult (from 21) | 40 | stands at the road's side | 1.2 | 0 | a timber engine beyond a horde: from 45 units it hurls a boulder every 3.2 s at where the squad will be; a red circle (1.7) shows 1.2 s before it lands and takes the soldiers in it (at most 14) |
| Giant (from 29) | three seconds of the squad's fire | 1.8 | 1.3 | 25 | a brute twice the size with a club, a health bar over its head; it drops a medallion when it falls |
| Shield bearer (from 26) | 3, and a shield of 14 times that from the front | 2.4 | 0.5 | 2 | a red footman behind a tall shield, in a horde's front rank |
| Bomber (from 37) | 2 | 6.5 | 0.45 | 8 | a runner with a keg: killed, it bursts for 10 musket balls within 2.2 (other bombers chain) |
| Drummer (from 42) | 4 | 3.2 | 0.5 | 2 | whoever is within 5 of it goes 40% faster; rings of its beat |

The fire goes for a bomber or a drummer up to 12 units beyond the nearest enemy first.

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
hold prisoners: bullets pass through; the squad touching one frees them (`+N`). **Rollers** (from 27) wait at the top of a
lane (x ± 1.2): once the squad is within 30 the boulder rolls down it, the lane marked in red, and takes whoever of the
squad stands in it (at most 40%); it cannot be shot. **Fort walls** (sieges, from 37) cross the whole road with hit points
(four to seven seconds of the squad's fire; rockets, the cannon and the mortar do three times, the flame burns it): the
march stops at a wall and the horde waits behind it while archers shoot from the ramparts; when it falls ("WALL DOWN!")
the horde charges. **Weather strips** (22 units with a pair of gates in them): on **ice** (from 31) the squad steers a
third as quickly; **wind** (from 62) pushes it sideways at 2.5-3.5 units a second (arrows on the road, streaks blowing
across); the bot leans into it. **Fog** (from 67, now and then) veils what lies further than about 18 units and
**night** (from 81) darkens all but the squad's own few units (the view only).

**Road hazards** (docs/CONTENT_PASS.md H2, H4, H5, H10, H11; strips in the level data, drawn without new art), each
brought in on its level with a hint and laid by a section of their own (two to three times as many on a gauntlet):
**hammers** (34, "MIND THE HAMMER"): a wrecking ball on a chain swings across the road from a gantry
(`x = 3.8 sin(2 pi t / 2.4)`, its path marked in red); passing under it costs whoever stands under the ball (a share
of the squad, at most a quarter; GIANTS walk through). **Pits** (43, "STAY ON THE ROAD"): 3-5 of the road gone on one
side for 5-9 units (water, clouds, lava or a gorge below, warning posts); whoever is over it falls, the squad's share
over it by the time it has crossed (at most a fifth), and enemies walking over it fall too. **The narrows** (47, "THE
BRIDGE IS NARROW"): a bridge 4.4 wide for 10-16 units with railings; the squad squeezes onto it (stretched along the
road) and fires at 0.6 times its rate; the horde funnels in. **The tide** (51, "MIND THE TIDE"): one half of the road
floods 3 s in every 6 after a second's red shimmer (lava on Cinderpeak), taking a fifth a second of whoever stands in
it and sweeping away the enemies in it. **Mud** (69, "MUD SLOWS THE MARCH"): 16-22 units where the march is half as
fast, a pair of gates in it. The bot keeps to the tide's dry half and the pit's far side, and stands where the hammer
will not be when it passes under it.

**Modes** (docs/CONTENT_PASS.md 3.3), by their place in each block of ten, each from the level that brings it in (a
march before): 1 and 6 a march; 2 an **arsenal** from 22 (the squad starts with a weapon); 3 a **rescue** from 34 (the
squad starts with 3, every horde guards a cage; on slot 4 from 24 to 33); 4 a **gauntlet** from 34 (rollers and
minefields, gates as bait, no horde but the end's); 5 a **gold rush** from 15 (no enemies, a road of gates, moving gates,
barrels and medallions; a barricade never takes the last 3 soldiers, so it cannot be lost); 7 a **siege** from 37 (every
horde behind a fort wall with archers), or on 57, 77 and 97 a **last stand** ("HOLD THE LINE": a road of 0.8 the time,
then the squad holds at its end with two supply crates before it while five growing waves, riders from the third and
brutes in the last, all come at it, about half a minute of them; no finish line, won when they are beaten); 8 a **rush** from 38 (half again as fast, a short road of gates and hazards, no
horde but the end's); 9 a **giants' road** from 49 (three giants with every horde); 10 the boss's. A hard level is always
a march or the boss's. The mode's icon sits in the level chip, and its hint shows on each of its levels.

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
| Horde | 1 | after every two or three other sections: up to 900 foot across the road or one side; riders (from 7), brutes (from 12), shield bearers (26), bombers (37) and drummers (42) with them, archers behind |

The next section is drawn by weight from those the level knows, never the same twice running. Each horde is made as big
as it may be (halving its size up to eight times against the bot) while its own enemies take no more than a share of
the squad, its "bite": 5% at level 1 rising to 35% at level 67 and after (times 0.6 to 1.4 at random, times 1.4 on a
hard level), measured against a weaker player: the bot plays to just before the horde wakes, then keeps half its
soldiers on level 4 rising to 85% on level 100 (`hordeCost`), so a player who missed some gates still gets through;
levels 1-3 hold at most 20 foot a horde (three soldiers win them). Only the soldiers its enemies take count
(`Battle::waveLost`), not what the bot gains or loses by steering around it. When no size costs near the bite (none cost anything, the next wipes the squad out), the horde is
15% smaller than the last that cost nothing. A section the bot cannot get through is dropped. Everything is laid on the
text form's grid (tenths) before the bot plays it, so what it measures is what the table holds.

**Each world's favourites** (docs/CONTENT_PASS.md 4.3) come twice as often once the level knows them: 1 Greenmeadow
moving gates, mines, cages; 2 Sunbaked archers, catapults, wind, fort walls (before some hordes on any road); 3 Frostvale
ice, shield bearers, fog; 4 Emberfall bombers, rollers, drummers; 5 Tidereach rollers, flip and moving gates, the tide,
the narrows; 6 Bloomgrove mystery and flip gates, cages; 7 Mirefen fog, bombers, giants, pits, mud; 8 Skyhold wind,
archers, rollers, the narrows, pits, hammers; 9 Cinderpeak night, mines, giants, walls, the tide (lava); 10 Lanternfair
flip, mystery and moving gates, rollers, hammers (1 Greenmeadow also favours pits). A world's favourite hazard is three
times as likely as the others. A hard level on a siege's or a
giants' road's place keeps that mode. A long road offers another weapon about every two minutes (a weapon gate beside a
second weapon or a modest sum) and holds up to 2 + 1 per 45 s of the level's power-ups.

**The ceiling** (a 15-minute road would otherwise snowball): past 100 + 25·n soldiers the road offers no ×2 or ×3, its
sums, barrels and cages give `E / ceiling` times less, and its hordes bite up to twice as hard (never more than 70%).

**The end fight** sets the level's difficulty. The boss (on every tenth level, on level 8, on every hard level, on 45% of
the others after 8 and 60% after 100) or, without one, a last stand of the horde across the road. The bake finds its
strength m by halving (in log steps, 0.05 to 90): the boss has as many hit points as the squad at the end of the road
deals in m seconds, and crushes a ninth of the squad it meets every second it stands on it (at least 22), so the
fight is over in about nine seconds either way, whatever the squad; the last stand is m times that squad in foot (up to
900, tougher past that), with riders and brutes, standing 50 units beyond the road's last section (it comes once the squad has passed everything
else, and beating it wins the level). The bake plays the road to 64 units before the end fight once and
replays only the end for each strength tried; the one it keeps is played again from the start to be sure.

**The difficulty curve** (the owner, 2026-11-26: "make sure that it scales up in difficulty, with extra hard levels
throwing in here and there with the later levels just barely theoretically beatable with 100% perfect runs"). The bake
judges each level by the bot's own run of it, the one every horde was sized against (a planned run that takes other
gates or another weapon meets hordes sized for a different squad, and on a 15-minute road one of them ends it; the
planner, `planLevel`, is kept for `--sweep`). The share of its peak squad that run keeps must be near the level's target
`0.02 + 0.6·e^(−n/22)` (between 0.6 and 1.5 times it; levels 1-10 keep 45-90%): about 60% at the start, 18% at level
20, 7% at 50 and 2% at 100, where the run ends with a handful of soldiers out of thousands. Boss levels ask for 0.8
times their neighbours'. **Hard levels** (the owner: "every 5-10 levels is a hard one (allows 0%-1% margin of error)",
"even in the low levels"): from level 6, the next 5 to 10 levels on (`isSpike`; never a boss's tenth or a gold rush's
fifth): the run keeps about 1% of its peak (0.2-1.5%), a boss, bigger bites, 8% faster; `--sim table` marks them.

**Themes** (the owner: "Every level should feel somewhat different than the last", "sprinkle in special levels every now
and then like fast ones"): every level leans on one theme, never the same as the level before (`themeOf`): columns of +1
(longer), rows of choices, barrels, hordes (one after every one or two sections, smaller), barricades, or a special one
about every third level, never the last special again, with its banner when no introduction takes the hint line:
**SPRINT** (1.45 times as fast, a short road, few hordes), **MULTIPLIER MADNESS** (rows of x2 and x3, past the ceiling
too), **SWARM** (rare hordes 1.7 times as big, a slower march), **CAVALRY CHARGE** (hordes of riders, a faster march),
**BRUTE SQUAD** (half the foot, three times the brutes, slower). Each level's pace also varies by up to 7% either way.
The road stops being laid once the bot's run of it reaches the level's time (0.6 of it for a rush or a sprint).

**Speed** rises with the level too (the owner: "Are you scaling the speed of the levels too?"): the squad marches at
4.6 units a second on level 1 and 7.0 on level 100 (linear), the horde's pace (its own speed, `Y` in the text form)
from 1.0 to 1.35 times its kind's; a hard level marches 8% and charges 10% faster.

The level keeps its run's end squad (`R`; the text form can still hold a plan, `P` and `Q`, but the bake writes none):
the tests replay the level with the bot and must end with exactly that squad, the proof that every level can be won. Three stars at 90% of it, two at
45%. Levels are stored as text (`S L V B W X T E A C R K Y D P Q` then `g`, `b` and `w` items) and played through that text
form during the bake, so the table is exactly what was tested. `papermarch_headless --sweep N` prints how level N's end
fight moves its margin.

Hint lines (once each, on the level that introduces the idea): 1 "DRAG TO STEER YOUR SQUAD", 2 "MARCH THROUGH THE BLUE
GATES", 3 "SHOOT A GATE TO GROW IT", 4 "SHOOT BARRELS TO FREE RECRUITS", 5 "×2 DOUBLES YOUR SQUAD", 6 "SHOOT RED
BARRICADES UP", 7 "RIDERS CHARGE FAST", 8 "BRING DOWN THE BOSS", 12 "BRUTES TAKE MANY HITS", 16 "DON'T RUN INTO
BARRELS"; then each new idea's on the level that brings it (`src/sim/levels.cpp`).

The bot: at the next row of gates within 15 units (or the plan's distance) it goes for the plan's gate, else the gate
that leaves the biggest squad (or a gap if every gate hurts); otherwise to a barrel within 26 units when no enemy is
within 12 (stepping aside if about to run into one); otherwise toward the horde, the nearest weighing most and the boss
twenty times. In the app it plays each level's plan (pictures, benchmarks, `--bot best`).

## 7. Stars and progress

One star for a win, two at the table's two-star squad, three at its three-star squad. The best stars per level are
saved, as is the next level to play. Every level is open in LEVELS. After level 100: "ALL 100 LEVELS CONQUERED!".

**Endless** (ENDLESS under the LEVELS grid, its best stage beside it): one run, stage after stage, until the squad
falls. The squad, its weapon and its running power-ups carry from stage to stage (`Battle::carry`). A stage is laid as it
is played (`src/sim/endless.cpp`), a section at a time about 110 units ahead of the squad, sized against the squad it
has then: the campaign's generator needs the bot and runs only in the tools, so endless is laid from formulas (a fight
against a horde costs the same share of any squad past 50). Stage k: world `(k-1) % 10 + 1`, march 4.8 + 0.2(k-1) (at
most 7.5), horde pace 1 + 0.04(k-1) (at most 1.45), 55 + 5(k-1) seconds of road (at most 110), hordes of 110 + 30(k-1)
foot scaled to the squad, every enemy 4% tougher a stage, riders from 2, brutes and archers from 3, shieldmen 4, bombers
5, drummers 6, fog or night now and then from 5, the road hazards from 3. Every third stage ends with the boss, sized to
3-7 seconds of the squad's fire; the others with a last stand. The bot falls between stages 3 and 14, about 7 on
average (`papermarch_headless --endless`, 14 seeds). The run ends on a card: RUN OVER (NEW BEST!), the stage and the paces marched, the best, AGAIN, LEVELS.

## 8. Screens

- **Play**: top left the `LEVEL N` chip (opens LEVELS) and the level's best stars; top right pause; under them the road
  progress bar (the squad's dot, a gold star at the finish or a red disk for the boss), which becomes the boss's
  strength bar when the ogre is in sight. A hint line under it on introducing levels. Before the march: "DRAG TO MARCH"
  with a hand swinging under the squad.
- **Victory**: VICTORY! (PERFECT! with three stars), three stars landing one by one, "SOLDIERS LEFT N", NEXT (FINISH on
  level 100), RETRY under three stars. Confetti.
- **Defeated**: "YOU MARCHED N% OF THE ROAD", RETRY, LEVELS. In endless the chip reads `ENDLESS N` and the run's end is
  its own card (section 7).
- **Pause**: RESUME, RESTART, LEVELS, SETTINGS (and the UPDATE pill on GitHub builds).
- **Settings**: SOUND, MUSIC, HAPTICS, REDUCE EFFECTS; hold to reset progress; version and credits.
- **Levels**: GLOWTIDE's selector: five a row, as many rows as fit, stars under each, -10/PREV/NEXT/+10 (a world), BACK.
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
