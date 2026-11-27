# PAPERMARCH content pass (docs/CONTENT_PASS.md): the effects of weapons and power-ups, and their HUD icons.
#   python -I -B tools/art/make_content.py captures/art/frames        (after make_fx.py; before pack_atlas.py)
# Same frame and .json format as make_fx.py, whose canvas and helpers it uses.
import sys, os, math, json
import numpy as np
from scipy import ndimage
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1] if len(sys.argv) > 1 else 'captures/art/frames'
sys.path.insert(0, HERE)
import fxlib as fx
Cv, book, puffs, bits, SS, TAU = fx.Cv, fx.book, fx.puffs, fx.bits, fx.SS, fx.TAU

# ---- weapons -----------------------------------------------------------------------------------------------------------
def streak(kind, w, h, edge, core):
    def draw(cv, t, f):
        v = cv.Y / cv.h
        cv.paint(np.exp(-(((cv.X - cv.w / 2) / (cv.w * 0.2)) ** 2)) * np.clip(1 - v, 0, 1) ** 1.2 * np.clip(v * 14, 0, 1), edge)
        cv.paint(np.exp(-(((cv.X - cv.w / 2) / (cv.w * 0.11)) ** 2)) * np.clip(1 - v * 1.5, 0, 1) * np.clip(v * 14, 0, 1), core)
    book(kind, 1, w, h, 100, True, draw, (0.5, 0.1))
streak('fx.tracer_twin', 0.2, 1.3, (60, 200, 255), (235, 255, 255))
streak('fx.tracer_hot', 0.3, 1.7, (255, 80, 20), (255, 230, 150))
streak('fx.pellet', 0.24, 0.55, (255, 140, 30), (255, 240, 200))
streak('fx.bolt', 0.3, 2.6, (50, 130, 255), (255, 255, 255))

def hit_cyan(cv, t, f):
    rnd = np.random.RandomState(4)
    cx, cy, R = cv.w / 2, cv.h / 2, cv.w / 2
    if f < 3:
        cv.glow(cx, cy, R * (0.35 + 0.2 * f), (60, 190, 255), 0.9 - 0.3 * f)
        cv.star(cx, cy, R * (0.5 + 0.15 * f), (235, 255, 255), n=4, k=0.12, p=4, rot=0.5, alpha=1 - 0.33 * f)
    for i in range(9):
        a, l = rnd.uniform(0, TAU), rnd.uniform(0.6, 1.0)
        r0, r1 = R * l * min(1.0, 1.25 * t + 0.05), R * l * min(1.0, 1.25 * t + 0.3)
        cv.line(cx + math.cos(a) * r0, cy + math.sin(a) * r0, cx + math.cos(a) * r1, cy + math.sin(a) * r1, 1.6 * SS * (1 - t) + 0.6, (150, 225, 255), alpha=1 - t ** 2)
book('fx.hit_cyan', 6, 1.0, 1.0, 84, True, hit_cyan)

def muzzle_wide(cv, t, f):
    k = [0.8, 0.6, 0.35, 0.15][f]
    cx, cy = cv.w / 2, cv.h * 0.8
    cv.glow(cx, cy - cv.h * 0.2, cv.w * 0.5, (255, 140, 30), 0.6 * k, sy=0.55)
    cv.star(cx, cy - cv.h * 0.15, cv.w * 0.5 * (0.6 + 0.4 * k), (255, 215, 110), n=9, k=0.45, p=1.2, rot=math.pi / 2 + f, alpha=k, sy=0.45)
book('fx.muzzle_wide', 4, 1.1, 0.7, 100, True, muzzle_wide, (0.5, 0.8))

def puff_small(cv, t, f):
    r = cv.w * 0.5 * (0.35 + 0.6 * t)
    cv.disc(cv.w / 2, cv.h / 2 - cv.h * 0.15 * t, r, (232, 226, 214), alpha=0.8 * (1 - t) ** 1.2, soft=r * 0.5)
book('fx.puff_small', 6, 0.5, 0.5, 80, False, puff_small)

def muzzle_ring(cv, t, f):
    cv.ring(cv.w / 2, cv.h / 2, cv.w * 0.5 * (0.25 + 0.6 * t), cv.w * 0.06 * (1 - t) + 1, (120, 190, 255), alpha=(1 - t))
    cv.glow(cv.w / 2, cv.h / 2, cv.w * 0.25, (255, 255, 255), 0.8 * (1 - t) ** 2)
book('fx.muzzle_ring', 4, 0.8, 0.8, 100, True, muzzle_ring)

def pierce(cv, t, f):
    cv.line(cv.w / 2, cv.h * (0.95 - 0.1 * t), cv.w / 2, cv.h * (0.05 + 0.1 * t), cv.w * 0.07 * (1 - t) + 1, (90, 160, 255), alpha=1 - t)
    cv.line(cv.w / 2, cv.h * 0.9, cv.w / 2, cv.h * 0.1, cv.w * 0.03 * (1 - t) + 0.6, (255, 255, 255), alpha=1 - t)
    cv.ring(cv.w / 2, cv.h / 2, cv.w * 0.42 * (0.3 + 0.7 * t), 2, (160, 210, 255), alpha=0.7 * (1 - t))
book('fx.pierce', 4, 0.8, 1.2, 84, True, pierce)

def rocket(cv, t, f):
    a = 0.12 * math.sin(TAU * t)
    cx, cy, W = cv.w / 2, cv.h * 0.42, cv.w
    c, n = math.cos(a), math.sin(a)
    def P(u, v): return cx + u * c - v * n, cy + u * n + v * c
    cv.glow(*P(0, W * 0.75), W * 0.42, (255, 150, 40), 0.9, sy=1.5)
    cv.disc(*P(0, W * 0.62), W * 0.16 * (0.8 + 0.2 * math.cos(TAU * t * 2)), (255, 235, 150))
    for u in (-0.2, 0.2):
        cv.rect(*P(W * u, W * 0.42), W * 0.1, W * 0.14, a + u, (150, 30, 36))
    cv.rect(*P(0, 0), W * 0.17, W * 0.5, a, (34, 40, 60))
    cv.rect(*P(0, 0), W * 0.13, W * 0.46, a, (230, 224, 210))
    cv.rect(*P(-W * 0.05, 0), W * 0.04, W * 0.44, a, (255, 255, 250))
    cv.disc(*P(0, -W * 0.52), W * 0.17, (34, 40, 60)); cv.disc(*P(0, -W * 0.52), W * 0.135, (227, 38, 46))
    cv.rect(*P(0, W * 0.2), W * 0.13, W * 0.05, a, (227, 38, 46))
