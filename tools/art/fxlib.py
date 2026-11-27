# PAPERMARCH: the canvas and helpers that make_fx.py and make_content.py draw effect flipbooks with (numpy).
# Writes <out>/<kind>.play.0.<frame>.png (2x supersampled) and <kind>.play.json, like render_sprites.py.
# Additive sprites: colour in RGB, intensity in alpha (blend SRC_ALPHA, ONE).
import sys, os, json, math
import numpy as np
from PIL import Image

OUT = sys.argv[1] if len(sys.argv) > 1 else 'captures/art/frames'
os.makedirs(OUT, exist_ok=True)
SS = 2
TAU = 2 * math.pi

class Cv:
    def __init__(s, w, h, add):
        s.w, s.h, s.add = w, h, add
        s.rgb = np.zeros((h, w, 3)); s.a = np.zeros((h, w))
        y, x = np.mgrid[0:h, 0:w]
        s.X, s.Y = x + 0.5, y + 0.5
    def paint(s, cov, col):
        col = np.array(col[:3], float) / 255.0
        c = cov[..., None]
        if s.add:
            s.rgb += c * col
        else:
            s.rgb = s.rgb * (1 - c) + col * c
            s.a = s.a * (1 - cov) + cov
    def disc(s, cx, cy, r, col, alpha=1.0, soft=2.0, sy=1.0):
        if r <= 0: return
        d = np.hypot(s.X - cx, (s.Y - cy) / sy)
        s.paint(np.clip((r - d) / soft + 0.5, 0, 1) * alpha, col)
    def glow(s, cx, cy, r, col, alpha=1.0, sy=1.0, p=2.0):
        if r <= 0: return
        d = np.hypot(s.X - cx, (s.Y - cy) / sy) / r
        s.paint(np.exp(-(d ** p) * 3.0) * alpha, col)
    def ring(s, cx, cy, r, th, col, alpha=1.0):
        d = np.abs(np.hypot(s.X - cx, s.Y - cy) - r)
        s.paint(np.exp(-(d / max(th, 0.5)) ** 2) * alpha, col)
    def rect(s, cx, cy, hw, hh, ang, col, alpha=1.0, soft=1.5):
        c, n = math.cos(ang), math.sin(ang)
        u = (s.X - cx) * c + (s.Y - cy) * n
        v = -(s.X - cx) * n + (s.Y - cy) * c
        s.paint(np.clip((hw - np.abs(u)) / soft + 0.5, 0, 1) * np.clip((hh - np.abs(v)) / soft + 0.5, 0, 1) * alpha, col)
    def star(s, cx, cy, r, col, n=4, k=0.25, p=3.0, rot=0.0, alpha=1.0, soft=2.0, sy=1.0):
        dx, dy = s.X - cx, (s.Y - cy) / sy
        d = np.hypot(dx, dy)
        edge = r * (k + (1 - k) * np.abs(np.cos(n * (np.arctan2(dy, dx) + rot) / 2)) ** p)
        s.paint(np.clip((edge - d) / soft + 0.5, 0, 1) * alpha, col)
    def line(s, x0, y0, x1, y1, th, col, alpha=1.0):
        dx, dy = x1 - x0, y1 - y0
        L2 = dx * dx + dy * dy + 1e-9
        t = np.clip(((s.X - x0) * dx + (s.Y - y0) * dy) / L2, 0, 1)
        d = np.hypot(s.X - (x0 + t * dx), s.Y - (y0 + t * dy))
        s.paint(np.exp(-(d / th) ** 2) * alpha, col)
    def image(s):
        if s.add:
            a = np.clip(s.rgb.max(axis=2), 0, 1)
            rgb = np.clip(s.rgb / np.maximum(s.rgb.max(axis=2), 1e-6)[..., None], 0, 1)
            rgb[a < 1e-4] = 0
        else:
            a = np.clip(s.a, 0, 1)
            rgb = np.clip(s.rgb / np.maximum(a, 1e-6)[..., None], 0, 1)
        return Image.fromarray((np.dstack([rgb, a]) * 255 + 0.5).astype(np.uint8), 'RGBA')

