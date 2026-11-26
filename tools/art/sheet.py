# A contact sheet of rendered frames on the road's colour, to judge them by eye.
#   python -I -B tools/art/sheet.py captures/art/frames captures/art/sheet_soldier.png soldier foot [--frames 0,2,4] [--scale 0.5]
import sys, glob, os
from PIL import Image

src, out = sys.argv[1], sys.argv[2]
rest = sys.argv[3:]
def opt(name, default):
    if name in rest:
        i = rest.index(name); v = rest[i + 1]; del rest[i:i + 2]; return v
    return default
frames = set(opt('--frames', '0,2,4,6').split(','))
scale = float(opt('--scale', '0.5'))
ims = []
for kind in rest:
    fs = [f for f in glob.glob(os.path.join(src, kind + '.*.png')) if os.path.basename(f).split('.')[-2] in frames]
    fs.sort(key=lambda f: (-int(f.split('.')[-3]), int(f.split('.')[-2])))
    for f in fs:
        im = Image.open(f).convert('RGBA')
        ims.append(im.resize((max(1, int(im.width * scale)), max(1, int(im.height * scale))), Image.LANCZOS))
W = 1600
x = y = rowh = 0
pos = []
for im in ims:
    if x + im.width > W:
        x, y, rowh = 0, y + rowh, 0
    pos.append((x, y)); x += im.width; rowh = max(rowh, im.height)
sheet = Image.new('RGBA', (W, y + rowh), (0xC6, 0xB4, 0x9A, 255))
for im, p in zip(ims, pos):
    sheet.alpha_composite(im, p)
sheet.convert('RGB').save(out)
