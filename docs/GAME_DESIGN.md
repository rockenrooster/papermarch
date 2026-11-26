# PAPERMARCH — Game Design

Read `PLAN.md` first. Every number here is a starting value (PLAN 0.6); the rules in section 3 are not.

| Section | Holds |
|---|---|
| 1 | The feel target |
| 2 | One level, move by move (the concept frame's level) |
| 3 | Rules |
| 4 | Controls |
| 5 | Elements |
| 6 | Kinds of level |
| 7 | Stars, undo, hints, the dead-end nudge |
| 8 | Levels: hand-made, calendar, generator, acceptance, bosses |
| 9 | Look |
| 10 | Juice (timings) |
| 11 | Screens and flow |
| 12 | Sound |
| 13 | Haptics |
| 14 | Accessibility and safety |
| 15 | Names |

---

## 1. The feel target

The owner's reference is how advertisements for mobile strategy games play, as opposed to the games behind them: a
small army with a number over its head, a map of enemies with numbers over theirs, a finger that picks the weak one
first, a number that jumps, and thirty seconds later a horde rolling over a castle. What makes those clips watchable:

- **The arithmetic is instant.** Bigger beats smaller. Nobody reads a rule.
- **Every tap pays.** The number goes up, the crowd gets longer, something flips to your colour.
- **The mistake is visible in advance.** You can see the 45 you cannot take yet and the ×2 you should save.
- **It ends in a flood.** The last fight is not close; you arrive with ten times what you started with.

PAPERMARCH is that clip as a real game: the same four properties, plus an honest puzzle underneath (the order matters,
and there is a provable best order).

Not the target: a strategy game. No resources, no building, no unit stats, no waiting.

---

## 2. One level, move by move

`concept/concept_gameplay.png` shows hand-made level 12 after two moves. The host starts at Home (bottom) with **10**.

| Site | Kind | Number | Roads to |
|---|---|---|---|
| Home | start | host 10 | A, B |
| A | camp | 6 | Home, C |
| B | village | +8 | Home, D |
| C | camp | 14 | A, E |
| D | shrine | ×2 | B, E |
| E | post (the bridge) | −5 | C, D, F, G |
| F | camp | 30 | E, I |
| G | camp | 45 | E, I |
| I | growing camp | 12, +3 a turn, at most 45 | F, G, K |
| K | the Keep | 120 | I |

Moves 1 and 2 (already made in the frame): A, 10 + 6 = 16; B, 16 + 8 = **24**. Turn 2. The frame shows C in gold (24 ≥
14: takeable), the shrine in violet (free to take), F, G, I and the Keep in ink with a lock, and I's `+3` badge (it
shows 18 now: 12 + 3 × 2).

- **The obvious line** takes the shrine now: 48, C 62, the bridge 57, F 87, G 132, I (33 by turn 7) 165, the Keep:
  **285**. A win, two stars.
- **The best line** saves the shrine and lets I ripen: C 38, the bridge 33, F 63, G 108, I (30 at turn 6) 138, the
  shrine 276, the Keep: **396**. Three stars.

Nothing on this map can strand the host, so nobody loses level 12; what separates two stars from three is two ideas
(multiply last; a growing camp is worth more later, while you can still beat it). That is the game's difficulty model:
winning asks for attention, three stars ask for the best order. Later levels add maps where the obvious line strands.

---

## 3. Rules

Everything is an integer. A level is a set of **sites** joined by **roads**, a starting host strength `S`, and
sometimes a rival or raids. `turn` starts at 0.

### 3.1 A site's number

A site's current number is `v = clamp(value + perTurn × turn, lo, hi)`. Most sites have `perTurn = 0`. The Keep's
number is further reduced by each depot taken (never below 1). A tag always shows the current number.

### 3.2 What can be taken

