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
    m = 5.5 * s                                         # room for the outline
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
    ink = np.clip(ndimage.gaussian_filter((ndimage.maximum_filter(a, size=int(3.2 * s) | 1) > 0.5).astype(float), 0.6 * SS) * 1.2, 0, 1)
    key = np.clip(ndimage.gaussian_filter((ndimage.maximum_filter(a, size=int(5.4 * s) | 1) > 0.5).astype(float), 0.6 * SS) * 1.2, 0, 1)
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
