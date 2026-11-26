# tools/art: how PAPERMARCH's pictures are made

Everything in `assets/art/` is generated; nothing is drawn by hand or downloaded. Rebuild from the repository root
(PowerShell 7; Blender 5.2, Python with PIL, numpy and scipy; Blender on 2 CPU threads, no windows):

```
$frames = 'captures\art\frames'
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' -b --factory-startup --python-exit-code 1 -P tools\art\render_sprites.py -- --out $frames
python -I -B tools\art\make_fx.py $frames
python -I -B tools\art\pack_atlas.py $frames assets\art
python -I -B tools\art\make_ground.py assets\art
python -I -B tools\art\compose.py target captures\art\target_battle.png
python -I -B tools\art\compose.py preview captures\art\preview_w1.png
```

| Script | Makes |
|---|---|
| `render_sprites.py` (Blender, Cycles on the CPU, 2 threads, about 45 minutes for everything) | every 3D thing, built from primitives in the script: the five units' march cycles, the barrel, gate posts and stakes, world 1's props. `--only soldier,foot,w1` renders some kinds. Frames at 2x, a `.json` per animation (pivot, pixels per unit, named points) |
| `make_fx.py` (numpy) | the effect flipbooks and ambient particles, same frame and `.json` format |
| `pack_atlas.py` | scales the frames down, bakes the dark rim round units, trims, packs `atlas_*.png` (2048 wide), bleeds edge colours, writes `sprites.txt` |
| `make_ground.py` (numpy, scipy) | `ground_w1_road.png`, `ground_w1_land.png`, `backdrop_w1.png` |
| `compose.py` | a still of a battle from the runtime files at the game's camera, drawn the way `docs/ART_DIRECTION.md` asks the engine to draw it: the target the engine is measured against |
| `sheet.py` | a contact sheet of frames, to look at them |

`assets/art/worlds.txt` is written by hand (the field list is at its top).

Conventions: Blender X = the game's x, Blender Y = the game's z (up the road), Z = up. The light is fixed in
`render_sprites.py` (`SUN`, `RIM`) and repeated in `make_ground.py` (`shade`); change both together. Sprite sizes are set
by the `JOBS` table (pixels per road unit); the manifest's `height` follows from it.
