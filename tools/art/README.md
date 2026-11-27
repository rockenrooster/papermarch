# tools/art: how PAPERMARCH's pictures are made

Everything in `assets/art/` is generated; nothing is drawn by hand or downloaded. Rebuild from the repository root
(PowerShell 7; Blender 5.2, Python with PIL, numpy and scipy; Blender on 2 CPU threads, no windows):

```
. scripts\env.ps1          # pins this shell and everything it starts to the project's 4 cores (the owner's limit)
$frames = 'captures\art\frames'
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' -b --factory-startup --python-exit-code 1 -P tools\art\render_sprites.py -- --out $frames
python -I -B tools\art\make_fx.py $frames
python -I -B tools\art\make_content.py $frames
python -I -B tools\art\pack_atlas.py $frames assets\art
python -I -B tools\art\make_ground.py assets\art
python -I -B tools\art\make_icon.py
```

| Script | Makes |
|---|---|
| `render_sprites.py` (Blender, Cycles on the CPU, 2 threads, about 45 minutes for everything) | every 3D thing, built from primitives in the script: the five units' march cycles, the barrel, gate posts and stakes, world 1's props. `--only soldier,foot,w1` renders some kinds. Frames at 2x, a `.json` per animation (pivot, pixels per unit, named points) |
| `make_fx.py` (numpy) | the effect flipbooks and ambient particles, same frame and `.json` format |
| `fxlib.py` | the canvas and helpers both effect scripts draw with |
| `make_content.py` | the content pass (`docs/CONTENT_PASS.md`): weapon and power-up effects, the weapon and power-up icons |
| `pack_atlas.py` | scales the frames down, bakes the dark rim round units, trims, packs `atlas_*.png` (2048 wide), bleeds edge colours, writes `sprites.txt` |
| `worlds.py` | the ten worlds as data: ground styles and colours, backdrop, haze, sky, grade, bloom, the `prop` lines, ambient particles, the boss's recolour |
| `make_ground.py [dir] [world numbers]` (numpy, scipy) | every world's `ground_wN_road.png`, `ground_wN_land.png`, `backdrop_wN.png`, and `worlds.txt` |
| `compose.py` | a still of a battle from the runtime files at the game's camera, drawn the way `docs/ART_DIRECTION.md` asks the engine to draw it: the target the engine is measured against |
| `make_icon.py` | the app icon from the `icon.bust` render: Android adaptive layers and legacy icons (`android/app/src/main/res/mipmap-*`), `web/icon-*.png` |
| `boss_tints.py` | a sheet of the shared ogre under candidate `boss` hue and tint values, with the shader's maths |
| `sheet.py` | a contact sheet of frames, to look at them |

`assets/art/worlds.txt` is written by `make_ground.py` from `worlds.py`: edit `worlds.py`, not the text file.
`compose.py` draws world 1 only (the stage-1 target picture); for the other worlds look at the real game (`--picture level --level N`).

Conventions: Blender X = the game's x, Blender Y = the game's z (up the road), Z = up. The light is fixed in
`render_sprites.py` (`SUN`, `RIM`) and repeated in `make_ground.py` (`shade`); change both together. Sprite sizes are set
by the `JOBS` table (pixels per road unit); the manifest's `height` follows from it.