book('fx.rocket', 8, 0.5, 1.0, 100, False, rocket, (0.5, 0.42))

def smoke_trail(cv, t, f):
    r = cv.w * 0.5 * (0.3 + 0.65 * t)
    g = 236 - 40 * t
    cv.disc(cv.w / 2 + cv.w * 0.03, cv.h / 2 + cv.w * 0.03, r, (g * 0.8, g * 0.8, g * 0.82), alpha=0.7 * (1 - t) ** 1.4, soft=r * 0.35)
    cv.disc(cv.w / 2 - cv.w * 0.03, cv.h / 2 - cv.w * 0.03, r * 0.82, (g, g, g), alpha=0.75 * (1 - t) ** 1.4, soft=r * 0.35)
book('fx.smoke_trail', 6, 0.8, 0.8, 64, False, smoke_trail)

def crate_break(cv, t, f):
    puffs(cv, t, 21, 5, cv.w * 0.42, fx.SMOKE, rise=0.15, cool=0.6)
    bits(cv, t, 8, 14, cv.w * 0.46, [(184, 130, 62), (138, 90, 52), (220, 170, 100), (90, 58, 34)], cv.w * 0.05)
book('fx.crate_break', 8, 2.6, 2.6, 56, False, crate_break, (0.5, 0.62))

# ---- pickups and power-ups ---------------------------------------------------------------------------------------------
def light_column(cv, t, f):
    u = (cv.X - cv.w / 2) / (cv.w / 2)
    v = 1 - cv.Y / cv.h                                  # 0 at the ground
    beam = np.exp(-(u / 0.55) ** 2) * np.clip(1 - v, 0, 1) ** 1.6 * np.clip(v * 30, 0, 1)
    shimmer = 0.75 + 0.25 * np.sin((v * 5 - t) * TAU + u * 3)
    cv.paint(beam * shimmer * 0.95, (255, 205, 90))
    cv.paint(np.exp(-(u / 0.2) ** 2) * np.clip(1 - v, 0, 1) ** 2.0 * 0.8, (255, 250, 220))
    for i in range(5):                                    # motes rising
        k = (i / 5 + t) % 1.0
        cv.glow(cv.w / 2 + cv.w * 0.25 * math.sin(i * 2.4), cv.h * (1 - k * 0.85), cv.w * 0.07, (255, 245, 200), 0.8 * (1 - k))
book('fx.light_column', 8, 1.5, 4.5, 40, True, light_column, (0.5, 1.0))

def pickup(cv, t, f):
    R = cv.w / 2
    cv.ring(cv.w / 2, cv.h / 2, R * (0.15 + 0.75 * (1 - (1 - t) ** 2)), R * 0.08 * (1 - t) + 1, (255, 225, 130), alpha=(1 - t) ** 1.2)
    if f < 3: cv.glow(cv.w / 2, cv.h / 2, R * 0.5, (255, 250, 220), 0.9 - 0.3 * f)
    for i in range(8):
        a = TAU * i / 8 + 0.3
        d = R * 0.8 * (1 - (1 - t) ** 2)
        cv.star(cv.w / 2 + math.cos(a) * d, cv.h / 2 + math.sin(a) * d, R * 0.14 * (1 - t), (255, 245, 190), n=4, k=0.12, p=4, alpha=1 - t)
book('fx.pickup', 8, 3.0, 3.0, 44, True, pickup)

book('fx.gun_glow', 1, 0.6, 0.6, 64, True, lambda cv, t, f: (cv.glow(cv.w / 2, cv.h / 2, cv.w * 0.5, (255, 50, 30), 0.8), cv.glow(cv.w / 2, cv.h / 2, cv.w * 0.2, (255, 190, 120), 0.8)))

def shield_dome(cv, t, f):
    # the dome's base circle has radius 1 unit: scale the sprite by the squad's footprint radius
    cx, by, R = cv.w / 2, cv.h * 0.86, cv.w * 0.47
    dx, dy = (cv.X - cx) / R, (cv.Y - by) / (R * 0.78)
    d = np.hypot(dx, dy)
    up = cv.Y <= by
    ang = np.arctan2(-dy, dx)
    cv.paint(((d < 1) & up) * 0.10 * (0.4 + 0.6 * d ** 3), (120, 190, 255))                 # the body, denser at the rim
    cv.paint(np.exp(-((d - 1) / 0.035) ** 2) * up * 0.85, (170, 220, 255))                    # the rim
    cv.paint(np.exp(-((d - 1) / 0.03) ** 2) * up * np.exp(-((ang - (0.5 + 2.2 * t)) / 0.25) ** 2), (255, 255, 255))   # a glint running round
    hexa = np.abs(np.sin(dx * 9 + dy * 5)) * np.abs(np.sin(dx * 9 - dy * 5 + 1.0))
    cv.paint(((d < 1) & up) * (hexa > 0.93) * 0.22 * d, (200, 235, 255))                      # facets
    g = np.hypot(dx, (cv.Y - by) / (R * 0.2))
    cv.paint(np.exp(-((g - 1) / 0.05) ** 2) * 0.6, (120, 190, 255))                           # where it meets the ground
book('fx.shield_dome', 8, 2.2, 1.9, 60, True, shield_dome, (0.5, 0.86))

def shield_crack(cv, t, f):
    rnd = np.random.RandomState(3)
    cx, cy = cv.w / 2, cv.h / 2
    cv.glow(cx, cy, cv.w * 0.4, (190, 230, 255), 0.8 * (1 - t))
    for i in range(6):
        a, x, y = rnd.uniform(0, TAU), cx, cy
        for k in range(3):
            a += rnd.uniform(-0.7, 0.7); l = cv.w * rnd.uniform(0.1, 0.18)
            x2, y2 = x + math.cos(a) * l, y + math.sin(a) * l
            cv.line(x, y, x2, y2, 1.4 * SS, (255, 255, 255), alpha=1 - t); x, y = x2, y2