A site is **reachable** when it is free (not yours, not burnt, not the rival's) and a road that is open joins it to a
site that is yours. Home is yours from the start. A road with a lock is open once its key tower is yours.

A reachable site is **takeable** when its condition holds (section 5.1). The host needs no path of its own: it
marches through your own land for free.

### 3.3 A move

Take one takeable site: apply its effect to `S`; the site becomes yours; every other free site in its fork group is
burnt; `turn` += 1. Then, in this order: the rival steps (6.4); raids due this turn arrive (6.5).

`S` is capped at 999,999,999.

### 3.4 Winning and losing

- **Win**: the Keep is yours. In Last Stand: the last raid is absorbed.
- **Lose**: a raid arrives stronger than `S`; the rival reaches Home stronger than `S`; or the host is **stranded**:
  nothing is takeable, no rival is marching and no raid is due.
- If nothing is takeable but a rival is marching or a raid is due, turns pass on their own until something changes.
- A site that is not takeable cannot be attacked: tapping it is refused (section 10), never a loss.

### 3.5 The rule every new element must keep

**With everything else equal, a larger `S` is never worse.** Every condition is "S at least something" and every
effect is non-decreasing in `S`. The solver's speed and exactness rest on it (TECH_SPEC 3.5), and a unit test compares
the solver with a brute-force search to hold the rule. An element that breaks it ("too many soldiers cannot cross")
is not allowed.

---

## 4. Controls

- **Tap** a takeable site: the host marches there and takes it. That is the whole control scheme.
- **Drag** from the host's banner to a site does the same (the advertisements show a drawn arrow); a dotted gold line
  follows the finger and snaps to the nearest reachable site.
- A tap on a site that is not takeable: the tag shakes, the lock rattles, a line under the tag says why for a second
  (`NEED 30`, `ROAD LOCKED`, `BRIDGE BURNT`).
- **UNDO** (bottom left): takes back the last move. Unlimited. Works on the lose panel too.
- **HINT** (bottom right): rings the site the solver's best line takes next (section 7).
- A tap during a move's animation is taken at once (the simulation is instant) and its animation queues; with a move
  queued, the running one plays at 2.5×. At most two moves queue.
- Touch targets: a site's hit circle is 64 dp across at least, centred on the site and covering its tag.

---

## 5. Elements

### 5.1 Sites

| Site | Tag | Condition | Effect | Pop-up |
|---|---|---|---|---|
| **Camp** | its number; gold when takeable, ink with a lock when not | `S ≥ v` | `S += v` (the garrison flips and joins) | Two ink tents, a garrison of pawns |
| **Village** | `+A` on ivory | none | `S += A` | Two little houses |
| **Shrine** | `×M` on violet (M is 2 or 3) | none | `S ×= M` | A violet obelisk with a gold star |
| **Post** | `−T` on ivory, the number orange | `S > T` | `S −= T` (soldiers stay to hold it) | A bridge or a gatehouse with two small towers |
| **Mire** | `½` on moss green | `S ≥ 2` | `S = ceil(S / 2)` | Reeds and a sunk signpost |
| **Depot** | its number, and a small `−X` with a keep icon | `S ≥ v` | `S += v`; the Keep's number drops by `X` | A stockade with crates |
| **Key tower** | its number, and a key icon | `S ≥ v` | `S += v`; its locked roads open | A slim tower with a key banner |
| **Keep** | its number, large, gold on ink | `S ≥ v` | `S += v`; the level is won | The castle |

Properties any site can carry:

- **Growing** (`perTurn > 0`, capped at `hi`): a badge `+3` on the tag; recruits walk in from the page's edge each turn.
  A growing camp gives more the later it is taken, until it outgrows the host.
- **Deserting** (`perTurn < 0`, never below `lo ≥ 1`): a badge `−2`; a pawn slips away each turn.
- **Fading shrine**: `×3` until turn `u`, then `×2` (or `×2` until `u`, then dark: taking it does nothing). A badge
  counts the turns left.
- **Fork**: sites of one fork group are joined by a rope bridge with a torch. Take one and the others burn (their
  pop-ups fold flat, their roads char). The classic "left or right".
- **Locked road**: drawn with a paper gate across it and the key tower's colour on the gate.

### 5.2 When each element is introduced

| Level | New | Level | New |
|---|---|---|---|
| 1 | Camps, the Keep | 22 | Key tower and locked road |
| 2 | Gold tag and lock (smallest first) | 28 | Mire |
| 3 | Village | 34 | Deserting camp |
| 4 | Shrine (the first level that can strand) | 40 | Siege kind: depots, a growing Keep |
| 5 | Post | 50 | Fading shrine |
| 6 | Fork | 60 | Race kind: the rival |
| 12 | Growing camp (the concept frame's level) | 75 | Last Stand kind: raids |
| 16 | Gauntlet kind | 10, 20, … | A boss every tenth level |

An introduction level shows the element alone and large, with one hint line under the HUD (GLOWTIDE's hint
mechanism), and cannot be lost in its first two moves.

### 5.3 Stretch elements (not built unless the owner asks after playing)

- **War drum**: a cache site gives a drum (the HUD shows it). Armed with a tap before a fight, the host fights that one
  site at double strength; the gain is unchanged. It keeps rule 3.5.
- **Fog and watchtowers**: tags show `?` until the site is reachable or a watchtower is yours.
- **Troop types**: three colours of garrison, each strong against one other.

---

## 6. Kinds of level

One rule set, five layouts of it. The kind's name is the objective chip in the HUD.

### 6.1 Conquest — `TAKE THE KEEP`

A free map of 6 to 18 sites on one screen. The default: about half of all levels.

### 6.2 Gauntlet — `RUN THE GAUNTLET`

The gate run. A road climbs a tall page through 5 to 9 rows; each row is a fork of two or three sites (`+40` or `×2`;
a camp of 60 or a post of −10), and the Keep stands at the top. The page is up to three screens tall: the view scrolls
with the host, and the next two rows are always in view. Free rows (take both sites) appear from level 80.

### 6.3 Siege — `BREAK THE SIEGE`

The Keep is far too strong and **grows every turn**. Depots lower it. Every move spent on a small camp is a turn of
the Keep's growth: the puzzle is which fights are worth their turn.

### 6.4 Race — `OUTMARCH THE WARLORD`

A **rival host** starts at its own camp with its own number. Its route is drawn on the map as a dotted ink line with
numbered pips. After each of your moves it takes the next free camp on its route and grows by it, skipping sites you
took first; after the last one it marches on Home. When it arrives: `S ≥ R` and it flips and joins you (`S += R`);
otherwise you lose. You may strike first: the site it stands on is a camp of strength `R` when it is reachable. The
Keep is still the goal. Taking a camp on its route both feeds you and starves it.

### 6.5 Last Stand — `HOLD THE LINE`

No Keep. Two to six **raids** come down the roads from the page's edge, each with a number and a countdown badge
(`in 3`); a raid token moves one step a turn. A raid that arrives with `S ≥ W` flips and joins; a stronger one ends the
level. The last raid is the Warlord, drawn as a boss. Between raids you grow on the map.

### 6.6 Bosses

Every tenth level closes a world: a named Keep (section 15), a larger map (14 to 20 sites), two kinds' rules at once
(a Siege with a rival; a Gauntlet that ends in a Last Stand), the boss music cue and a longer win sequence.

---

## 7. Stars, undo, hints

### 7.1 Stars

The **final host** is `S` at the moment of victory, the Keep's (or the Warlord's) number included. The solver knows the
best possible final host of every level (`best`).

| Stars | For |
|---|---|
| 1 | Winning |
| 2 | Final host ≥ 70% of `best` (`star2Pct`) |
| 3 | Final host ≥ `best` (`star3Pct` = 100): the win panel says `PERFECT` |

The win panel shows `HOST 285 · BEST 396`. The saved value is the most stars ever earned on the level.

### 7.2 Undo

Unlimited and free; it does not touch the stars. This is a puzzle: trying an order and taking it back is the play.

### 7.3 Hints

HINT rings the next site of a line that reaches `best` from the current position (or, from a position that can no
longer reach it, the best line left; in a dead end it pulses UNDO instead). A level on which HINT was used gives at
most two stars; the button says so the first time (`HINT: 2 STARS AT MOST`), and RETRY clears it.

### 7.4 The dead-end nudge

After every move the solver checks the position. If no line wins any more, UNDO pulses gold and a paper slip slides in
under the HUD: `NO WAY THROUGH FROM HERE`. The player is never left walking a dead line without knowing
(`deadNudge`, on by default).

### 7.5 Levers that add tension (none built in advance; the owner picks after P2)

- **Harsh taps**: a too-strong site can be attacked, and the host is routed (the advertisements' "fail").
- **Supplies**: a move limit a level, shown as loaves on the HUD.
- **Pure star**: a fourth mark for three stars with no undo.
- **The clock**: the rival moves every four seconds instead of every move.

---

## 8. Levels

300 levels, ten to a world, ten worlds, cycling three times. Level data is baked into the app complete (TECH_SPEC 4).

### 8.1 Hand-made levels

Each has a one-line hint. Numbers are exact; lay the sites out as in the sketches of P2's subplan.

| Level | Start | Sites and roads | The lesson (hint line) |
|---|---|---|---|
| 1 | 10 | Home – camp 4 – camp 9 – Keep 20, in a line | `TAP THE CAMP` (cannot be lost; final 43) |
| 2 | 10 | Home – A camp 6; Home – B camp 14; A – Keep 28; B – Keep 28 | `GOLD MEANS YOU CAN TAKE IT` (B is locked until A; best 58) |
| 3 | 5 | Home – village +8; Home – camp 10; camp – Keep 22 | `VILLAGERS JOIN FOR FREE` (best 45) |
| 4 | 6 | Home – camp 5; Home – shrine ×2; camp 5 – camp 9; shrine – camp 9; camp 9 – Keep 40 | `GROW FIRST, DOUBLE LAST` (shrine first strands at 26; best 80) |
| 5 | 12 | Home – camp 8 – post −6 – village +10 – camp 20 – Keep 40, and Home – camp 3 | `A POST COSTS SOLDIERS` (best 87) |
| 6 | 10 | Home – camp 6; then a fork: left camp 15, right village +6; both – Keep 30 | `ONE BRIDGE BURNS` (right strands at 22; best 61) |
| 7, 8 | — | Recipes `Hourglass` and `Tree` at seeds picked by hand with the solver: every element so far, 8 and 9 sites, random play wins at least 70% | none |
| 12 | 10 | Section 2 | `IT GROWS EVERY TURN` |

### 8.2 The calendar

Which kind each level is, per block of ten after its introductions (the tenth is the boss):

| Levels | Conquest | Gauntlet | Siege | Race | Last Stand | Boss |
|---|---|---|---|---|---|---|
| 9–40 | 7–9 | 0–2 (from 16) | — | — | — | 1 |
| 41–80 | 6 | 2 | 1 | 0–1 (from 60) | 0–1 (from 75) | 1 |
| 81–300 | 4 | 2 | 1 | 1 | 1 | 1 |

Within a block no kind but Conquest appears twice in a row. If the owner says at P2 that another kind should lead, this
table is the one thing to change.

### 8.3 The generator

A **recipe** places sites and roads; a second step gives them kinds and numbers. Recipes are planar by construction
(no road crosses another) on a 100 × 160 lattice (Gauntlet: 100 × up to 420).

| Kind | Recipes |
|---|---|
| Conquest | `Tree` (branches from Home), `Ladder` (two columns and rungs), `Ring` (around a lake), `Hourglass` (two lobes and a bridge), `Islands` (clusters joined by posts over the river), `Crossroads` (a hub), `Twin Valleys` (two routes that meet at the Keep), `Spiral` |
| Gauntlet | `Pairs`, `Triples`, `Braid` (forks and free rows) |
| Siege | `Ringed Keep`, `Supply Line` |
| Race | `Parallel Roads`, `Pincer` |
| Last Stand | `Three Roads`, `Homestead` |

Numbers come from a forward walk: pick an intended order (a random walk over the map that respects reachability),
carry `S` along it, and set each camp to a share of the `S` it will meet (30% to 95%: the closer to 100%, the tighter).
Then add **traps**: an early shrine in reach, a tempting village behind a fork, a growing camp that the intended order
leaves for late, a strong camp next to Home. Site numbers stay under 100,000 and are shown in full; friendly values
(steps of 1 under 30, 5 under 300, 10 or 50 above). Start strengths are 5 to 20 in the first hundred levels.

### 8.4 Acceptance (the bake keeps a seed only if every line holds)

1. The solver proves it winnable and gives `best`; `best` is at least 4 × the start strength (the snowball) and the
   Keep is at least 1.5 × the strongest camp.
2. **Random play** (a uniformly random takeable site each move, 400 plays) wins within the level's band, and
   **greedy** (always the weakest takeable camp, else any free site) wins or loses as the band says:

   | Levels | Random play wins | Greedy | Random play reaches three stars | Sites |
   |---|---|---|---|---|
   | 9–30 | 70–100% | wins | 3–40% | 6–10 |
   | 31–80 | 45–85% | either | 1–20% | 8–13 |
   | 81–160 | 25–65% | loses on at least 4 of each 10 levels | at most 10% | 9–16 |
   | 161–300 | 12–50% | loses on at least 6 of each 10 | at most 5% | 10–20 |

3. Three stars need at least one real decision: at least two takeable sites at some turn where only one keeps `best`.
4. The layout lint passes (tags clear of each other and of the HUD at the eight aspect ratios; sites at least 56 dp
   apart on the smallest frame).
5. **No repeats**: within any 12 consecutive levels no two share a recipe and the same set of element kinds, and a
   layout fingerprint (site count, degree sequence, kinds in order of distance from Home) is unique in the table.
6. The solver's cost from the start is within the device budget (TECH_SPEC 3.5).

The bake writes a report with every level's numbers and the band tables; P5 reads it back.

---

## 9. Look

### 9.1 The idea

A pop-up book of war maps. Each level is a page of cut paper lying on a wooden table: a cream page, a land of bright
flat colour glued on it, lighter terraces on the land, a river cut out of it, stitched sand roads. Everything that
stands up (trees, tents, houses, the shrine, the Keep, flags, the soldiers themselves) is a paper cut-out standing on
the page: two-tone where it folds, throwing a soft shadow down and to the right. Daylight. Flat colour, soft shadows,
paper grain, no outlines, no gradients inside a shape, **no glow and no bloom**.

`concept/concept_gameplay.png` is the target; `concept/make_concept.py` holds its numbers.

### 9.2 Colours

Constant in every world (the game's identity):

| What | Colour |
|---|---|
| The host: body / fold shade | ivory `#FFFBEE` / `#E9DCC2` |
| The host's scarf, banner, taken sites' flags and tents | scarlet `#F03A4C` / `#B81F3A` |
| Gold: the takeable tag, the banner's trim, stars, the bearer's scarf | `#FFC83A` / `#E09010` |
| The enemy: body / fold light | ink `#292C55` / `#474D8C` |
| Text on light paper | `#33221A` |
| Shrine | violet `#8B5CF2` / `#B79AFF` |
| The page | `#FFF4D6` |
| Shadow | `#26140A` at 35% to 55% |

World 1, Greenmeadow (from the concept): land `#74C658`, terraces `#96D864` and `#BDEA7C`, forest `#23965C` /
`#46BE74`, blossom trees `#FF86AE` / `#FFB9D0`, river `#24BEDA` with ripples `#9AECF4`, road `#F9E4A8` with stitches
`#C8964E`, the enemy's accent orange `#FF7A2A`, the table `#70482C` to `#45281B`.

The ten worlds (starting palettes; P5 renders three candidates each and the lint picks or the owner does):

| # | World | Land (base → terraces) | Trees | Water | Enemy accent | Table |
|---|---|---|---|---|---|---|
| 1 | Greenmeadow | fresh greens | pine green, a few pink blossoms | turquoise | orange `#FF7A2A` | warm oak |
| 2 | Sunbaked | sand golds `#F2B45A` → `#FBDD96` | teal palms and cacti | oasis teal | magenta `#E0338F` | terracotta |
| 3 | Frostvale | glacier blues `#A9D3EC` → `#DCF0FB` | blue pines, white caps | ice `#7FD6F2` | violet `#7A4DE0` | grey driftwood |
| 4 | Emberfall | autumn oranges `#E8873A` → `#F6BD6A` | crimson and gold maples | teal | cyan `#19C8D6` | dark walnut |
| 5 | Tidereach | sand islands on a turquoise sea page | palms | the sea itself | lime `#B6F23A` | whitewashed boards |
| 6 | Bloomgrove | blossom pinks `#F7A8C4` → `#FFDCE8` | white and pink | sky blue | emerald `#12B886` | pale birch |
| 7 | Mirefen | teal-olive `#5FA88A` → `#A2D6B4` | violet mushrooms, reeds | murky blue | hot pink `#FF4FA3` | mossy planks |
| 8 | Skyhold | green islands on a sky-blue page with white clouds | round bushes | cloud gaps | purple `#9B4DFF` | pale blue cloth |
| 9 | Cinderpeak | plum ash `#6A4A6E` → `#A57CA6` | ember trees | lava orange paper | acid green `#9BE22D` | black slate |
| 10 | Lanternfair | dusk lavender `#C79BE0` → `#E8CFF5` | lantern strings, plum trees | rose | tangerine `#FF8A3D` | plum cloth |

**The palette lint** (a unit test over the table): the middle terrace's luminance is between 0.30 and 0.72; ivory
against it has a contrast of at least 1.3 and ink at least 3.0; the accent's hue is at least 40° from scarlet and from
gold; in a greyscale render the host and the enemy are still told apart; neighbouring worlds differ in land hue by at
least 40°.

### 9.3 Figures

A **pawn** is a head and a bell-shaped body with a scarf, 20 dp tall on the page. The host is drawn as a column behind
its banner bearer (larger, gold scarf, a scarlet banner with a gold stripe), three abreast, following the road.

How many figures show (it never shrinks as the number grows):

| Figure | Stands for | Host shows | A garrison shows |
|---|---|---|---|
| Pawn | 1 | `clamp(S, 3, 40)` | `clamp(v, 1, 20)` |
| Rider (a pawn on a folded paper horse, 1.5 × as tall) | 100 | `min(S / 100, 30)` | `min(v / 100, 12)` |
| Giant (3 × as tall, a paper colossus with a banner) | 10,000 | `min(S / 10000, 12)` | `min(v / 10000, 5)` |

### 9.4 Tags

A rounded paper label on a little pointer, above its site, the number in the heavy rounded font. Tags are drawn after
every pop-up and figure: a tag is never hidden. States:

| State | Tag | Extra |
|---|---|---|
| Takeable now | gold, dark number, white edge | bobs 3 dp; a dotted gold road from the host |
| Too strong | ink, ivory number | a lock; the number's shortfall on a refused tap |
| Free (village, post, mire) | ivory | the number in the element's colour |
| Shrine | violet, white number | |
| Not reachable yet | as its state, at 70% size | |
| The Keep | ink, gold number, gold edge, 1.4 × | |
| The host | ivory, scarlet number and edge, 1.25 × | follows the banner |

Numbers: in full up to 99,999 for sites; the host's in full up to 999,999, then three significant digits (`1.24M`).

### 9.5 The page turn and the pop-up

Between levels the page turns: the old page curls away from the bottom right corner, its back the page's cream, and
the new map lies under it flat; as the curl passes, the new page's pop-ups stand up in a wave from the top (each rises
from flat in 250 ms with a small overshoot, its shadow growing with it), the Keep last. This is the game's signature
move; section 10 has its numbers.

---

## 10. Juice (timings; all tweakables)

| Moment | What happens |
|---|---|
| Tap accepted | The tag pops (1.15 × for 80 ms); a light haptic; a wood tick |
| March | The banner hops along the road at 90 map units a second, at least 300 ms, at most 800 ms; the column follows in a wave; dust scraps at the feet; footstep ticks on the beat |
| Camp fight | The view leans in (1.10 × toward the site, 200 ms); the two crowds meet for 300 ms with paper scraps in both colours; then the garrison **flips**: each enemy figure turns over (its width through zero, 120 ms) to ivory, 30 ms after its neighbour, and hops into the column. Tents flip to scarlet, a scarlet flag rises |
| Count-up | The host's number climbs to its new value in 500 ms, at most 24 ticks, each a step up the count ladder (section 12); the tag grows 1.3 × and settles |
| Village | Villagers run out of the houses and join; a rising three-note chime |
| Shrine | Every figure unfolds into two (`×3`: three); a violet ring of confetti; the count-up runs double speed; a glockenspiel run |
| Post | Pawns peel off and take their places on the towers; the number counts down; coins |
| Mire | Half the column sinks to flat scraps that blow away; a low reed note |
| Fork burnt | The rope bridge burns as orange paper curls (no flash); the lost sites fold flat |
| Refused tap | The tag shakes 6 dp three times in 240 ms; two low wood knocks; the host's bearer shakes its head |
| Win | The Keep folds flat and stands up again in scarlet and ivory; the host cheers (a hop wave); two confetti cannons; the fanfare; stars land one at a time on the beat. `PERFECT` adds a gold confetti rain and the glockenspiel run |
| Lose | Stranded: the Keep's gate opens, the garrison pours out, the host scatters as scraps on the wind; a falling horn. The panel offers UNDO first |
| Page turn | 600 ms curl, the pop-up wave 400 ms behind it; a long paper swish, then ratchet clicks as things stand |

A second move queued: everything above at 2.5 ×. `FAST MOVES` in SETTINGS makes that the default. With "reduce
effects": no lean-in, confetti at a quarter, the flip is a cross-fade, the page turn is a slide.

Confetti budget: 600 scraps alive at once.

---

## 11. Screens and flow

GLOWTIDE's flow (its GAME_DESIGN 11), restyled as paper: every panel is a cream card with a soft shadow; primary
buttons are gold with dark text, secondary ivory; a pressed button sinks 2 dp and darkens to a contrast of at least
4:1 against its label.

- **HOME**: the level's map already on the table, the title as cut letters, `LEVEL N` chip (opens LEVELS), stars
  total, SETTINGS, the UPDATE pill when a newer build exists. A touch anywhere else starts the level.
- **PLAY HUD**: top: the level chip with the level's saved stars, the objective chip, pause. Bottom: UNDO, the host
  plate (`YOUR HOST` and the number, the largest text on screen), HINT. The dead-end slip and the hint line sit under
  the top row.
- **PAUSE**: RESUME, RESTART, HOME, SETTINGS.
- **WIN**: stars, `HOST n · BEST m`, NEXT (primary), RETRY when under three stars.
- **LOSE**: what happened in four words (`THE HOST IS STRANDED`), UNDO (primary), RETRY.
- **SETTINGS**: sound, music, haptics, fast moves, reduce effects, the version and build line, credits.
- **LEVELS**: GLOWTIDE's selector unchanged in behaviour (five a row, stars under each, the current one gold, `-100`,
  `PREV`, `NEXT`, `+100`, BACK, every level open).

---

## 12. Sound

Everything is synthesized at start-up from recipes (GLOWTIDE's synth primitives); nothing is recorded. The palette is
wood, paper, drums and brass: a toy marching band. It must not sound like GLOWTIDE (no kalimba, no ambient pad bed).

### 12.1 The march (music)

A step sequencer on sixteenth notes drives pre-synthesized one-shots through the mixer. One theme of 8 bars in 2/4 a
world, looping. Five layers; how many play follows **how close the host is to its goal**, `r = S / goal` (the goal is
the Keep's number, or the next raid's):

| Layer | Plays when | Instruments |
|---|---|---|
| 0 | always | a soft frame drum on the beat, a held open fifth |
| 1 | `r ≥ 0.10` | snare rudiments (rim clicks at low `r`), a low tom on beat 1 |
| 2 | `r ≥ 0.30` | the bass (a tuba-like square with a slow filter), oom-pah |
| 3 | `r ≥ 0.60` | the melody (a fife: sine and breath noise) doubled by a glockenspiel an octave up |
| 4 | `r ≥ 1.00` | brass harmony, cymbal swells: **you can take the Keep now**, and the ear knows it |

A layer fades in over one bar. The theme of world 1 in D major at 120 BPM, as scale degrees (8 = the octave) with
lengths in sixteenths:

```
bar 1: 1(2) 1(1) 2(1) 3(2) 5(2)      bar 5: 8(2) 8(1) 7(1) 6(2) 5(2)
bar 2: 6(2) 5(2) 3(4)                bar 6: 6(2) 4(2) 2(4)
bar 3: 4(2) 4(1) 5(1) 6(2) 8(2)      bar 7: 3(2) 5(2) 4(2) 2(2)
bar 4: 7(2) 5(2) 5(4)                bar 8: 1(8)
chords: I  I  IV  V  I  IV  V  I
```

P4 renders this and two variants of the executor's own for the owner to choose from. The other worlds use the same
theme in their own mode, tempo and lead voice (P5):

| World | Mode, tempo | Lead |
|---|---|---|
| 1 Greenmeadow | D major, 120 | fife and glockenspiel |
| 2 Sunbaked | D Phrygian dominant, 108 | a plucked string, frame drum |
| 3 Frostvale | F major, 112 | celesta, sleigh bells |
| 4 Emberfall | A Dorian, 116 | a reed organ |
| 5 Tidereach | G Mixolydian, 124 | a steel-drum-like bell, shaker |
| 6 Bloomgrove | F major pentatonic, 104 | a plucked string and a flute |
| 7 Mirefen | E natural minor, 100 | a bassoon-like reed, marimba |
| 8 Skyhold | C Lydian, 126 | piccolo and celesta |
| 9 Cinderpeak | D harmonic minor, 132 | low brass, an anvil on the offbeat |
| 10 Lanternfair | B♭ major, 112, swung | a music box and a clarinet-like reed |

A boss level adds a timpani pedal and plays a fifth lower.

### 12.2 Effects

| Sound | Recipe (starting point) |
|---|---|
| Count ladder | A wood block (a sine at the note with a 60 ms decay and a 2 ms click). Each tick of a count-up is the next step up the world's scale from the current chord's root; a count-up of 24 ticks climbs two octaves. A count-down descends |
| Paper flip | A 40 ms noise burst through a band-pass sweeping 2 to 6 kHz; one a figure in a flip wave, at most 12 a second |
| Clash | A drum thump (70 Hz, 120 ms), a metallic ring (four inharmonic partials, 300 ms) and a shouted "hah" (noise through two formants), on the next sixteenth |
| Village | Three rising glockenspiel notes (1, 3, 5 of the chord) |
| Shrine | A glockenspiel run of eight notes up the scale and a soft cymbal |
| Post | Three coin clinks falling in pitch |
| Mire | A low reed note falling a fourth, with wet noise |
| Refuse | Two low wood knocks a minor second apart |
| Fork burning | Crackle (sparse noise clicks) for 500 ms |
| Raid horn, rival step | A two-note low horn; one deep drum |
| Win | A brass arpeggio (1, 3, 5, 8) into a held chord, a cymbal, a short choir "ah"; a star is a bell on the beat. `PERFECT` adds the run |
| Lose | A horn falling a fifth, then paper tearing (long noise, falling filter) |
| Page turn, pop-up | A 500 ms swish (noise, a rising then falling filter); then quick ratchet clicks, one a pop-up, at most 16 |
| UI tap | A soft wood tick |

Levels: the effects bus and the music bus as GLOWTIDE mixed them (its PROGRESS, "Mix levels"): the limiter holds
samples at −1.5 dBFS; the music sits 9 ± 2 dB under a busy battle render. A phone speaker has no bass: every
instrument has energy above 300 Hz.

---

## 13. Haptics

GLOWTIDE's kinds and rate limit (twelve a second of wall-clock time): a light tick on an accepted tap, a medium one on
a clash, a double tick on a refusal, the win pattern, the lose buzz. Count-ups are not haptic.

---

## 14. Accessibility and safety

- No information by colour alone: takeable is gold **and** bobbing with a dotted road; too strong is ink **and** a
  lock; shrines, posts and mires differ in tag shape and sign (`×`, `−`, `½`).
- Text: numbers on tags at least 3% of the frame's height; body text at least 14 dp.
- No full-screen flashes, ever. Confetti and paper only. The flash audit (GLOWTIDE's `flash-audit.ps1`) covers the win,
  boss win, lose and page-turn strips.
- "Reduce effects" (section 10) and "fast moves".
- One thumb reaches everything: UNDO, HINT and the host plate are in the bottom band; nothing needs two fingers.

---

## 15. Names

Worlds: section 9.2. Boss Keeps, one a world: Thornwick, Sandspire, Coldharbour, Rustgate, Saltcrown, Petalmoat,
Bogholt, Cloudrook, Ashenhall, Lanternkeep. The rival is "the Warlord". The player's side is "the host"; soldiers are
"pawns", "riders", "giants". Strings live in one table; all caps in the UI, as in GLOWTIDE.
