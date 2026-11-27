# PAPERMARCH: packs the rendered frames into atlases and writes assets/art/sprites.txt (the engine's manifest).
#   python -I -B tools/art/pack_atlas.py captures/art/frames assets/art
# Per frame: scale down by the supersampling factor, add the dark rim (units), trim, pack on shelves, bleed edge colours.
import sys, os, json, glob, math
import numpy as np
from scipy import ndimage
from PIL import Image

SRC, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
PAD = 4          # transparent pixels between frames (the trim keeps 1 more around each)
SIZE = 2048

import re
def atlas_of(kind):
    """A world's own kinds (wN.*, boss_wN) go in that world's two atlases; the engine loads those per world."""
    if kind == 'boss': return 'boss'
    if kind.startswith('ui.'): return 'ui'
    m = re.match(r'w(\d+)\.', kind)
    if m: return 'props_w' + m.group(1)
    return 'fx' if kind.startswith('fx.') else 'units'
def world_of(gid):
    m = re.search(r'_w(\d+)$', gid)   # props_wN: world N only
    return ' ' + m.group(1) if m else ''

# HUD sprites built in 3D: dp per model unit (the manifest's height is in dp for ui.* kinds)
UI_DP = {'ui.star_full': 44, 'ui.star_empty': 44, 'ui.shako': 32, 'ui.boss_head': 14, 'ui.skull': 32, 'ui.hand': 50, 'ui.trophy': 60}

def rimmed(kind):
    return kind in ('soldier', 'foot', 'rider', 'brute', 'barrel', 'squad.banner') or kind.startswith('gate.') or kind == 'boss' or kind in UI_DP

# kind prefix -> fps, loop, blend (everything else: 12 fps, loop, alpha)
ANIMS = [('fx.muzzle', 30, 'once', 'add'), ('fx.tracer', 1, 'loop', 'add'), ('fx.hit', 30, 'once', 'add'), ('fx.poof', 20, 'once', 'alpha'),
         ('fx.debris', 16, 'loop', 'alpha'), ('fx.explosion_glow', 24, 'once', 'add'), ('fx.explosion', 24, 'once', 'alpha'),
         ('fx.bigboom_glow', 20, 'once', 'add'), ('fx.bigboom', 20, 'once', 'alpha'), ('fx.ring', 24, 'once', 'add'),
         ('fx.dust', 12, 'once', 'alpha'), ('fx.sparkle', 20, 'once', 'add'), ('fx.gatepass', 24, 'once', 'add'),
         ('fx.confetti', 16, 'loop', 'alpha'), ('w1.windmill', 8, 'loop', 'alpha'), ('w10.ferris', 6, 'loop', 'alpha')]
ADD_KINDS = {'firefly', 'ember', 'lantern'}       # additive ambient particles (worlds.py style 'glow')
def anim_of(kind):
    for p, fps, loop, blend in ANIMS:
        if kind.startswith(p): return fps, loop, blend
    if re.match(r'w\d+\.', kind) and anim_name.get(kind) == 'play':
        return 10, 'loop', 'add' if kind.split('.')[1] in ADD_KINDS else 'alpha'
    return 12, 'loop', 'alpha'