def book(kind, frames, w_units, h_units, ppu, add, draw, pivot=(0.5, 0.5)):
    """draw(cv, t, f): t = f / frames; canvas pixels, origin top-left."""
    s = ppu * SS
    W, H = int(round(w_units * s)), int(round(h_units * s))
    for f in range(frames):
        cv = Cv(W, H, add)
        draw(cv, f / frames, f)
        cv.image().save(os.path.join(OUT, '%s.play.0.%d.png' % (kind, f)))
    with open(os.path.join(OUT, '%s.play.json' % kind), 'w') as fh:
        json.dump({'ppu': s, 'ss': SS, 'frames': frames, 'elevs': {'0': {'pivot': [W * pivot[0], H * pivot[1]], 'points': {}}}}, fh)
    print('drew', kind)

def lerp3(stops, x):
    x = min(max(x, 0.0), 1.0)
    for (x0, c0), (x1, c1) in zip(stops, stops[1:]):
        if x <= x1:
            k = (x - x0) / max(x1 - x0, 1e-6)
            return tuple(c0[i] + (c1[i] - c0[i]) * k for i in range(3))
    return stops[-1][1]

FIRE = [(0.0, (92, 84, 88)), (0.25, (120, 70, 50)), (0.45, (235, 90, 30)), (0.65, (255, 160, 40)), (0.85, (255, 225, 90)), (1.0, (255, 252, 220))]
SMOKE = [(0.0, (196, 186, 170)), (0.5, (236, 228, 212)), (1.0, (255, 252, 244))]

def puffs(cv, t, seed, n, radius, ramp, rise=0.25, cool=1.0):
    """A toon burst: shaded balls flying out, cooling and shrinking away."""
    rnd = np.random.RandomState(seed)
    cx, cy = cv.w / 2, cv.h / 2
    items = []
    for i in range(n):
        ang, spd, size, late = rnd.uniform(0, TAU), rnd.uniform(0.15, 1.0), rnd.uniform(0.5, 1.0), rnd.uniform(0, 0.15)
        tt = (t - late) / (1 - late)
        if tt <= 0: continue
        ease = 1 - (1 - tt) ** 2.5
        x = cx + math.cos(ang) * spd * ease * radius * 0.62
        y = cy + math.sin(ang) * spd * ease * radius * 0.62 - rise * radius * tt * tt
        r = radius * 0.34 * size * (0.55 + 0.75 * ease) * (1.0 if tt < 0.55 else max(0.0, 1 - ((tt - 0.55) / 0.45) ** 1.5))
        heat = 1 - cool * tt * (0.9 + 0.8 * spd)
        items.append((heat, x, y, r))
    for heat, x, y, r in sorted(items):
        c = lerp3(ramp, heat)
        cv.disc(x + r * 0.06, y + r * 0.06, r, tuple(v * 0.72 for v in c))
        cv.disc(x - r * 0.07, y - r * 0.07, r * 0.88, c)
        cv.disc(x - r * 0.3, y - r * 0.32, r * 0.4, tuple(min(255, v * 1.12 + 14) for v in c), alpha=0.8)

def bits(cv, t, seed, n, radius, cols, size, gravity=0.5):
    rnd = np.random.RandomState(seed)
    cx, cy = cv.w / 2, cv.h / 2
    for i in range(n):
        ang, spd, spin = rnd.uniform(0, TAU), rnd.uniform(0.45, 1.0), rnd.uniform(-9, 9)
        x = cx + math.cos(ang) * spd * t * radius
        y = cy + math.sin(ang) * spd * t * radius * 0.8 - radius * 0.35 * t + gravity * radius * t * t
        flip = abs(math.cos(spin * t + i))
        cv.rect(x, y, size, size * (0.25 + 0.75 * flip), spin * t, cols[i % len(cols)], alpha=min(1.0, 3.0 * (1 - t)))

