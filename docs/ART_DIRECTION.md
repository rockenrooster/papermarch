# PAPERMARCH — Art direction

Owner's brief: "Visuals and graphics need lots and lots of work, it needs to be visually impressive." Bold, colourful,
layered, varied; a spectacle. This document is the look, the asset list and what the engine has to do for it.
Generators: `tools/art/` (see its README). Runtime files: `assets/art/`.

## 1. Style: the painted toy-box diorama

**Chunky hand-painted toy soldiers marching through a sunlit tabletop diorama.** Every figure is a real 3D model
(built by script in Blender, rendered with Cycles) with glossy enamel paint, a baked sun and a sky fill, then shipped
as a sprite. The world is a model-railway landscape: cobbled road, round lollipop trees, windmills, cottages, picket
fences and bunting.

Why this and not flat paper:

- The flat stickers read well but have no light, no volume and no material: that is what looks cheap. Rendered toys
  get specular highlights, soft form shadows, ambient occlusion and colour bounce for free, at the cost of one quad each.
- Toy proportions (head and hat 45% of the height, stub legs) keep the silhouette legible at 20 px tall, which a
  realistic figure does not.
- A diorama in daylight is the opposite of GLOWTIDE (glowing wisps, twilight, void): solid things, hard sun, green land.
- The paper name still fits loosely (toy soldiers, a paper-flag army); the name is unconfirmed anyway. The page-curl
  transition and the cream paper UI cards can stay.

Three rules for every asset:

1. **Silhouette and value first.** Each kind has one shape nobody else has (section 4) and reads as a blob of one
   colour at 20 px: squad = cobalt with a white plume dot, horde = crimson with dark steel, gates = big flat panels.
2. **Saturated things on a calm ground.** The road and land are mid-value and lower in saturation than every unit;
   the brightest and most saturated pixels on screen are units, gates, barrels and effects.
3. **One sun.** Everything is lit from the same direction (section 3), including the baked shading in ground textures.

