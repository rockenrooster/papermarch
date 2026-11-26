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
| Squad pack | `#C9955A` tan leather, bedroll `#F2E9D0` | seen from behind, which is how the squad is always seen |
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
| `boss_w1` (per world: `boss_wN`) | 4.6 | 3.4 wide | The ogre: moss-green, huge pale belly, small head sunk between spiked iron pauldrons, two long ivory horns, under-bite tusks, glowing yellow eyes, crimson loincloth with a gold buckle, a tree-trunk club with iron bands in the right hand. Stomping walk. Nothing else on the road is green or that big. |

Boss variants per world (stage 2, same model re-rendered with other materials and one add-on prop): OGRE CHIEF (moss
green, bone necklace), SAND OGRE (ochre, turban wrap), FROST OGRE (ice blue, fur mantle, icicle club), EMBER OGRE
(charcoal with glowing orange cracks), SALT OGRE (pale grey-pink, crystal shards), THORN OGRE (dark green, thorn
vines, flower crown), BOG OGRE (olive and mud, mushroom shoulders), CLOUD OGRE (white-violet, gold armour), ASH OGRE
(black, embers, red eyes), LANTERN OGRE (plum, carrying paper lanterns).

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

Sprite elevations rendered: **60, 38, 20** degrees for units, barrel and gate parts (the boss: 38 and 20 only; it never
gets nearer than about 11 units from the camera's foot). Props: 45 and 20. Effects and ambient: 0.
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

`barrel` (1.3 tall, 1.0 wide): a fat golden keg with cobalt iron bands, gold rivets and a white star on the front and
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
- Haze: distance fog to `#BFE0F2` from 60 to 150 units ahead of the camera, so the horizon melts into the sky.
- Sky: `#4FA8F0` at the top, `#BFE0F2` at the horizon; backdrop strip `backdrop_w1.png` (far hills, a castle and
  clouds) standing on the horizon.

## 8. Props and the ten worlds

World 1 (stage 1) kinds: `w1.tree_a` (round oak), `w1.tree_b` (tall poplar), `w1.tree_c` (pink blossom tree),
`w1.bush`, `w1.rock`, `w1.flowers`, `w1.haystack`, `w1.cottage`, `w1.windmill` (8 frames: the sails turn); edge props `w1.bollard` (a striped stone post
on the curb every 3 units) and `w1.banner` (a pole with a cobalt banner and a white star every 12 units); ambient `w1.leaf` and `w1.petal`.

Density: `prop` lines in `worlds.txt` (weights and spacing); about 60 props on screen. Big landmarks (cottage,
windmill) are rare and far from the road (x 12-22).

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
| `fx.muzzle` | add | 4, 30 | 0.9 | at the soldier's `muzzle` point on each shot shown (cap about 20 a frame) |
| `fx.tracer` | add | 1 | 0.26 x 1.5 (pivot at the head) | each bullet: a stretched quad along its velocity, cream core, orange tail |
| `fx.hit` | add | 6, 30 | 1.0 | bullet hits anything |
| `fx.poof_red`, `fx.poof_blue` | alpha | 8, 20 | 1.6 | an enemy / a soldier dies: a puff with bits in its colour; the figure itself scales to 0.6 and fades in 0.12 s (no tip-over) |
| `fx.debris` | alpha | 8, 16 | 0.5 | tumbling plank; thrown by barrels and barricades |
| `fx.explosion` + `fx.explosion_glow` | alpha + add | 14, 24 | 4.5 | barrel burst |
| `fx.bigboom` + `fx.bigboom_glow` | alpha + add | 16, 20 | 11 | boss death (3 of them over 0.6 s, then the big one), with shake and a 0.15 s hit-stop |
| `fx.ring` | add | 8, 24 | 5 (drawn flat on the ground: squash y by sin(elevation)) | barrel burst, boss stomp, boss death |
| `fx.dust` | alpha | 6, 12 | 0.9 | march dust: under the squad's back row and under riders, 10-20 a second; boss footfalls (scaled 3x) |
| `fx.sparkle` | add | 6, 20 | 0.8 | recruits joining, gate growth, stars |
| `fx.gatepass_add` / `_mul` / `_neg` | add | 10, 24 | 4 x 3 | gate passed |
| `fx.confetti_a` .. `_f` | alpha | 8, 16 | 0.3 | six colours of tumbling paper: gate pass, victory (200 pieces, ballistic with flutter) |
| `w1.leaf`, `w1.petal` | alpha | 8, 10 | 0.35 | ambient drift across the screen, upper-left to lower-right, 6 a second |

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

## 11. HUD and screens (stage 3, notes only)

- Keep the cream cards but give them a 2 px ink outline, a hard drop shadow and a cobalt header ribbon; numbers in the
  existing font with a dark outline.
- The squad badge: a cobalt pennant flag on a pole carried in the middle of the squad (a sprite in the world, drawn
  above the squad), the number in white on it; it pops on every change.
- Progress bar: a miniature road with the squad's shako icon marching along it and the boss's head at the end.
- Victory: confetti, three gold stars slamming in with `fx.sparkle`, the squad cheering (hats in the air).
- App icon: a soldier's head in three-quarter view with the plume, on a sunburst of cobalt and gold.

## 12. Asset contract notes (as built)

- Manifest `assets/art/sprites.txt` as agreed, with atlas ids as numbers and `atlas <id> <file> [world]`: an atlas
  with a world number holds only that world's kinds (`wN.*` in `atlas_props_wN.png`, `boss_wN` in `atlas_boss_wN.png`). Kinds may contain dots (`gate.post_add`, `w1.tree_a`): parse the last
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

Stage 1 as built (8.7 MB): `atlas_units.png` 2048x1024, 1.85 MB (soldier, foot, rider, brute, barrel, gate parts: 111
frames); `atlas_fx.png` 2048x2048, 0.88 MB (193 frames); `atlas_props_w1.png` 2048x2048, 1.84 MB (53 frames, world
1); `atlas_boss_w1.png` 2048x2048, 2.97 MB (16 frames, world 1); the road 0.59 MB and the land 0.85 MB (1024x1024,
256-colour PNG); the backdrop 0.03 MB.

A world costs about 6.3 MB at this quality (props 1.8, boss 3.0, ground 1.4), so ten worlds would be about 65 MB, not
15. Ways down, to be decided before stage 2 (open question for the owner and the engineer):

1. One boss model for all worlds, recoloured by the engine (a hue rotation and a multiply tint per world in
   `worlds.txt`) plus a small per-world add-on sprite: saves 27 MB. Recommended.
2. Props atlases at 1024x2048 with fewer elevations for small props (one instead of two): about 1.0 MB a world.
3. Ground at 512x512 for the land (it is soft anyway): about 0.3 MB instead of 0.85.
4. With 1-3: 8.7 + 9 x (1.0 + 0.9 + 0.05) = about 26 MB. To reach 15 MB the props would also have to be 256-colour
   PNGs (visible banding on the smooth toy shading) or lossy WebP (about a quarter of the PNG size, needs a decoder in
   the engine: libwebp, BSD). WebP for every atlas would put all ten worlds near 9 MB.

Props: the sun is baked in, so mirroring a prop flips its light. It does not show on trees, bushes and rocks; do not
mirror `w1.cottage`, `w1.windmill` and `w1.banner`.
