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
| Fire | 5 + 2.4·min(squad, 50) shots a second, from random points across the squad's front; each bullet flies 42 units a second for 30 units |
| Damage | a bullet does max(1, squad / 50) |

Drawn: up to 150 blue paper soldiers (helmet, gun, gold belt) in a sunflower pattern inside the footprint, bobbing as
they march, muzzle flashes; a blue badge over them with the full number.

## 3. Gates

A gate is a panel across part of the road at some z, with a value and a kind:

- **add**, value > 0: blue, `+N`. Passing it adds N.
- **add**, value ≤ 0: a red barricade, `-N`. Passing it takes N (it can take the whole squad).
- **multiply**: violet, `×N`. Passing it multiplies.

The squad passes the gate its centre is in when its centre crosses the gate's z; the other gates of that row are left
behind (drawn faded). Gates that **grow** (most add gates and barricades) take one per bullet that hits them, up to
their cap: shooting a `+3` gate makes it a `+5`, shooting a `-20` barricade can make it a `+3`. Multipliers never grow.
Bullets stop at the first growing gate, barrel or enemy in their path.

## 4. Barrels

A gold barrel stands on the road with hit points and a reward (`+N`, shown over it with a bar of its strength left).
Each bullet does the squad's damage; at zero it bursts and the reward joins the squad. A barrel the squad runs into
bursts too and costs max(1, hp / 2) soldiers.

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

Waves spawn as a loose crowd (ranks with every soldier a little off its place).

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
as it must be (halving its size up to seven times against the bot) to cost the bot a share of its squad, its "bite":
5% at level 1 rising to 35% at level 200 and after (times 0.6 to 1.4 at random, times 1.4 on a hard level). A section
the bot cannot get through is dropped.

**The end fight** sets the level's difficulty. The boss (on every tenth level, on level 8, on every hard level, on 45% of
the others after 8 and 60% after 100) or, without one, a last stand of the horde across the road. The bake finds its
strength m by halving (in log steps, 0.05 to 90): the boss has as many hit points as the squad at the end of the road
deals in m seconds; the last stand is m times that squad in foot, with riders and brutes.

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
45%. Levels are stored as text (`S L V B W X T E A C R K Y P Q` then `g`, `b` and `w` items) and played through that text
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