A thin dark rim (about 1.5 px at sprite resolution, the sprite's own colour darkened, 55% alpha) is baked around
every unit so figures separate from each other in a crowd of 1,000.

## 2. Palette

| Role | Colour | Notes |
|---|---|---|
| Squad coat | `#1F58F2` cobalt, hat `#1440C4` | the only large blue on the road |
| Squad plume / belts | `#FFFFFF`, gold trim `#FFC83A` | the white plume is the top-down "dot" |
| Squad pack | `#7A4E2C` dark leather, bedroll cobalt with gold ends, navy belt | seen from behind, which is how the squad is always seen; kept dark so the squad reads blue |
| Horde tunic | `#E3262E` crimson, shade `#8E0F1E` | |
| Horde metal | `#3A3F4C` dark steel, ivory `#F3E6C4` | dark helmets keep red figures apart from red gates |
| Brute | `#A3131F` oxblood, shield `#5B0E18` with gold `#FFC83A` boss | darker than the foot |
| Boar (rider's mount) | `#6B4630`, tusks ivory | |
| Ogre (world 1) | skin `#6FAE3C` moss, belly `#9CCB5A`, loincloth crimson, iron `#4A4F5C`, club `#7A5232` | green = not squad, not gate |
| Gate + | `#2E8BFF` panel, `#BFE4FF` highlights, posts cobalt and gold | |
| Gate x | `#9B4DFF` panel, `#E6CCFF` highlights, posts violet and gold | |
| Gate - (barricade) | `#F0323C` panel, `#FFC2B8` highlights, dark-wood spiked posts | |
| Barrel | gold `#FFC228`, bands cobalt `#2456D8`, white star | "this is yours": squad colours on gold |
| Bullets / muzzle | `#FFF3B0` core, `#FFB020` edge | additive |
| World 1 road | stone `#CDBBA0` / `#B9A486`, joints `#7C6A52`, curb `#F4EBDD` | value about 70%, saturation under 25% |
| World 1 land | grass `#6FBF4A` / `#58A83E`, flowers white, yellow, pink | |
| UI | cream `#FFF6E0` cards, ink `#3A2A1E`, cobalt and gold accents | stage 3 |

Value plan (sRGB luma, world 1): road 0.70, grass 0.58, squad 0.35 with 1.0 plume, horde 0.30, gates 0.45-0.6 with
white numbers, effects 0.9-1.0 additive. Worlds change the ground hue but keep road luma within 0.55-0.75 (Cinderpeak,
the dark world, 0.40 with rim-lit units; section 8).

## 3. Lighting (baked, the same in every sprite and texture)

World axes: x right, z up the road (away from the camera), y up.

- **Sun** (key): the light comes from behind the camera's left shoulder. Direction to the sun
  `L = normalize(-0.45, 0.70, -0.55)` (x, y, z): 44 degrees above the ground, 39 degrees left of straight behind the
  camera. Colour warm white `#FFF1D6`, strength 3.0 (Blender sun), angle 6 degrees (soft-edged form shadows).
  The camera sees the lit side of both armies (the squad's backs and the horde's fronts).
- **Sky fill**: uniform world light `#BFD9FF`, strength 0.9. Shadows are blue-tinted, never black.
- **Rim**: a second sun from ahead-right, `normalize(0.6, 0.35, 0.72)`, `#FFFFFF`, strength 1.2: a bright edge on the
  right and top of every figure, which separates crowds.
- **Ground bounce**: a warm plane under the figure (`#D9C7A6`) lifts the undersides.
- View transform Standard (not AgX or Filmic: they grey out saturated toy paint), 8-bit sRGB.
- **Cast shadows** are the engine's: fall toward **+x, +z** (right and up the road), length about 1.0 x the thing's
  height on the ground (`shadow` line in `worlds.txt`, engine request 2).

## 4. Characters

All rendered orthographic at fixed elevations (section 5). Heights are in road units; the squad faces +z (we see
backs), the horde faces -z (we see fronts). March cycles are 8 frames at 12 fps, looped; the engine should offset each
figure's phase.

| Kind | Height | Footprint | Silhouette (the one-glance read) |
|---|---|---|---|
| `soldier` | 1.2 (plume top) | 0.45 wide | Tall cylinder hat (shako) with a white plume, tan pack with a cream bedroll, musket pointing up the road on the right side. From above: blue disc, white dot, a thin dark line (the gun) ahead of it. |
| `foot` | 1.05 body, spear to 1.6 | 0.5 | Pointed dark steel helmet (a cone), crimson tunic, small round wooden shield, a spear held upright with a red pennant. From above: a dark point in a red blob, plus a thin tall line. |
| `rider` | 1.7 | 0.7 wide, 1.5 long | A red raider on a galloping dark-brown **war boar** (tusks, bristle ridge), lance levelled forward with a long swallow-tail pennant. The only long, horizontal shape in the horde. |
| `brute` | 1.9 | 1.3 wide | Twice as wide as a foot soldier: oxblood barrel body, tiny legs, horned steel helmet, a big round shield with a gold boss held in front, a spiked mace on the shoulder. From above: a dark-red disc with a gold dot and two ivory horns. |
| `boss` (one model for every world) | 5.8 | 4.3 wide | The ogre: moss-green, huge pale belly, small head sunk between spiked iron pauldrons, two long ivory horns, under-bite tusks, glowing yellow eyes, crimson loincloth with a gold buckle, a tree-trunk club with iron bands in the right hand. Stomping walk. Nothing else on the road is green or that big. |

Boss per world: the one shared ogre, recoloured by the engine (`boss <hueDegrees> <tint>` in `worlds.txt`: rotate
the hue, multiply by the tint) plus an add-on sprite drawn over it (`bossaddon wN.boss_addon`, rendered in the boss's
frames with the ogre as a hold-out, so horns and arms cut into it correctly):

| World | Boss | hue, tint | Add-on |
|---|---|---|---|
| 1 | OGRE CHIEF | 0, `#FFFFFF` | none |
| 2 | SAND OGRE | -52, `#FFE9C4` (ochre) | a cream turban with a jewel and a plume |
| 3 | FROST OGRE | 100, `#E6F2FF` (ice blue) | a white fur mantle |
| 4 | EMBER OGRE | -88, `#D8B8A8` (burnt orange) | a crown of flames, burning eyes |
| 5 | SALT OGRE | -125, `#C8D8D0` (dusty rose: a grey-green tint mutes the pink) | pink-white crystal shards on the shoulders |
| 6 | THORN OGRE | 45, `#88B8A8` (dark teal green, clear of world 1's moss) | a flower crown |
| 7 | BOG OGRE | -34, `#B8A878` (olive mud) | toadstools on the shoulders |
| 8 | CLOUD OGRE | 150, `#F4F0FF` (blue-violet) | a gold crown with gems |
| 9 | ASH OGRE | -100, `#6A5A60` (charred) | flames on the horn tips, red eyes |
| 10 | LANTERN OGRE | 215, `#DCB4DC` (magenta plum) | paper lanterns hanging from the horns |

The hue rotation also turns the loincloth and buckle; that is wanted (each ogre gets its own trim). Try values with
`tools/art/boss_tints.py`, which uses the shader's maths (a multiply tint cannot lighten or desaturate: to mute a hue, tint
with its complement).

Two more squad sprites: `squad.banner.march` (8 frames, elevations 60/38/20, 2.0 x 1.1 unit cobalt cloth with a gold
border on a 2.8 unit pole, leaning back to face the camera; point `text` = the cloth's centre) and `soldier.cheer` (8
frames at 12 fps, a jump with both arms up and the hat thrown and caught; for the survivors after a win).

Variation (engine request 5): per-figure scale 0.94-1.06, a multiply tint of value 0.90-1.06, phase offset. Do not
mirror unit sprites (the sun would flip).

## 5. Camera

Chosen: **`camPitch = 30`, `camHeight = 15`, `camBack = 8.5`** (`camRoad` 0.92, `camSquadY` 0.80 unchanged).

- The squad is seen at 60 degrees instead of 73: bodies, packs and guns show instead of hat tops.
- The clash zone (10-25 units ahead) is seen at 39-24 degrees: the horde shows faces, shields and spears.
- At 1080x2400: 99 px per unit at the squad, 63 at 10 units ahead, 46 at 20, 30 at 40, 21.5 at 62 (where waves wake,
  y = 572), 17.5 at 80 (y = 503). The horizon is at y = 199 (8% from the top), behind the HUD row, so a strip of sky
  and a far backdrop show between the HUD chips: the world has a far end instead of running off the screen top.
- 80+ units of road are visible (the limit asked for was 60). The road between the squad and 40 units ahead covers
  y 1920 to 716: half the screen, against two thirds with the old camera. Checked with the frozen build
  (`captures/art/cam_abc.png`: a = 30/14/9, b = 26/12/10, c = 32/16/8); lower than about 14 the far horde becomes too
  small to shoot at knowingly.

Sprite elevations rendered: **60, 38, 20** degrees for units, barrel, gate parts and edge posts (the boss and its
add-ons: 38 and 20 only; it never gets nearer than about 11 units from the camera's foot). Props: 45 and 20. Effects and ambient: 0.
The engine picks the nearest (thresholds 49 and 29 for units).

## 6. Gates, barrels, barricades

### Gate (the panel is a shader; posts and the cap are sprites)

A gate is a banner of light stretched between two posts across x0..x1, bottom 0.25 above the road, top at 2.2 units.

- **Posts**: `gate.post_add`, `gate.post_mul`, `gate.post_neg` (2.7 tall, pivot at the base), one at x0 and one at x1.
  Add: cobalt-and-white striped pole on a stone foot, gold ball finial, a small blue pennant. Multiply: violet and
  gold spiral pole, gold star finial. Negative: a dark sharpened timber post with iron bands and a red rag.
- **Barricade stakes**: `gate.spikes` (1.0 wide, 0.9 tall, pivot at the bottom centre), tiled along the bottom of a
  negative gate every 1.0 unit between the posts, drawn at the gate's z, in front of the panel.
- **Panel shader** (alpha blend, one quad; uv: u 0..1 across, v 0 bottom..1 top; `t` seconds; base colour C, highlight
  Hc from section 2):
  - Body: vertical gradient `mix(C * 0.75, C, v)`, alpha 0.62 at v = 0 rising to 0.86 at v = 1.
  - Frame: a 0.06-unit border in Hc at alpha 0.95 (top and bottom edges; the posts cover the sides), and a 0.02-unit
    inner line in white at alpha 0.5.
  - Moving chevrons: `+` gates: upward-pointing chevrons, period 0.45 units, scrolling up at 0.6 units/s, drawn as
    `C * 1.25` at +0.12 alpha. `x` gates: diagonal stripes at 45 degrees, period 0.4, scrolling sideways at 0.5
    units/s plus 6 twinkling 4-point stars (white, additive, random cells, 0.6 s life). `-` gates: downward chevrons at
    0.4 units/s and a dark vignette at the bottom (`C * 0.5` over the lowest 25%).
  - Sheen: a soft white band 0.18 of the width wide, tilted 20 degrees, sweeping left to right once every 2.4 s, +0.25
    additive.
  - Glow: write the top edge and the number at 1.3x brightness so the bloom catches them (threshold in `worlds.txt`).
  - Number: the existing white digits with a dark outline of `C * 0.35`, 1.25 units tall, centred, scaled to at most 80%
    of the panel's width; it pulses to 1.15x for 0.12 s each time the value changes.
- **Hit flash** (a bullet grew the gate): panel adds white 0.35 decaying over 0.10 s; the number pops (above); one
  `fx.hit` at the bullet's point.
- **Passed** (the squad's gate): the panel flashes white (0.6 decaying over 0.15 s) and collapses downward (scale y
  1 to 0 in 0.18 s, ease-in); `fx.gatepass_add` / `_mul` / `_neg` (additive, 10 frames) at the panel's centre, 4 units
  wide; 24 `fx.confetti_*` pieces in the gate's colours for + and x; for a `-` gate 10 `fx.poof_blue` instead and a
  short red screen-edge flash. The other gates of the row fade to 25% alpha over 0.3 s and stop animating.
- A negative gate shot up into a positive one: 0.25 s cross-fade of C from red to blue, posts swap, one `fx.sparkle`
  burst.

### Barrel

`barrel` (1.6 tall, 1.25 wide: bigger than the sim's barrel so it reads at 40 units): a fat golden keg with cobalt iron bands, gold rivets and a white star on the front and
the lid, a short lit fuse on top. Hit: the engine's white flash plus a 6% squash. Burst: `fx.explosion` (alpha, 14
frames at 24 fps, 4.5 units), `fx.explosion_glow` under it (additive), `fx.ring` on the ground (additive, 5 units),
8 `fx.debris` planks thrown as ballistic particles, then the freed recruits pop in (scale 0 to 1.15 to 1 over 0.2 s)
with `fx.sparkle` each.

## 7. Road and land

- **Road** `ground_w1_road.png` (1024x1024, spans 12 units across: 10 of road plus a 1-unit curb each side; one repeat
  is 12 units long): irregular fitted flagstones in warm cream-grey with darker joints, a worn lighter track in the
  middle third, moss in some joints near the curbs; the curbs are white dressed-stone blocks with a baked highlight on
  the top and a shadow on the inner side (they read as raised). Baked sun from section 3.
- **Land** `ground_w1_land.png` (1024x1024, 16 units a repeat): mown-meadow grass in two greens with darker tufts,
  clover patches and tiny white, yellow and pink flowers. Lower contrast than the road.
- Finish line: keep the engine's checks, drawn over the road texture (black and white, 2 rows of 10, 1 unit squares).
- Haze: distance fog to `#9CCBEC` from 60 to 150 units ahead of the camera, so the horizon melts into the sky. The haze
  colour must stay under the bloom threshold (luma below about 0.8), or the far road blooms into a white band.
- Sky: `#3F98EA` at the top, `#9CCBEC` at the horizon; backdrop strip `backdrop_w1.png` (far hills, a castle and
  clouds) standing on the horizon.

## 8. Props and the ten worlds

Every world has 6 to 9 scattered props, two edge props (`wN.post` on the curb every 3 units, `wN.banner` every 12),
one or two ambient particles and its boss add-on, all in one atlas (`atlas_props_wN.png`, 256 colours). The builders
are shared and coloured per world (`render_sprites.py`); density is the `prop` lines of `worlds.txt` (written from
`tools/art/worlds.py`). Big landmarks are rare and far from the road (x 12-22). As built:

| # | Props (`wN.` + name) | Post / banner | Ambient |
|---|---|---|---|
| 1 | tree_a (apple oak), tree_b (poplar), tree_c (blossom), bush, rock, flowers, haystack, cottage, windmill (8 frames, turning) | cobalt-white, gold ball / cobalt | leaf, petal |
| 2 | palm, cactus, tent (magenta stripes), arch (sandstone), pots, rock | sand, gold point / magenta | sand |
| 3 | fir (snowy), snowman, crystal (ice), cabin (snow roof), rock, bush (snow mound) | ice blue, lantern / ice blue | snow |
| 4 | maple_a (red), maple_b (gold), pumpkins, scarecrow, haystack, barn, bush | timber, gold point / orange | leaf |
| 5 | palm, umbrella, lighthouse, boat, rock, grass (dune grass) | white-teal, buoy / teal | feather, spray |
| 6 | cherry, wisteria, tulip_a, tulip_b, beehive, bush, flowers | pink-white, flower box / pink | petal, petal_w |
| 7 | mangrove, mushroom_a (red), mushroom_b (glowing teal), reeds, hut (on stilts), rock | timber, torch / green | firefly (add), spore |
| 8 | column, ruin, statue (gold star), cloud_tree, airship, puff (cloud) | marble-gold / white-gold | feather, wisp |
| 9 | charred, vent (lava), obsidian, lava_crystal, brazier, rock | black-red, skull / black, orange star | ember (add), ash |
| 10 | lantern_tree, stall, tent_a, tent_b, balloons, ferris (8 frames, turning), bush | red-gold, lantern / violet | confetti, lantern (add) |

The order of worlds is kept. Each world changes: ground textures, prop set, haze and sky, grade, ambient, edge props,
the boss variant. Units keep their colours (readability), the grade tints them slightly.

| # | World | Ground | Props | Sky / haze | Ambient | Edge |
|---|---|---|---|---|---|---|
| 1 | GREENMEADOW | cream flagstones, mown grass | oaks, poplars, blossom trees, haystacks, cottage, windmill | bright blue, pale cyan haze | leaves, petals | striped bollards, blue banners |
| 2 | SUNBAKED | sandstone slabs, rippled dunes | palms, cacti, striped tents, sandstone arches, pots | hot blue to peach haze | sand wisps | rope posts, magenta banners |
| 3 | FROSTVALE | blue-grey setts with snow in the joints, snow with ski tracks | snowy firs, snowmen, ice crystals, log cabins | pale turquoise, white haze | snowflakes | lantern posts, ice-blue banners |
| 4 | EMBERFALL | warm brick herringbone, fallen leaves on ochre grass | maples in red, orange and gold, pumpkins, scarecrows, hay carts | amber haze, low sun grade | falling maple leaves | split-rail fence, orange banners |
| 5 | TIDEREACH | pale boardwalk planks, sand with shells and tide pools | beach umbrellas, lighthouses, boats, driftwood, palms | aqua sky, sea-mist haze, sea on one side | gulls' feathers, spray | rope and buoys, striped flags |
| 6 | BLOOMGROVE | pink-grey cobbles with petals, flower meadow | cherry and wisteria trees, giant tulips, beehives, trellises | soft rose haze | petals, bees | flower boxes, garlands |
| 7 | MIREFEN | mossy planks and stepping stones, dark water with lily pads | mangroves, giant mushrooms, reeds, stilt huts, glowing toadstools | green-teal haze (near: starts at 30) | fireflies (day-visible yellow), spores | torches, tattered green flags |
| 8 | SKYHOLD | white marble with gold inlay, cloud tops and floating lawns | marble columns, gold statues, kites, airships, cloud trees | deep cobalt top, white horizon | cloud wisps, feathers | gold railings, long white banners |
| 9 | CINDERPEAK | dark basalt with glowing lava joints, ash and obsidian | charred trees, lava vents, obsidian spikes, iron braziers | brown-red haze, orange horizon; bloom up | embers, ash | chains and skulls, black-red banners |
| 10 | LANTERNFAIR | plum cobbles with confetti, festival lawn | lantern trees, market stalls, striped tents, balloons, a Ferris wheel | warm afternoon, pink-gold haze | confetti, paper lanterns | strings of bunting and lanterns |

Cinderpeak is the only low-key world: road luma 0.40, and unit sprites get +10% brightness from the `light` line so the
squad still reads.

## 9. VFX list

Flipbooks are generated by `tools/art/make_fx.py`; every effect's anim is `play`, elevation 0. `add` = additive:
colour in RGB, intensity in alpha, blend `SRC_ALPHA, ONE`.

| Kind | Blend | Frames, fps | Size (units) | Use |
|---|---|---|---|---|
| `fx.muzzle` | add | 4, 30 | 0.6 (dimmed: at 0.9 and full white it bloomed into big stars) | at the soldier's `muzzle` point on each shot shown (cap about 20 a frame) |
| `fx.tracer` | add | 1 | 0.26 x 1.5 (pivot at the head) | each bullet: a stretched quad along its velocity, cream core, orange tail |
| `fx.hit` | add | 6, 30 | 1.0 | bullet hits anything |
| `fx.poof_red`, `fx.poof_blue` | alpha | 8, 20 | 1.6 | an enemy / a soldier dies: a puff with bits in its colour; the figure itself scales to 0.6 and fades in 0.12 s (no tip-over) |
| `fx.debris` | alpha | 8, 16 | 0.5 | tumbling plank; thrown by barrels and barricades |
| `fx.explosion` + `fx.explosion_glow` | alpha + add | 14, 24 | 4.5 | barrel burst |
| `fx.bigboom` + `fx.bigboom_glow` | alpha + add | 16, 20 | 11 | boss death (3 of them over 0.6 s, then the big one), with shake and a 0.15 s hit-stop |
| `fx.ring` | add | 8, 24 | 5 (drawn flat on the ground: squash y by sin(elevation)) | barrel burst, boss stomp, boss death |
| `fx.dust` | alpha | 6, 12 | 0.9 | march dust: under the squad's back row and under riders, 10-20 a second; boss footfalls (scaled 3x) |
| `fx.sparkle` | add | 6, 20 | 0.6 | recruits joining, gate growth, stars |
| `fx.gatepass_add` / `_mul` / `_neg` | add | 10, 24 | 4 x 3 | gate passed |
| `fx.confetti_a` .. `_f` | alpha | 8, 16 | 0.3 | six colours of tumbling paper: gate pass, victory (200 pieces, ballistic with flutter) |
| `wN.<ambient>` (table in section 8) | alpha, or add for firefly, ember, lantern | 8, 10 | 0.2-1.2 | ambient drift across the screen, upper-left to lower-right, 6 a second |

Boss hits: `fx.hit` at 2x plus the white flash; every 10% of health lost, an `fx.explosion` at 0.6x on the body and a
0.1 s shake. Boss stomp: on frames 0 and 4 of `boss.march` (each footfall) one `fx.ring` at 0.5x and 3 `fx.dust` at 3x.

## 10. Post effects

In order, on a half-resolution chain where possible:

1. **Bloom**: threshold 0.82 (luma, sRGB), soft knee 0.1, 4 mips down / up, strength 0.55 (world 9: threshold 0.7,
   strength 0.8). Only effects, gate edges, plumes and highlights cross the threshold in daylight worlds.
2. **Grade**: multiply by the world's `light` colour at its strength (`mix(1, colour, strength)`), then contrast 1.06
   around 0.5 and saturation 1.12.
3. **Vignette**: darken corners to 0.84 with a smoothstep from radius 0.55 to 1.05 (aspect-corrected); none under the
   HUD's top row.
4. Optional (high tier only): depth tilt-shift, a blur of up to 2 px on the top 18% of the screen. It sells the
   miniature diorama, but skip it if it costs a pass.

## 11. HUD and screens

See section 15.

## 12. Asset contract notes (as built)

- Manifest `assets/art/sprites.txt` as agreed, with atlas ids as numbers and `atlas <id> <file> [world]`: an atlas
  with a world number holds only that world's kinds (`wN.*` in `atlas_props_wN.png`). `atlas_boss.png` (kind `boss`),
  `atlas_units.png` and `atlas_fx.png` are global. Kinds may contain dots (`gate.post_add`, `w1.tree_a`): parse the last
  three dot-separated fields of a sprite name as anim, elevation, frame.
- `height` is the rect's height in road units; frames are trimmed per frame, so w, h, pivot and height differ from
  frame to frame of one anim (use each frame's own values).
- Points: `point soldier.march.<elev> muzzle x y` is in the same pixel space as that anim's frame 0 **relative to the
  pivot** being at (pivotX, pivotY) of frame 0: add `(x - pivotX, y - pivotY)` scaled to the screen to the figure's
  ground point. (The muzzle bobs by under 2 px over the cycle; frame 0 is used for all frames.)
- Extra `worlds.txt` fields:
  - `backdrop <file> <height>`: an image whose bottom edge sits on the horizon, as wide as the screen, `height` =
    its height as a share of the screen's width; scroll it horizontally by -0.02 x squad x (parallax); drawn after
    the sky, before the ground, under the haze colour at 35%.
  - `shadow <dx> <dz> <alpha>`: cast-shadow offset per unit of the thing's height, and the shadow's opacity.
  - `grade <contrast> <saturation>` and `vignette <strength>`.
  - `boss <hueDegrees> <0xRRGGBB>` and `bossaddon <kind>`: the add-on is an ordinary trimmed sprite with its own rect,
    pivot and height per frame; draw it at the boss's ground point with the boss's scale, elevation and frame.

## 13. Engine requests, in order of visual payoff

1. **Sprite batch + textured ground + haze** (the contract): instanced quads sorted far to near, elevation variants,
   road and land textures with mipmaps and anisotropy if available (the road is seen at 10 degrees in the distance;
   without at least trilinear filtering it shimmers), haze to the world's colour.
2. **Cast shadows**: for each thing, a soft ellipse on the ground, centre offset from the pivot by `(dx, dz) * height
   * 0.5`, half-length `0.5 * height`, half-width = the footprint radius, along the direction `(dx, dz)`; one union
   layer as today (so overlaps do not darken twice), colour `#1B2A55` at the world's shadow alpha (0.35). Trees and
   the boss need it most.
3. **Post pass**: bloom, grade, vignette (section 10).
4. **Gate panel shader** and the gate's passed / hit animations (section 6).
5. **Per-instance variation**: scale, multiply tint, animation phase, and a per-instance `flash` add (white).
6. **Additive effects and tracers** (section 9), muzzle flashes at the `muzzle` point.
7. **Spawn and death motion**: recruits pop in with overshoot; the dead shrink and fade into `fx.poof_*`; knock-back
   of 0.3 units on the hit that kills.
8. **Backdrop strip and sky** (`backdrop`, `sky`).
9. **Ambient particles** (`ambient`) and march dust.
10. **Camera feel**: shake (as today), a 0.15 s hit-stop and a slight zoom-out (3%) on the boss's death; the camera
    eases 2% closer while the squad is under 10 soldiers.
11. **Squad banner** (section 11) in the world instead of the floating badge.

## 14. Sizes and budget

Limit: about 28 MB for `assets/art`. As built, all ten worlds and the HUD: **16.4 MB**.

| Files | Size |
|---|---|
| `atlas_boss.png` (2048x2048, the shared ogre, 16 frames) | 3.2 MB |
| `atlas_units.png` (2048x2048: soldier march and cheer, squad banner, foot, rider, brute, barrel, gate parts; 159 frames) | 2.9 MB |
| `atlas_fx.png` (2048x2048, 193 frames) | 0.9 MB |
| `atlas_props_w1..10.png` (2048 wide, 512 to 2048 tall, 256 colours with alpha) | 0.06-0.26 MB each, 1.2 MB |
| `ground_wN_road.png` (1024x1024, 256 colours) | 0.5-0.7 MB each, 5.9 MB |
| `ground_wN_land.png` (512x512, 256 colours) | 0.2 MB each, 2.1 MB |
| `backdrop_wN.png` (1024x256 RGBA) | 0.03 MB each, 0.3 MB |
| `atlas_ui.png` (2048x512, HUD sprites) | 0.4 MB |

Room left (11 MB) if something looks soft: props at a higher resolution or in full colour, or 1024 land textures.

The sun is baked into every sprite: nothing may be mirrored.

## 15. HUD, screens, icon (stage 3)

Sizes are in dp. Text stays Roboto Black. The look: cream toy-box cards with a navy outline and a hard drop shadow,
chunky buttons with a lip that sinks when pressed, cobalt ribbons for titles, the game's own toys as icons.

### 15.1 UI sprites (`atlas_ui.png`, global; `height` in dp; placed by their centre)

| Kind | Height | Use |
|---|---|---|
| `ui.star_full`, `ui.star_empty` | 42.7 | stars everywhere; scale to the size given per place |
| `ui.shako` | 41.7 | the squad's marker on the progress road: the soldier himself, facing us |
| `ui.boss_head` | 40.7 | the ogre's head; draw it with the world's `boss` hue and tint |
| `ui.skull` | 36.7 | HARD levels |
| `ui.hand` | 62.7 | the drag hint; point `tip` = the fingertip |
| `ui.trophy` | 122 | the end screen |
| `ui.logo` | 138 (288 wide) | the title: PAPER in cobalt over MARCH in gold, drawn flat by `make_fx.py` (white keyline, thick ink outline and lip). Draw it 288 wide, or the screen's width less 48 if that is smaller |
| `ui.ribbon_l/_m/_r`, `ui.ribbon_red_*`, `ui.ribbon_gold_*` | 53 / 41 / 53 | title ribbons: the pivot of each part is its joint (the caps' inner edge, the middle's centre) at the band's mid height. Stretch `_m` across the text width + 24, put `_l` and `_r` at its ends. The band is 40 tall; the tails hang 12 lower |
| `ui.rays.play` | 252 | a white sunburst (alpha): tint it, spin it 10 degrees a second, behind stars, the logo and the trophy |

Order: `ui.rays` under panels; every other UI sprite over panels and under text.

### 15.2 Colours and code-drawn parts

| Name | Value |
|---|---|
| Ink (outlines, text on light) | `#16224F` |
| Cream (cards, chips) | `#FFF6E0`, shade `#E9DCC2` |
| Cobalt | `#2F6BFF`, light `#8AB4FF`, dark `#1440C4` |
| Gold | `#FFC83A`, light `#FFE07A`, dark `#D98A00` |
| Red | `#E3262E`, light `#FF7A86`, dark `#A8141F` |
| Scrim behind cards | `#16224F` at 45% |
| Drop shadow | `#16224F` at 40%, offset (0, 6), no blur (a hard toy shadow) |

**Card**: fill cream, outline 2.5 ink, radius 22, the drop shadow. Its title sits on a ribbon whose band centre is on
the card's top edge; title text white, 24, with a 2 ink outline.

**Buttons** (height 56, radius 16, outline 2.5 ink, label 22; a "lip" of the dark colour shows 6 below the face):

| Kind | Face | Lip | Label | Hover (pointer) | Pressed |
|---|---|---|---|---|---|
| Primary (NEXT, RESUME, RETRY, UPDATE) | gold, the top 45% gold light | gold dark | ink | (not drawn on phones) | face moves down 4 (lip 2), face `#E8A81E`, outline 3 white |
| Secondary (RESTART, LEVELS, SETTINGS, pager) | cream | `#C9B893` | ink | face white, outline 3 cobalt | down 4, face cobalt, label white, outline 3 white |
| Danger (hold to reset) | red | red dark | white | face red light | down 4, face red dark, outline 3 white; the hold fills left to right in gold |
| Round (pause), 48 | cream | `#C9B893` | ink bars | as secondary | as secondary |

The pressed state inverts the secondary button (cream to cobalt) and puts a white outline on every kind, so it shows
on any background. Disabled: face `#D8CFBC`, label `#9A927F`, no lip.

**Toggle** (settings): a 56 x 30 pill, outline 2.5 ink; on = cobalt with a white knob (24) at the right, off = `#B8B0A0`
with the knob at the left; the knob slides in 0.1 s; pressed = outline 3 white.

### 15.3 HUD

- **Level chip** (top left, 48 tall for the touch target, radius 14): cream, outline 2.5 ink, `LEVEL N` in ink at 18.
  **HARD level**: the chip is red with white text and `ui.skull` (26) inside its left end; it pulses to 1.05x once a second
  for the first 3 s. The hint line reads `HARD LEVEL! EVERY SOLDIER COUNTS` on a red pill, and the progress road's fill is
  red instead of cobalt.
- **Stars**: three `ui.star_full` / `ui.star_empty` at 26, 2 apart, right of the chip.
- **Pause**: the round button, top right.
- **Progress road**: a track 14 tall, radius 7, ink at 60%, outline 1.5 white at 35%; the marched part cobalt with a 3
  tall cobalt-light line along its top; `ui.shako` (40) centred on the fill's end, bobbing 1.5 at the march's rhythm;
  at the far end `ui.star_full` (26) or, on a boss level, `ui.boss_head` (34).
- **Boss bar** (replaces the road when the ogre is in sight): a track 26 tall, radius 13, ink; fill red with a red-light
  line along the top; a gold strip shows the health just lost and shrinks after 0.4 s; no flash on hits (the boss is hit
  every frame); `ui.boss_head` (48) over the left end; the name centred, white at 16 with a 1.5 ink outline.
- **Hint line**: a pill, ink at 88%, outline 1.5 white at 25%, text white at 15 (red pill on HARD levels).
- **Squad banner**: the world sprite `squad.banner.march` planted at the squad's centre (sorted like a soldier at the
  squad's z + 0.2), the count at its `text` point: white with a 0.05 unit ink outline, 0.62 units tall, scaled down to fit
  1.7 units; it pops to 1.25x for 0.12 s on every change. It replaces the floating badge. Hide it when the squad is lost.
- **Drag hint**: `ui.hand` (64) under the squad, swinging 60 left and right in 1.6 s, `DRAG TO MARCH` under it in white
  at 20 with a 2 ink outline.
- Floating numbers (+N, rewards): white with a 2 ink outline for gains, red-light for losses; they rise 1.5 units in 0.6 s.

### 15.4 Screens

- **Title** (cold start, over level 1): `ui.rays` (300, tinted gold light at 70%) spinning behind `ui.logo` (288 wide)
  centred at max(top + 250, 32% of the screen's height), the logo bobbing 3 in 2 s; under it the tagline on a red pill (white, 14); `TAP TO
  MARCH` blinking under the squad, white at 22 with a 2 ink outline (it must read on the pale road); the UPDATE pill is a small primary button (height 40).
- Every card is as tall as its content plus 20 of padding: no empty band under the last button.
- **Victory**: scrim, a card with a cobalt ribbon `VICTORY!` (a gold ribbon and `PERFECT!` with three stars). Three
  `ui.star_empty` slots at 64; each earned star slams in (scale 2.2 to 1 in 0.18 s, ease-out) with `ui.rays` (120, gold)
  behind it and one `fx.sparkle`, 0.25 s apart. `SOLDIERS LEFT N` in ink at 18. NEXT (primary; FINISH on level 300);
  RETRY (secondary) under three stars. On the field: the survivors play `soldier.cheer`, confetti falls.
- **Defeated**: a card with the red ribbon `DEFEATED`; `YOU MARCHED N% OF THE ROAD` with a small progress road under it;
  RETRY (primary), LEVELS (secondary).
- **Pause**: a card with the cobalt ribbon `PAUSED`; RESUME (primary), RESTART, LEVELS, SETTINGS (secondary).
- **Settings**: the cobalt ribbon `SETTINGS`; rows of label (ink, 18) and toggle: SOUND, MUSIC, HAPTICS, REDUCE EFFECTS;
  the danger button; version and credits in ink at 60%, 12; BACK (secondary).
- **Levels**: the cobalt ribbon carries the world's name (`WORLD 3 - FROSTVALE`). Cells up to 64 x 64, 10 apart, radius 12, outline 2.5
  ink, the number at 20, three stars at 14 under it: played = cream; the next level to play = gold face (and it bobs);
  not played yet = cream shade with the number at 50%. A HARD level has a red band across its top 12 and `ui.skull` (14)
  in the top right corner; a boss level has `ui.boss_head` (24, the world's tint) there instead, overhanging the cell's
  corner by 6. Pressed cell = cobalt
  with a white number and outline. -100, PREV, NEXT, +100 and BACK are secondary buttons.
- **End**: `ui.rays` (320, gold) and `ui.trophy` (160) over the gold ribbon `ALL 300 LEVELS CONQUERED!` (text at 18),
  the total of stars with a `ui.star_full` (26), a primary button, confetti, the squad cheering.

### 15.5 App icon

`tools/art/make_icon.py`: the squad's soldier, facing us (rendered as `icon.bust`), on a cobalt sunburst with a warm glow
behind the head. Adaptive layers: the head and hat sit inside the 66 dp safe circle and the body runs off the bottom of
the 108 dp layer, so every launcher mask cuts it cleanly. Legacy icons are the middle 72 dp in a rounded square; the web
icons are the same picture, square.

## 16. Content pass assets (design: `docs/CONTENT_PASS.md`)

Built in the design's section-6 order. Steps 1 and 2 (weapons 1-4, pickups, power-ups 1-5) as built:

**Weapons on the squad.** `soldier.march` and `soldier.cheer` no longer hold a gun. The gun is an overlay with the same
frames and elevations, rendered with the soldier as a hold-out (his hat and arm cut into it): draw it straight after
its soldier, same ground point, scale, frame. Each has a `muzzle` point per elevation, relative to its own frame 0
pivot as for every sprite; `wpn.twin` also has `muzzle2` (the left rifle).

| Kind | Look |
|---|---|
| `wpn.musket.march` | the old musket and bayonet |
| `wpn.twin.march` | a short cyan-steel rifle in each hand, gold muzzle rings |
| `wpn.scatter.march` | a brass blunderbuss with a bell mouth |
| `wpn.lance.march` | a long white tube with three cobalt rings and a glowing blue tip |
| `wpn.rocket.march` | an olive tube on the right shoulder, the rocket's red nose showing |

**Carriers** (atlas `atlas_items.png`, global, with the weapon overlays):

| Kind | Size | Notes |
|---|---|---|
| `gate.post_wpn.still` | as the other posts | orange and cream stripes, gold ball, orange pennant |
| `crate.wpn.still` | 1.4 wide, 1.25 tall | orange iron bands; a cream plate on the front and the lid: draw `ui.wpn_<name>` over the crate, centred 1.5 units above its pivot, 0.9 units tall, bobbing |
| `crate.supply.still` | the same | gold bands and gold stars; draw `ui.pow_<name>` over it the same way |
| `pickup.medal.spin` | 1.2 across, hovering 1.0 up | 8 frames at 10 fps = half a turn, both faces alike (loop it). Draw `ui.pow_<name>` over its centre (1.0 above the pivot), 0.7 units tall, its width scaled by abs(cos(pi x frame / 8)) so it turns with the disc |

**Weapon gate panel** (the panel shader's colours): body `#FFF6E0`, frame and chevrons `#FF8A1E`, highlight `#FFD9A8`; no
chevron scroll, a slow sheen only; `ui.wpn_<name>` centred at 1.3 units tall in place of the number, the weapon's name
under it in ink at 0.3 units if it fits.

**Icons** (`atlas_ui.png`, heights in dp after trimming; each is a coloured glyph with an ink outline and a white
keyline, so it reads on cream panels, wood and the road): `ui.wpn_musket`, `_twin`, `_scatter`, `_lance`, `_rocket`,
`_flame`, `_coil`, `_mortar`, `_frost` (drawn on a 40 dp square) and `ui.pow_rapid`, `_power`, `_shield`, `_freeze`,
`_airstrike`, `_magnet`, `_giants`, `_reinforce`, `_quick`, `_hourglass`, `_double`, `_fieldgun` (36 dp square). All
nine weapon icons and all twelve power-up icons exist already.

**Effects** (all anim `play`, elevation 0):

| Kind | Blend | Frames, fps | Size (units) | Use |
|---|---|---|---|---|
| `fx.tracer_twin` | add | 1 | 0.2 x 1.3 | twin rifles' bullets (cyan) |
| `fx.hit_cyan` | add | 6, 30 | 1.0 | their hits |
| `fx.pellet` | add | 1 | 0.24 x 0.55 | scattergun pellets |
| `fx.muzzle_wide` | add | 4, 30 | 1.1 x 0.7 | scattergun muzzle, with one `fx.puff_small` |
| `fx.puff_small` | alpha | 6, 16 | 0.5 | small smoke and dust hits |
| `fx.bolt` | add | 1 | 0.3 x 2.6 | the lance's bolt |
| `fx.muzzle_ring` | add | 4, 30 | 0.8 | the lance's muzzle |
| `fx.pierce` | add | 4, 30 | 0.8 x 1.2 | on each enemy the bolt passes through |
| `fx.rocket` | alpha | 8, 16 loop | 0.5 x 1.0 | the rocket, nose up: rotate along its flight; drop one `fx.smoke_trail` behind it every 0.05 s |
| `fx.smoke_trail` | alpha | 6, 16 | 0.8 | rocket smoke |
| `fx.crate_break` | alpha | 8, 20 | 2.6 | any crate opening |
| `fx.light_column` | add | 8, 10 loop | 1.5 x 4.5, pivot at its foot | over a medallion |
| `fx.pickup` | add | 8, 24 | 3.0 | a medallion or supply crate taken |
| `fx.tracer_hot` | add | 1 | 0.3 x 1.7 | bullets under RAPID FIRE |
| `fx.gun_glow` | add | 1 | 0.6 | at each shown muzzle under POWER SHOT, pulsing 0.6-1.0 |
| `fx.shield_dome` | add | 8, 10 loop | 2.2 x 1.9 for a footprint radius of 1: scale by the squad's radius; pivot at the base centre | SHIELD |
| `fx.shield_crack` | add | 4, 24 | 1.4 | where the shield absorbs a hit |
| `fx.shield_break` | add | 8, 24 | 3.2, scaled like the dome | the shield spent |
| `fx.frost_ring` | add | 8, 24 | 5, flat on the ground like `fx.ring`; scale it to sweep 45 units | FREEZE |
| `fx.frost_overlay` | alpha | 1 | 0.9 x 0.5, pivot at its foot | at the feet of each frozen enemy, with a tint of `#9CD4FF` at 60% on the enemy |
| `fx.bomb` | alpha | 1 | 0.7 x 1.0 | AIRSTRIKE: falls from 12 units up in 0.5 s, nose down |
| `fx.shadow_grow` | alpha | 1 | 2.0 x 1.0 at full size | under a falling bomb, growing from 0.3x to 1x (later: boulders, shells) |

The explosion of a rocket is `fx.explosion` at 0.8x with its glow and ring; an airstrike bomb is `fx.bigboom` at 0.5x.
