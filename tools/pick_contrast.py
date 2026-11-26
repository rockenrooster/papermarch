"""pick_contrast.py A.png B.png X Y [R] : WCAG contrast between the mean colours of a (2R+1)^2 patch at X,Y in two
pictures (a button's normal and pressed fills, P3 exit check: at least 3 to 1)."""
import sys

from PIL import Image


def lin(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def mean_patch(path, x, y, r):
    im = Image.open(path).convert("RGB")
    px = [im.getpixel((x + dx, y + dy)) for dx in range(-r, r + 1) for dy in range(-r, r + 1)]
    return tuple(sum(p[i] for p in px) / len(px) for i in range(3))


def luminance(rgb):
    r, g, b = (lin(v) for v in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


a, b = sys.argv[1], sys.argv[2]
x, y = int(sys.argv[3]), int(sys.argv[4])
r = int(sys.argv[5]) if len(sys.argv) > 5 else 4
ca, cb = mean_patch(a, x, y, r), mean_patch(b, x, y, r)
la, lb = luminance(ca), luminance(cb)
ratio = (max(la, lb) + 0.05) / (min(la, lb) + 0.05)
print("A #%02X%02X%02X  B #%02X%02X%02X  contrast %.2f:1" % (*map(round, ca), *map(round, cb), ratio))
sys.exit(0 if ratio >= 3.0 else 1)
