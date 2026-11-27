# A sheet of the shared ogre under candidate recolours, with the engine's maths (shaders/sticker.frag: rotate RGB about the
# grey axis, clamp at 0, multiply by the tint).   python -I -B tools/art/boss_tints.py captures/art/boss_tints.png [hue:tint ...]
import sys, os, math
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from worlds import WORLDS
src = np.asarray(Image.open('captures/art/frames/boss.march.38.0.png').convert('RGBA').reduce(3), float) / 255.0
cands = [tuple(int(v, 0) for v in a.split(':')) for a in sys.argv[2:]] or [WORLDS[n]['boss'][:2] for n in sorted(WORLDS)]
h, w = src.shape[:2]
sheet = np.full((h, w * len(cands), 3), 0.72)
for i, (hue, tint) in enumerate(cands):
    a = math.radians(hue % 360)
    ax = np.full(3, 0.57735027)
    rgb = src[..., :3]
    rot = rgb * math.cos(a) + np.cross(np.broadcast_to(ax, rgb.shape), rgb) * math.sin(a) + ax * (rgb @ ax)[..., None] * (1 - math.cos(a))
    rot = np.maximum(rot, 0) * (np.array([(tint >> 16) & 255, (tint >> 8) & 255, tint & 255]) / 255.0)
    al = src[..., 3:]
    sheet[:, i * w:(i + 1) * w] = sheet[:, i * w:(i + 1) * w] * (1 - al) + np.clip(rot, 0, 1) * al
Image.fromarray((sheet * 255).astype(np.uint8)).save(sys.argv[1])