book('fx.shield_crack', 4, 1.4, 1.4, 64, True, shield_crack)

def shield_break(cv, t, f):
    cv.ring(cv.w / 2, cv.h / 2, cv.w * 0.5 * (0.3 + 0.65 * t), cv.w * 0.03 + 1, (150, 205, 255), alpha=(1 - t) ** 1.5)
    bits(cv, t, 31, 18, cv.w * 0.46, [(150, 205, 255), (220, 240, 255), (90, 160, 255)], cv.w * 0.035, gravity=0.3)
book('fx.shield_break', 8, 3.2, 3.2, 40, True, shield_break)

def frost_ring(cv, t, f):
    R = cv.w / 2
    r = R * (0.12 + 0.8 * (1 - (1 - t) ** 2))
    cv.ring(cv.w / 2, cv.h / 2, r, R * 0.07 * (1 - t) + 1, (235, 250, 255), alpha=(1 - t) ** 1.2)
    cv.ring(cv.w / 2, cv.h / 2, r * 0.85, R * 0.14 * (1 - t) + 1, (90, 190, 255), alpha=0.6 * (1 - t) ** 1.6)
book('fx.frost_ring', 8, 5, 5, 28, True, frost_ring)

def frost_overlay(cv, t, f):
    rnd = np.random.RandomState(6)
    for i in range(7):
        x = cv.w * (0.14 + 0.12 * i); h = cv.h * rnd.uniform(0.35, 0.85); w = cv.w * rnd.uniform(0.07, 0.11)
        a = rnd.uniform(-0.25, 0.25)
        cv.rect(x, cv.h - h / 2, w, h / 2, a, (70, 130, 190))
        cv.rect(x, cv.h - h / 2, w - SS, h / 2 - SS, a, (170, 220, 250))
        cv.rect(x - w * 0.3, cv.h - h / 2, w * 0.25, h / 2 - 2 * SS, a, (240, 250, 255))
book('fx.frost_overlay', 1, 0.9, 0.5, 80, False, frost_overlay, (0.5, 0.95))

def bomb(cv, t, f):
    cx, W = cv.w / 2, cv.w
    for u in (-0.26, 0.26):
        cv.rect(cx + W * u, cv.h * 0.14, W * 0.1, cv.h * 0.12, u, (150, 30, 36))
    cv.rect(cx, cv.h * 0.2, W * 0.1, cv.h * 0.12, 0, (227, 38, 46))
    cv.disc(cx, cv.h * 0.6, W * 0.42, (20, 24, 40), sy=1.25)
    cv.disc(cx, cv.h * 0.6, W * 0.37, (58, 63, 76), sy=1.25)
    cv.disc(cx - W * 0.13, cv.h * 0.5, W * 0.1, (150, 160, 180), alpha=0.8, sy=1.6)
    cv.rect(cx, cv.h * 0.56, W * 0.37, cv.h * 0.03, 0, (255, 200, 58))
book('fx.bomb', 1, 0.7, 1.0, 100, False, bomb)

book('fx.shadow_grow', 1, 2.0, 1.0, 40, False, lambda cv, t, f: cv.disc(cv.w / 2, cv.h / 2, cv.w * 0.46, (22, 34, 79), alpha=0.5, soft=cv.w * 0.16, sy=0.5))