def load(path, ss, rim):
    """-> float RGBA (straight alpha), scaled down by ss, with the rim."""
    im = np.asarray(Image.open(path).convert('RGBA'), float) / 255.0
    pm = np.dstack([im[..., :3] * im[..., 3:], im[..., 3]])                     # premultiplied for filtering
    if ss > 1:
        h, w = pm.shape[0] // ss * ss, pm.shape[1] // ss * ss
        pm = pm[:h, :w].reshape(h // ss, ss, w // ss, ss, 4).mean(axis=(1, 3))
    if rim:
        pad = np.pad(pm, ((3, 3), (3, 3), (0, 0)))
        blur = np.dstack([ndimage.gaussian_filter(pad[..., c], 1.3) for c in range(4)])
        a = pad[..., 3]
        ra = np.clip(blur[..., 3] * 3.0, 0, 1) * 0.6                             # the rim's coverage, wider than the figure
        rc = blur[..., :3] / np.maximum(blur[..., 3:], 1e-4) * 0.28              # the figure's own colour, darkened
        out_a = a + ra * (1 - a)
        pm = np.dstack([pad[..., :3] + rc * (ra * (1 - a))[..., None], out_a])
        off = 3
    else:
        off = 0
    a = pm[..., 3]
    rgb = pm[..., :3] / np.maximum(a, 1e-5)[..., None]
    return np.dstack([np.clip(rgb, 0, 1), a]), off

anim_name = {}
frames = []   # dicts: name, kind, anim, elev, frame, img, pivot, ppu
points = []
anims = {}
for meta_path in sorted(glob.glob(os.path.join(SRC, '*.json'))):
    base = os.path.basename(meta_path)[:-5]
    kind, anim = base.rsplit('.', 1)
    if kind.startswith('icon.'): continue        # tools/art/make_icon.py's input, not a sprite
    meta = json.load(open(meta_path))
    ss, ppu = meta['ss'], meta['ppu'] / meta['ss'] / UI_DP.get(kind, 1)
    anim_name[kind] = anim
    anims[(kind, anim)] = anim_of(kind)
    for elev, em in meta['elevs'].items():
        first = None
        for f in range(meta['frames']):
            img, off = load(os.path.join(SRC, '%s.%s.%s.%d.png' % (kind, anim, elev, f)), ss, rimmed(kind))
            ys, xs = np.nonzero(img[..., 3] > 2 / 255)
            if len(xs) == 0:
                ys, xs = np.array([0]), np.array([0])
            x0, x1, y0, y1 = max(xs.min() - 1, 0), min(xs.max() + 2, img.shape[1]), max(ys.min() - 1, 0), min(ys.max() + 2, img.shape[0])
            px, py = em['pivot'][0] / ss + off - x0, em['pivot'][1] / ss + off - y0
            if kind.startswith('ui.') and not kind.startswith('ui.ribbon'):
                px, py = (x1 - x0) / 2, (y1 - y0) / 2          # HUD sprites are placed by their centre (the ribbon parts by their joint)
            fr = dict(kind=kind, anim=anim, elev=0 if kind.startswith('ui.') else int(elev), frame=f, img=img[y0:y1, x0:x1], pivot=(px, py), ppu=ppu)
            frames.append(fr)
            if f == 0:
                for name, p in em['points'].items():
                    if kind.startswith('ui.') and name == 'muzzle': continue
                    points.append((kind, anim, 0 if kind.startswith('ui.') else int(elev), name, p[0] / ss + off - x0, p[1] / ss + off - y0))

# ---- shelf packing, one atlas per group --------------------------------------------------------------------------------
groups = {}
for fr in frames:
    groups.setdefault(atlas_of(fr['kind']), []).append(fr)
lines = ['# PAPERMARCH sprites: written by tools/art/pack_atlas.py, do not edit by hand.',
         '# atlas <id> <file> [world] | anim <kind>.<anim> <fps> <loop|once> <alpha|add> | sprite <kind>.<anim>.<elev>.<frame> <atlas> <x> <y> <w> <h> <pivotX> <pivotY> <height>',
         '# point <kind>.<anim>.<elev> <name> <x> <y>  (pixels in frame 0 of that elevation, same origin as its pivot)']
sprite_lines = []
for num, (gid, frs) in enumerate(sorted(groups.items())):
    frs.sort(key=lambda fr: (-fr['img'].shape[0], fr['kind'], fr['anim'], fr['elev'], fr['frame']))
    x = y = PAD; shelf = 0
    for fr in frs:
        h, w = fr['img'].shape[:2]
        if x + w + PAD > SIZE:
            x, y, shelf = PAD, y + shelf + PAD, 0
        fr['x'], fr['y'] = x, y
        x += w + PAD; shelf = max(shelf, h)
    used = y + shelf + PAD
    H = 1 << max(6, math.ceil(math.log2(used)))
    if H > SIZE:
        sys.exit('atlas %s overflows: %d rows used' % (gid, used))
    at = np.zeros((H, SIZE, 4))
    for fr in frs:
        h, w = fr['img'].shape[:2]
        at[fr['y']:fr['y'] + h, fr['x']:fr['x'] + w] = fr['img']
        sprite_lines.append(((fr['kind'], fr['anim'], -fr['elev'], fr['frame']), 'sprite %s.%s.%d.%d %s %d %d %d %d %.1f %.1f %.4f' % (
            fr['kind'], fr['anim'], fr['elev'], fr['frame'], num, fr['x'], fr['y'], w, h, fr['pivot'][0], fr['pivot'][1], h / fr['ppu'])))
    # bleed: transparent pixels near a frame take the nearest opaque colour, so mipmaps and bilinear taps do not fringe
    opaque = at[..., 3] > 1 / 255
    dist, (iy, ix) = ndimage.distance_transform_edt(~opaque, return_indices=True)
    near = (~opaque) & (dist <= 6)
    at[near, :3] = at[iy[near], ix[near], :3]
    at[(~opaque) & ~near, :3] = 0
    name = 'atlas_%s.png' % gid
    im = Image.fromarray((np.clip(at, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA')
    if world_of(gid):      # a world's props: 256 colours with alpha, about an eighth of the bytes
        im = im.quantize(256, method=Image.FASTOCTREE)
    im.save(os.path.join(OUT, name), optimize=True)
    lines.append('atlas %d %s%s' % (num, name, world_of(gid)))
    print('%s: %d frames, %dx%d, %.0f%% of the rows used, %.2f MB' % (name, len(frs), SIZE, H, 100 * used / H, os.path.getsize(os.path.join(OUT, name)) / 1e6))
for (kind, anim), (fps, loop, blend) in sorted(anims.items()):
    lines.append('anim %s.%s %d %s %s' % (kind, anim, fps, loop, blend))
lines += [l for _, l in sorted(sprite_lines)]
for kind, anim, elev, name, px, py in points:
    lines.append('point %s.%s.%d %s %.1f %.1f' % (kind, anim, elev, name, px, py))
with open(os.path.join(OUT, 'sprites.txt'), 'w', newline='\n') as fh:
    fh.write('\n'.join(lines) + '\n')
print('sprites.txt: %d sprites' % len(sprite_lines))
