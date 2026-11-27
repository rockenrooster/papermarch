# PAPERMARCH app icon: the squad's soldier (rendered by render_sprites.py as icon.bust) on a cobalt sunburst.
#   python -I -B tools/art/make_icon.py [captures/art/frames]
# Writes the Android adaptive layers (108 dp, the art inside the 66 dp safe circle) and legacy icons in
# android/app/src/main/res/mipmap-*, mipmap-anydpi-v26/ic_launcher.xml, web/icon-180/192/512.png, and
# captures/art/icon_512.png + icon_round.png for review.
import os, sys, math
import numpy as np
from scipy import ndimage
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FRAMES = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'captures', 'art', 'frames')
RES = os.path.join(ROOT, 'android', 'app', 'src', 'main', 'res')
S = 1296  # working size of a 108 dp layer (12 px per dp)

def background():
    y, x = np.mgrid[0:S, 0:S].astype(float)
    dx, dy = x - S / 2, y - S * 0.56
    r = np.hypot(dx, dy) / (S * 0.7)
    a, b = np.array([0x4A, 0x86, 0xFF]) / 255.0, np.array([0x12, 0x3A, 0xB8]) / 255.0
    col = a + (b - a) * np.clip(r, 0, 1)[..., None] ** 1.2
    beam = np.clip(np.cos(np.arctan2(dy, dx) * 9 + 0.35) * 4 + 0.5, 0, 1)              # 18 rays
    col += (beam * 0.10 * np.clip(1.3 - r, 0, 1))[..., None]
    col += (np.exp(-(r / 0.42) ** 2) * 0.22)[..., None] * np.array([1.0, 0.95, 0.7])    # a warm glow behind the head
    return Image.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8), 'RGB')

def foreground():
    bust = Image.open(os.path.join(FRAMES, 'icon.bust.still.8.0.png')).convert('RGBA')
    # the render is 1.3 units square, centred 0.78 above the ground. One unit = 60% of the layer: the head and hat sit in
    # the 66 dp safe circle, the body runs off the bottom of the layer (every launcher mask cuts it there).
    n = int(1.3 * 0.60 * S)
    bust = bust.resize((n, n), Image.LANCZOS)
    ox, oy = (S - n) // 2 + int(0.03 * S), (S - n) // 2 + int(0.14 * 0.60 * S)
    full_layer = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    full_layer.alpha_composite(bust.crop((0, 0, n, min(n, S - oy))), (ox, oy))
    bust, ox, oy = full_layer, 0, 0
    # a dark rim and a soft drop shadow, so the figure holds on any launcher mask
    al = np.zeros((S, S)); al[oy:oy + bust.height, ox:ox + bust.width] = np.asarray(bust.getchannel('A'), float) / 255.0
    rim = np.clip(ndimage.gaussian_filter(al, 7) * 3.2, 0, 1)
    shadow = np.roll(ndimage.gaussian_filter(al, 22), (26, 14), axis=(0, 1)) * 0.45
    under = np.zeros((S, S, 4)); under[..., :3] = np.array([0x0A, 0x1C, 0x5A]) / 255.0
    under[..., 3] = np.clip(np.maximum(rim, shadow), 0, 1)
    layer = Image.fromarray((under * 255).astype(np.uint8), 'RGBA')
    layer.alpha_composite(bust, (ox, oy))
    return layer

bg, fg = background(), foreground()
full = bg.convert('RGBA'); full.alpha_composite(fg)
for name, dp in (('mdpi', 1), ('hdpi', 1.5), ('xhdpi', 2), ('xxhdpi', 3), ('xxxhdpi', 4)):
    d = os.path.join(RES, 'mipmap-' + name); os.makedirs(d, exist_ok=True)
    n = int(108 * dp)
    bg.resize((n, n), Image.LANCZOS).save(os.path.join(d, 'ic_launcher_background.png'), optimize=True)
    fg.resize((n, n), Image.LANCZOS).save(os.path.join(d, 'ic_launcher_foreground.png'), optimize=True)
    # legacy: the middle 72 dp of the layers, in a rounded square
    m = int(48 * dp)
    c = S * 18 // 108
    leg = full.crop((c, c, S - c, S - c)).resize((m * 4, m * 4), Image.LANCZOS)
    mask = Image.new('L', leg.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, leg.width - 1, leg.height - 1), radius=int(leg.width * 0.22), fill=255)
    leg.putalpha(mask)
    leg.resize((m, m), Image.LANCZOS).save(os.path.join(d, 'ic_launcher.png'), optimize=True)
d = os.path.join(RES, 'mipmap-anydpi-v26'); os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'ic_launcher.xml'), 'w', newline='\n').write(
    '<?xml version="1.0" encoding="utf-8"?>\n<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
    '    <background android:drawable="@mipmap/ic_launcher_background" />\n    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n</adaptive-icon>\n')
c = S * 16 // 108
square = full.crop((c, c, S - c, S - c)).convert('RGB')
for n in (180, 192, 512):
    square.resize((n, n), Image.LANCZOS).save(os.path.join(ROOT, 'web', 'icon-%d.png' % n), optimize=True)
os.makedirs(os.path.join(ROOT, 'captures', 'art'), exist_ok=True)
square.resize((512, 512), Image.LANCZOS).save(os.path.join(ROOT, 'captures', 'art', 'icon_512.png'))
rnd = full.crop((S * 18 // 108, S * 18 // 108, S - S * 18 // 108, S - S * 18 // 108)).resize((512, 512), Image.LANCZOS)
mask = Image.new('L', (512, 512), 0); ImageDraw.Draw(mask).ellipse((0, 0, 511, 511), fill=255)
rnd.putalpha(mask); rnd.save(os.path.join(ROOT, 'captures', 'art', 'icon_round.png'))
print('icons written')