# ---- HUD icons: a bold coloured glyph, an ink outline, a white keyline (they read on white panels, wood and the road) ---
INK = (22, 34, 79)
FONT = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'assets', 'font', 'Roboto-Black.ttf')
def icon(kind, dp, paint):
    s = 3 * SS                                          # px per dp, supersampled
    n = int(dp * s)
    im = Image.new('RGBA', (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    o = 1.0 if dp >= 30 else dp / 40.0                  # a thinner outline on small icons (the 24 dp mode chips)
    m = 5.5 * s * o                                     # room for the outline
    k = (n - 2 * m) / 100.0
    class G:
        def P(self, pts): return [(m + x * k, m + y * k) for x, y in pts]
        def poly(self, pts, c): d.polygon(self.P(pts), fill=c + (255,))
        def ell(self, x0, y0, x1, y1, c): d.ellipse(self.P([(x0, y0), (x1, y1)]), fill=c + (255,))
        def line(self, pts, w, c):
            d.line(self.P(pts), fill=c + (255,), width=int(w * k), joint='curve')
            for x, y in pts: self.ell(x - w / 2, y - w / 2, x + w / 2, y + w / 2, c)
        def arc(self, box, a0, a1, w, c): d.arc(self.P([box[:2], box[2:]]), a0, a1, fill=c + (255,), width=int(w * k))
        def pie(self, box, a0, a1, c): d.pieslice(self.P([box[:2], box[2:]]), a0, a1, fill=c + (255,))
        def text(self, xy, t, size, c): d.text(self.P([xy])[0], t, font=ImageFont.truetype(FONT, int(size * k)), anchor='mm', fill=c + (255,))
    paint(G())
    a = np.asarray(im.getchannel('A'), float) / 255.0
    ink = np.clip(ndimage.gaussian_filter((ndimage.maximum_filter(a, size=int(3.2 * s * o) | 1) > 0.5).astype(float), 0.6 * SS) * 1.2, 0, 1)
    key = np.clip(ndimage.gaussian_filter((ndimage.maximum_filter(a, size=int(5.4 * s * o) | 1) > 0.5).astype(float), 0.6 * SS) * 1.2, 0, 1)
    out = Image.new('RGBA', (n, n), (255, 255, 255, 0))
    out.putalpha(Image.fromarray((key * 255).astype(np.uint8)))
    out.alpha_composite(Image.merge('RGBA', [Image.new('L', (n, n), v) for v in INK] + [Image.fromarray((ink * 255).astype(np.uint8))]))
    out.alpha_composite(im)
    out.save(os.path.join(OUT, '%s.still.0.0.png' % kind))
    json.dump({'ppu': s, 'ss': SS, 'frames': 1, 'elevs': {'0': {'pivot': [n / 2, n / 2], 'points': {}}}}, open(os.path.join(OUT, '%s.still.json' % kind), 'w'))
    print('drew', kind)

BROWN, STEEL, CYAN, BRASS, ORANGE, RED, YEL = (138, 90, 52), (154, 163, 180), (79, 200, 240), (217, 164, 65), (255, 138, 30), (227, 38, 46), (255, 210, 58)
BLUE, PALE, WHITE, VIOLET, GREEN, DARK, GOLD = (47, 107, 255), (143, 216, 255), (255, 255, 255), (155, 77, 255), (60, 200, 100), (58, 63, 76), (255, 200, 58)

def snowflake(g, c):
    for i in range(3):
        a = math.radians(60 * i + 90)
        p0, p1 = (50 - 44 * math.cos(a), 50 - 44 * math.sin(a)), (50 + 44 * math.cos(a), 50 + 44 * math.sin(a))
        g.line([p0, p1], 9, c)
        for p, sgn in ((p0, 1), (p1, -1)):
            m = (p[0] * 0.68 + 50 * 0.32, p[1] * 0.68 + 50 * 0.32)       # a V of two barbs on each arm
            for da in (-0.9, 0.9):
                g.line([m, (m[0] - sgn * 15 * math.cos(a + da), m[1] - sgn * 15 * math.sin(a + da))], 6, c)
    g.ell(40, 40, 60, 60, WHITE)

W = {
    'musket': lambda g: (g.line([(14, 84), (60, 42)], 12, BROWN), g.poly([(4, 84), (22, 96), (34, 74), (20, 64)], BROWN), g.line([(52, 50), (92, 12)], 6, STEEL)),
    'twin': lambda g: (g.line([(8, 68), (40, 40)], 11, BROWN), g.line([(34, 46), (76, 8)], 7, CYAN), g.line([(30, 92), (62, 64)], 11, BROWN), g.line([(56, 70), (96, 34)], 7, CYAN)),
    'scatter': lambda g: (g.line([(8, 86), (30, 68)], 13, BROWN), g.line([(26, 72), (56, 46)], 14, BRASS), g.poly([(48, 36), (78, 18), (90, 46), (66, 60)], BRASS),
                          g.ell(86, 2, 98, 14, ORANGE), g.ell(88, 24, 100, 36, ORANGE), g.ell(68, 0, 80, 12, ORANGE)),
    'lance': lambda g: (g.line([(8, 92), (74, 26)], 8, (220, 230, 244)), g.ell(24, 62, 42, 80, BLUE), g.ell(46, 40, 64, 58, BLUE), g.poly([(64, 20), (98, 2), (80, 36)], (111, 184, 255))),
    'rocket': lambda g: (g.ell(2, 70, 30, 98, ORANGE), g.ell(8, 76, 22, 90, YEL), g.poly([(24, 58), (10, 56), (26, 42)], RED), g.poly([(42, 76), (44, 90), (58, 74)], RED),
                         g.poly([(24, 60), (40, 76), (78, 38), (62, 22)], (233, 223, 206)), g.poly([(62, 22), (78, 38), (96, 4)], RED)),
    'flame': lambda g: (g.poly([(50, 2), (70, 30), (84, 56), (76, 80), (50, 96), (24, 80), (16, 56), (34, 34)], ORANGE), g.poly([(50, 40), (64, 60), (62, 80), (50, 88), (38, 80), (36, 62)], YEL)),
    'coil': lambda g: g.poly([(60, 2), (20, 54), (44, 54), (34, 98), (82, 40), (56, 40)], VIOLET),
    'mortar': lambda g: (g.line([(10, 92), (64, 92)], 9, DARK), g.poly([(30, 88), (14, 72), (50, 38), (66, 54)], (106, 138, 90)), g.ell(72, 6, 94, 28, DARK), g.ell(56, 26, 64, 34, DARK), g.ell(62, 14, 68, 20, DARK)),
    'frost': lambda g: snowflake(g, PALE),
}
for name, paint in W.items():
    icon('ui.wpn_' + name, 40, paint)

def burst(g):
    pts = []
    for i in range(16):
        r = 48 if i % 2 == 0 else 30
        pts.append((50 + r * math.cos(TAU * i / 16), 50 + r * math.sin(TAU * i / 16)))
    g.poly(pts, RED); g.ell(32, 32, 68, 68, YEL)
P_ = {
    'rapid': lambda g: (g.poly([(50, 4), (90, 44), (68, 44), (50, 26), (32, 44), (10, 44)], ORANGE), g.poly([(50, 42), (90, 82), (68, 82), (50, 64), (32, 82), (10, 82)], ORANGE)),
    'power': burst,
    'shield': lambda g: (g.poly([(50, 4), (88, 18), (84, 58), (50, 96), (16, 58), (12, 18)], BLUE), g.poly([(50, 14), (50, 84), (26, 56), (22, 26)], (138, 180, 255))),
    'freeze': lambda g: snowflake(g, (190, 232, 255)),
    'airstrike': lambda g: (g.poly([(36, 2), (64, 2), (60, 26), (40, 26)], RED), g.ell(22, 20, 78, 96, DARK), g.ell(34, 34, 48, 56, STEEL)),
    'magnet': lambda g: (g.arc((12, 8, 88, 84), 180, 360, 24, RED), g.poly([(12, 46), (36, 46), (36, 74), (12, 74)], RED), g.poly([(64, 46), (88, 46), (88, 74), (64, 74)], RED),
                         g.poly([(12, 74), (36, 74), (36, 94), (12, 94)], WHITE), g.poly([(64, 74), (88, 74), (88, 94), (64, 94)], WHITE)),
    'giants': lambda g: g.poly([(50, 4), (92, 50), (66, 50), (66, 96), (34, 96), (34, 50), (8, 50)], GREEN),
    'reinforce': lambda g: (g.line([(12, 44), (50, 84)], 3, INK), g.line([(88, 44), (50, 84)], 3, INK), g.line([(50, 44), (50, 84)], 3, INK),
                            g.pie((8, 4, 92, 88), 180, 360, BLUE), g.pie((8, 4, 92, 88), 240, 300, WHITE), g.ell(40, 76, 60, 96, BLUE)),
    'quick': lambda g: (g.poly([(6, 12), (30, 12), (56, 50), (30, 88), (6, 88), (32, 50)], WHITE), g.poly([(46, 12), (70, 12), (96, 50), (70, 88), (46, 88), (72, 50)], WHITE)),
    'hourglass': lambda g: (g.poly([(22, 10), (78, 10), (50, 50)], VIOLET), g.poly([(50, 50), (78, 90), (22, 90)], VIOLET), g.poly([(36, 78), (64, 78), (72, 90), (28, 90)], GOLD),
                            g.poly([(14, 2), (86, 2), (86, 12), (14, 12)], GOLD), g.poly([(14, 88), (86, 88), (86, 98), (14, 98)], GOLD)),
    'double': lambda g: g.text((50, 50), '×2', 78, GOLD),
    'fieldgun': lambda g: (g.poly([(22, 50), (26, 30), (92, 14), (96, 36)], BRASS), g.ell(10, 46, 58, 94, BROWN), g.ell(26, 62, 42, 78, (90, 58, 34))),
}
for name, paint in P_.items():
    icon('ui.pow_' + name, 36, paint)

# ---- steps 3 and 4: mines' and catapults' effects, cages, coins, warnings, mode icons -----------------------------------
def flat(kind, w_units, h_units, ppu, paint, pivot=(0.5, 0.5)):
    """A one-frame effect drawn with PIL polygons; paint(draw, s) with s = pixels per unit."""
    s = ppu * SS
    im = Image.new('RGBA', (int(w_units * s), int(h_units * s)), (0, 0, 0, 0))
    paint(ImageDraw.Draw(im), s)
    im.save(os.path.join(OUT, '%s.play.0.0.png' % kind))
    json.dump({'ppu': s, 'ss': SS, 'frames': 1, 'elevs': {'0': {'pivot': [im.width * pivot[0], im.height * pivot[1]], 'points': {}}}}, open(os.path.join(OUT, '%s.play.json' % kind), 'w'))
    print('drew', kind)

def arrow(d, s):        # an arrow in flight, point up: rotate it along its path
    d.line([(0.09 * s, 0.14 * s), (0.09 * s, 0.84 * s)], fill=(90, 58, 34, 255), width=int(0.035 * s))
    d.polygon([(0.09 * s, 0.0), (0.15 * s, 0.17 * s), (0.03 * s, 0.17 * s)], fill=(200, 208, 220, 255), outline=INK + (255,))
    d.polygon([(0.09 * s, 0.7 * s), (0.17 * s, 0.9 * s), (0.09 * s, 0.84 * s), (0.01 * s, 0.9 * s)], fill=(227, 38, 46, 255))
flat('fx.arrow', 0.18, 0.92, 100, arrow, (0.5, 0.1))

def arrow_ground(d, s):  # painted on the road: "go here" (the stomp wave's gap); drawn flat, point up the road
    pts = [(0.8, 0.06), (1.5, 0.8), (1.08, 0.8), (1.08, 1.5), (0.52, 1.5), (0.52, 0.8), (0.1, 0.8)]
    d.polygon([(x * s, y * s) for x, y in pts], fill=(255, 210, 58, 255), outline=INK + (255,), width=int(0.07 * s))
flat('fx.arrow_ground', 1.6, 1.6, 64, arrow_ground)

def coin(cv, t, f):       # the disc turns about its upright axis
    w = max(0.14, abs(math.cos(math.pi * t)))
    R = cv.h * 0.44
    for r, c in ((R, (150, 90, 0)), (R - SS, (255, 200, 58)), (R * 0.68, (255, 224, 122))):
        d = np.hypot((cv.X - cv.w / 2) / w, cv.Y - cv.h / 2)
        cv.paint(np.clip((r - d) / 2.0 + 0.5, 0, 1), c)
    cv.disc(cv.w / 2 - R * 0.25 * w, cv.h / 2 - R * 0.3, R * 0.16, (255, 250, 220), alpha=0.9 * w)
book('fx.coin', 8, 0.45, 0.45, 80, False, coin)

def cage_open(cv, t, f):
    puffs(cv, t, 23, 4, cv.w * 0.4, fx.SMOKE, rise=0.15, cool=0.6)
    bits(cv, t, 12, 16, cv.w * 0.46, [(47, 168, 90), (31, 122, 64), (120, 220, 150), (255, 200, 58)], cv.w * 0.04)
book('fx.cage_open', 8, 3.2, 3.2, 44, False, cage_open, (0.5, 0.6))

def warn_circle(cv, t, f):     # the circle's radius is 1 unit: scale by the blast radius; drawn flat on the road
    R = cv.w / 2.3
    k = 0.6 + 0.4 * math.sin(TAU * t)
    dx, dy = cv.X - cv.w / 2, cv.Y - cv.h / 2
    d = np.hypot(dx, dy) / R
    cv.paint((d < 1) * (0.16 + 0.12 * k) * (0.4 + 0.6 * d), (255, 40, 30))
    cv.paint(np.exp(-((d - 1) / 0.035) ** 2) * (0.75 + 0.25 * k), (255, 70, 50))
    ang = np.arctan2(dy, dx)
    tick = (np.abs(((ang / TAU * 12) % 1.0) - 0.5) < 0.16) & (d > 0.72 - 0.1 * k) & (d < 0.93)       # 12 teeth pointing in
    cv.paint(tick * np.clip((d - (0.72 - 0.1 * k)) / 0.2, 0, 1) * 0.9, (255, 120, 80))
book('fx.warn_circle', 8, 2.3, 2.3, 44, True, warn_circle)

DIRT = [(0.0, (86, 60, 38)), (0.5, (140, 104, 66)), (1.0, (200, 164, 114))]
def dirt_burst(cv, t, f):
    puffs(cv, t, 17, 12, cv.w * 0.5, DIRT, rise=0.5, cool=0.8)
    bits(cv, t, 4, 12, cv.w * 0.5, [(86, 60, 38), (120, 124, 132), (140, 104, 66)], cv.w * 0.03, gravity=0.9)
book('fx.dirt_burst', 8, 3.4, 3.4, 44, False, dirt_burst, (0.5, 0.66))

def warn_half(cv, t, f):       # a tile of the red zone: 2.5 units square, chevrons running down the road; it tiles both ways
    u, v = cv.X / cv.w * 2, cv.Y / cv.h * 2
    ch = ((v - np.abs((u % 1.0) - 0.5) * 0.9 - t) % 1.0) < 0.36
    cv.paint(np.full_like(u, 0.2), (255, 40, 30))
    cv.paint(ch * 0.55, (255, 80, 60))
book('fx.warn_half', 8, 2.5, 2.5, 40, True, warn_half)

def wave_ridge(cv, t, f):      # a rolling ridge of dust, 2 units wide: it tiles across the road
    n = 6
    for layer, (col, rr, yy) in enumerate((((150, 128, 98), 0.2, 0.74), ((196, 176, 142), 0.22, 0.62), ((232, 216, 186), 0.17, 0.5))):
        for k in range(n):
            x = cv.w * ((k + 0.5 * (layer % 2)) / n)
            bob = math.sin(TAU * (t + k / n + layer * 0.3))
            r = cv.w * rr * (0.85 + 0.15 * bob)
            y = cv.h * (yy - 0.07 * bob)
            for xx in (x - cv.w, x, x + cv.w):
                cv.disc(xx, y, r, col, soft=3)
    foot = np.clip((cv.h * 0.98 - cv.Y) / (cv.h * 0.12), 0, 1)             # it thins out into the road at its foot
    cv.a *= foot
    cv.rgb *= foot[..., None]
book('fx.wave_ridge', 8, 2.0, 1.3, 64, False, wave_ridge, (0.5, 0.92))

# mode chips (24 dp), beside the level chip
def _wall(g):
    g.poly([(8, 34), (8, 92), (92, 92), (92, 34), (78, 34), (78, 20), (60, 20), (60, 34), (40, 34), (40, 20), (22, 20), (22, 34)], (170, 176, 190))
    g.poly([(40, 92), (40, 62), (50, 52), (60, 62), (60, 92)], RED)
def _boot(g):
    g.poly([(30, 8), (62, 8), (62, 56), (92, 70), (92, 92), (26, 92), (26, 60), (30, 50)], BROWN)
    g.poly([(26, 82), (92, 82), (92, 92), (26, 92)], DARK)
    g.poly([(26, 44), (4, 30), (22, 34), (2, 16), (30, 26)], WHITE)
def _cage(g):
    g.poly([(14, 22), (86, 22), (86, 92), (14, 92)], (31, 122, 64))
    for x in (26, 44, 62):
        g.poly([(x, 30), (x + 10, 30), (x + 10, 84), (x, 84)], (150, 236, 170))
    g.poly([(8, 12), (92, 12), (92, 26), (8, 26)], GREEN)
def _guns(g):
    g.line([(12, 88), (84, 16)], 11, BROWN); g.line([(60, 40), (90, 10)], 6, STEEL)
    g.line([(88, 88), (16, 16)], 11, BROWN); g.line([(40, 40), (10, 10)], 6, STEEL)
def _hammer(g):
    g.line([(20, 90), (56, 40)], 11, BROWN)
    g.poly([(30, 30), (62, 8), (88, 44), (56, 66)], (170, 176, 190))
def _giant(g):
    g.ell(18, 34, 82, 98, (163, 19, 31)); g.ell(34, 8, 66, 40, (217, 154, 114))
    g.poly([(30, 16), (14, 2), (38, 8)], (243, 230, 196)); g.poly([(70, 16), (86, 2), (62, 8)], (243, 230, 196))
def _coin(g):
    g.ell(8, 8, 92, 92, GOLD); g.ell(22, 22, 78, 78, (255, 224, 122))
    g.poly([(50, 28), (56, 44), (72, 44), (59, 54), (64, 70), (50, 60), (36, 70), (41, 54), (28, 44), (44, 44)], (217, 138, 0))
def _clock(g):
    g.ell(8, 8, 92, 92, (255, 246, 224)); g.line([(50, 50), (50, 24)], 8, INK); g.line([(50, 50), (70, 60)], 8, INK); g.ell(44, 44, 56, 56, RED)
def _flag(g):
    g.line([(22, 94), (22, 8)], 8, BROWN); g.poly([(26, 10), (92, 26), (60, 36), (92, 52), (26, 58)], BLUE)
def _ogre(g):
    g.ell(18, 26, 82, 94, (111, 174, 60)); g.poly([(26, 40), (4, 6), (40, 28)], (243, 230, 196)); g.poly([(74, 40), (96, 6), (60, 28)], (243, 230, 196))
    g.ell(32, 46, 44, 58, YEL); g.ell(56, 46, 68, 58, YEL); g.poly([(36, 70), (40, 84), (46, 70)], WHITE); g.poly([(54, 70), (60, 84), (64, 70)], WHITE)
for name, paint in (('siege', _wall), ('rush', _boot), ('rescue', _cage), ('arsenal', _guns), ('gauntlet', _hammer), ('giants', _giant),
                    ('gold', _coin), ('laststand', _clock), ('march', _flag), ('boss', _ogre)):
    icon('ui.mode_' + name, 24, paint)

# ---- step 5: weapons 5-9 and power-ups 6-12 -----------------------------------------------------------------------------
def flame(cv, t, f):          # the flamethrower's cone, 9 units long, the nozzle at the foot: a body of fire and rolling puffs that loop
    rnd = np.random.RandomState(8)
    nx, ny, L = cv.w / 2, cv.h * 0.98, cv.h * 0.95
    v = np.clip((ny - cv.Y) / L, 0, 1)                                   # 0 at the nozzle, 1 at the cone's end
    half = L * (0.02 + 0.3 * v)
    u = (cv.X - nx) / half
    lick = 0.75 + 0.25 * np.sin(v * 22 - t * TAU * 2 + u * 2.5) * np.sin(v * 9 + t * TAU + 1.0)
    body = np.exp(-(u ** 2) * 1.6) * np.clip(v * 30, 0, 1) * np.clip((1 - v) * 3.5, 0, 1) ** 0.8 * lick
    cv.paint(body * 0.5, (255, 96, 16))
    cv.paint(np.exp(-(u ** 2) * 4.0) * np.clip(v * 30, 0, 1) * np.clip(1 - v * 1.5, 0, 1) * lick * 0.55, (255, 200, 60))
    cv.paint(np.exp(-(u ** 2) * 7.0) * np.clip(v * 40, 0, 1) * np.clip(1 - v * 3.2, 0, 1) * 0.7, (255, 250, 215))
    for i in range(46):
        ph, side, wob = rnd.rand(), rnd.uniform(-1, 1), rnd.uniform(0, TAU)
        s_ = (t + ph) % 1.0
        d = 0.08 + 0.92 * s_ ** 0.8
        x = nx + side * d * L * 0.24 + math.sin(wob + TAU * t) * L * 0.012
        y = ny - d * L
        r = L * (0.03 + 0.11 * d)
        fade = min(1.0, s_ * 6) * (1 - s_) ** 0.8
        col = fx.lerp3([(0.0, (255, 230, 150)), (0.3, (255, 170, 40)), (0.65, (255, 90, 16)), (1.0, (140, 26, 10))], s_)
        cv.glow(x, y, r * 1.3, col, 0.5 * fade, p=2.2)
book('fx.flame', 16, 5.8, 9.4, 26, True, flame, (0.5, 0.98))

def char(cv, t, f):
    r = cv.w * 0.5 * (0.35 + 0.6 * t)
    cv.disc(cv.w / 2, cv.h / 2 - cv.h * 0.2 * t, r, (40, 34, 36), alpha=0.75 * (1 - t) ** 1.2, soft=r * 0.4)
    cv.disc(cv.w / 2 - r * 0.2, cv.h / 2 - cv.h * 0.2 * t - r * 0.2, r * 0.5, (86, 70, 66), alpha=0.6 * (1 - t) ** 1.2, soft=r * 0.4)
book('fx.char', 6, 0.9, 0.9, 64, False, char)

def muzzle_big(cv, t, f):
    k = [1.0, 0.8, 0.55, 0.3, 0.12][f]
    cx, cy = cv.w / 2, cv.h * 0.78
    cv.glow(cx, cy - cv.h * 0.2, cv.w * 0.5, (255, 140, 30), 0.7 * k)
    cv.star(cx, cy - cv.h * 0.22, cv.w * 0.46 * (0.6 + 0.4 * k), (255, 200, 80), n=11, k=0.62, p=1.4, rot=math.pi / 2 + f * 0.7, alpha=k, sy=1.3)
    cv.glow(cx, cy - cv.h * 0.2, cv.w * 0.3 * k, (255, 250, 225), 0.95 * k, sy=1.3)
    cv.ring(cx, cy - cv.h * 0.1, cv.w * 0.45 * (0.3 + 0.7 * t), cv.w * 0.03, (255, 200, 120), alpha=0.5 * (1 - t))
book('fx.muzzle_big', 5, 2.4, 2.6, 48, True, muzzle_big, (0.5, 0.78))

def dust_line(cv, t, f):      # one burst of the dust a cannonball throws up; drop one every unit of its path
    for i, (dx, s_) in enumerate(((-0.28, 0.6), (0.0, 0.8), (0.3, 0.55), (-0.1, 0.5))):
        r = cv.w * 0.3 * s_ * (0.5 + 0.8 * t)
        cv.disc(cv.w * (0.5 + dx * (1 + 0.5 * t)), cv.h * (0.72 - 0.35 * t * s_), r, (214, 196, 164) if i % 2 else (232, 216, 186), alpha=0.8 * (1 - t) ** 1.3, soft=r * 0.4)
book('fx.dust_line', 6, 1.6, 1.1, 56, False, dust_line, (0.5, 0.8))

def lightning(cv, t, f):      # a bolt from the foot of the quad to its top: stretch the quad between the two points
    rnd = np.random.RandomState(40 + f)
    n = 9
    xs = [cv.w / 2] + [cv.w / 2 + rnd.uniform(-0.3, 0.3) * cv.w for _ in range(n - 1)] + [cv.w / 2]
    ys = [cv.h * (1 - i / n) for i in range(n + 1)]
    for i in range(n):
        cv.line(xs[i], ys[i], xs[i + 1], ys[i + 1], cv.w * 0.1, (150, 90, 255), alpha=0.75)
    for i in range(n):
        cv.line(xs[i], ys[i], xs[i + 1], ys[i + 1], cv.w * 0.035, (255, 255, 255))
    for i in rnd.choice(np.arange(2, n - 1), 3, replace=False):     # short forks
        a = rnd.uniform(-1, 1)
        cv.line(xs[i], ys[i], xs[i] + a * cv.w * 0.35, ys[i] - cv.h * 0.07, cv.w * 0.03, (200, 170, 255), alpha=0.8)
book('fx.lightning', 4, 0.9, 4.0, 48, True, lightning, (0.5, 1.0))

def zap(cv, t, f):
    rnd = np.random.RandomState(11 + f)
    cx, cy, R = cv.w / 2, cv.h / 2, cv.w / 2
    cv.glow(cx, cy, R * 0.8, (160, 90, 255), 0.8 * (1 - t))
    cv.star(cx, cy, R * (0.5 + 0.4 * t), (255, 255, 255), n=4, k=0.1, p=5, rot=0.4 + f, alpha=1 - t)
    for i in range(5):
        a = rnd.uniform(0, TAU); x, y = cx, cy
        for k in range(3):
            a += rnd.uniform(-0.8, 0.8); x2, y2 = x + math.cos(a) * R * 0.3, y + math.sin(a) * R * 0.3
            cv.line(x, y, x2, y2, 1.2 * SS, (210, 180, 255), alpha=1 - t); x, y = x2, y2
book('fx.zap', 5, 1.3, 1.3, 64, True, zap)

def shell(cv, t, f):          # the mortar's shell, nose up: rotate it along its arc
    cx, W = cv.w / 2, cv.w
    for u in (-0.3, 0.3):
        cv.rect(cx + W * u, cv.h * 0.86, W * 0.1, cv.h * 0.1, u * 1.2, (40, 46, 40))
    cv.rect(cx, cv.h * 0.8, W * 0.12, cv.h * 0.14, 0, (40, 46, 40))
    cv.disc(cx, cv.h * 0.42, W * 0.44, (20, 24, 30), sy=1.6)
    cv.disc(cx, cv.h * 0.42, W * 0.38, (79, 98, 64), sy=1.6)
    cv.disc(cx - W * 0.12, cv.h * 0.34, W * 0.1, (150, 170, 130), alpha=0.8, sy=2.0)
    cv.rect(cx, cv.h * 0.5, W * 0.38, cv.h * 0.025, 0, (255, 200, 58))
book('fx.shell', 1, 0.45, 0.85, 100, False, shell)

def target_ring(cv, t, f):    # where the shell will land, flat on the road; its outer ring has radius 1: scale by the blast radius (2.6)
    R = cv.w / 2.3
    dx, dy = cv.X - cv.w / 2, cv.Y - cv.h / 2
    d = np.hypot(dx, dy) / R
    cv.paint(np.exp(-((d - 1) / 0.03) ** 2) * 0.9, (255, 220, 120))
    inner = 1 - 0.7 * t                                                # a ring closing in, again and again
    cv.paint(np.exp(-((d - inner) / 0.025) ** 2) * 0.8 * (1 - t * 0.5), (255, 150, 40))
    ang = np.arctan2(dy, dx)
    cross = (np.abs(np.sin(ang * 2)) < 0.06) & (d > 0.15) & (d < 0.45)
    cv.paint(cross * 0.8, (255, 240, 200))
    dash = ((ang / TAU * 16 + t) % 1.0 < 0.5) & (np.abs(d - 0.86) < 0.025)
    cv.paint(dash * 0.7, (255, 200, 90))
book('fx.target_ring', 8, 2.3, 2.3, 44, True, target_ring)

streak('fx.frost_bolt', 0.3, 1.2, (90, 190, 255), (240, 252, 255))

def ice_shards(cv, t, f):
    cv.ring(cv.w / 2, cv.h / 2, cv.w * 0.45 * (0.3 + 0.7 * t), 1.5 * SS, (200, 235, 255), alpha=0.7 * (1 - t))
    bits(cv, t, 19, 12, cv.w * 0.44, [(170, 220, 250), (240, 250, 255), (110, 180, 240)], cv.w * 0.05, gravity=0.5)
book('fx.ice_shards', 6, 1.4, 1.4, 64, False, ice_shards)

def sound_ring(cv, t, f):     # the drummer's beat: two rings spreading from the drum
    R = cv.w / 2
    for k, col in ((0.0, (255, 90, 70)), (0.25, (255, 200, 90))):
        tt = t - k
        if tt < 0: continue
        cv.ring(cv.w / 2, cv.h / 2, R * (0.15 + 0.8 * tt / (1 - k)), R * 0.035 + 1, col, alpha=(1 - tt / (1 - k)) ** 1.2 * 0.9)
book('fx.sound_ring', 8, 2.6, 2.6, 40, True, sound_ring)

def magnet_arc(cv, t, f):     # gold sparks running from the foot of the quad to its top: stretch it from a barrel to the squad
    v = 1 - cv.Y / cv.h
    cv.paint(np.exp(-(((cv.X - cv.w / 2 - cv.w * 0.12 * np.sin(v * TAU * 1.5 + t * TAU)) / (cv.w * 0.07)) ** 2)) * 0.25 * np.sin(v * math.pi), (255, 200, 80))
    for i in range(7):
        k = (i / 7 + t) % 1.0
        x = cv.w / 2 + cv.w * 0.12 * math.sin(k * TAU * 1.5 + t * TAU)
        y = cv.h * (1 - k)
        cv.glow(x, y, cv.w * 0.16, (255, 230, 130), 0.9 * math.sin(k * math.pi) ** 0.5)
        cv.star(x, y, cv.w * 0.2, (255, 252, 230), n=4, k=0.12, p=4, alpha=math.sin(k * math.pi) ** 0.5)
book('fx.magnet_arc', 8, 0.8, 4.0, 40, True, magnet_arc, (0.5, 1.0))

def stomp_dust(cv, t, f):     # under a giant soldier's foot: a low ring of dust
    n = 10
    for i in range(n):
        a = TAU * i / n
        d = cv.w * 0.36 * (0.3 + 0.7 * (1 - (1 - t) ** 2))
        r = cv.w * 0.12 * (0.6 + 0.6 * t)
        cv.disc(cv.w / 2 + math.cos(a) * d, cv.h * 0.62 + math.sin(a) * d * 0.4 - cv.h * 0.12 * t, r, (226, 208, 176) if i % 2 else (206, 188, 156), alpha=0.75 * (1 - t) ** 1.2, soft=r * 0.4)
book('fx.stomp_dust', 6, 2.4, 1.3, 48, False, stomp_dust, (0.5, 0.65))

def speed_lines(cv, t, f):    # streaks trailing behind a quick-marching squad; the tile loops top to bottom
    rnd = np.random.RandomState(5)
    for i in range(14):
        x, ph, ln, th = rnd.uniform(0.05, 0.95) * cv.w, rnd.rand(), rnd.uniform(0.18, 0.4), rnd.uniform(0.008, 0.02)
        y = ((ph + t) % 1.0) * cv.h
        for yy in (y - cv.h, y, y + cv.h):
            cv.line(x, yy, x, yy + ln * cv.h, th * cv.w, (255, 255, 255), alpha=0.7)
book('fx.speed_lines', 8, 3.0, 3.0, 36, True, speed_lines)

def gate_gold_rim(cv, t, f):  # a gold frame over a gate panel (drawn for 5 x 1.95 units: stretch it to the panel), a glint running round
    b = cv.h * 0.07
    ex, ey = np.minimum(cv.X, cv.w - cv.X), np.minimum(cv.Y, cv.h - cv.Y)
    e = np.minimum(ex, ey)
    cv.paint(np.clip((b - e) / 2 + 0.5, 0, 1) * 0.85, (255, 200, 58))
    cv.paint(np.exp(-((e - b) / (b * 0.9)) ** 2) * 0.35, (255, 170, 30))
    per = 2 * (cv.w + cv.h)                                             # position along the frame, clockwise from the top left
    pos = np.where(ey <= ex, np.where(cv.Y < cv.h / 2, cv.X, cv.w + cv.h + (cv.w - cv.X)), np.where(cv.X > cv.w / 2, cv.w + cv.Y, 2 * cv.w + cv.h + (cv.h - cv.Y))) / per
    g = np.abs(((pos - t + 0.5) % 1.0) - 0.5)
    cv.paint(np.clip((b * 1.3 - e) / 2 + 0.5, 0, 1) * np.exp(-(g / 0.03) ** 2), (255, 255, 240))
book('fx.gate_gold_rim', 8, 5.0, 1.95, 40, True, gate_gold_rim)
