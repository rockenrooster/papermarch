# PAPERMARCH: the 2D effect flipbooks and ambient particles, drawn with numpy.
#   python -I -B tools/art/make_fx.py captures/art/frames
# Writes <out>/<kind>.play.0.<frame>.png (2x supersampled) and <kind>.play.json, like render_sprites.py.
# Additive sprites: colour in RGB, intensity in alpha (blend SRC_ALPHA, ONE).
import sys, os, json, math
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from worlds import WORLDS
from fxlib import *
from fxlib import OUT, SS, TAU

# ---- the flipbooks -----------------------------------------------------------------------------------------------------
def muzzle(cv, t, f):
    k = [0.75, 0.6, 0.38, 0.15][f]
    cx, cy, R = cv.w / 2, cv.h * 0.72, cv.w * 0.5
    cv.glow(cx, cy - R * 0.25, R * 0.9 * (0.7 + 0.3 * k), (255, 150, 30), 0.55 * k, sy=1.3)
    cv.star(cx, cy - R * 0.2, R * (0.55 + 0.45 * k), (255, 200, 60), n=5, k=0.18, p=2.5, rot=math.pi / 2 + f, alpha=k, sy=1.5)
    cv.star(cx, cy - R * 0.15, R * 0.55 * k, (255, 236, 170), n=5, k=0.3, p=2.0, rot=math.pi / 2 + f, alpha=k * 0.8, sy=1.5)
book('fx.muzzle', 4, 0.6, 0.75, 100, True, muzzle, (0.5, 0.72))

def tracer(cv, t, f):
    v = cv.Y / cv.h
    across = np.exp(-(((cv.X - cv.w / 2) / (cv.w * 0.2)) ** 2))
    cv.paint(across * np.clip(1 - v, 0, 1) ** 1.2 * np.clip(v * 14, 0, 1), (255, 160, 30))
    cv.paint(np.exp(-(((cv.X - cv.w / 2) / (cv.w * 0.11)) ** 2)) * np.clip(1 - v * 1.5, 0, 1) * np.clip(v * 14, 0, 1), (255, 250, 215))
book('fx.tracer', 1, 0.26, 1.5, 100, True, tracer, (0.5, 0.1))

def hit(cv, t, f):
    rnd = np.random.RandomState(4)
    cx, cy, R = cv.w / 2, cv.h / 2, cv.w / 2
    if f < 3:
        cv.glow(cx, cy, R * (0.35 + 0.2 * f), (255, 190, 60), 0.9 - 0.3 * f)
        cv.star(cx, cy, R * (0.5 + 0.15 * f), (255, 250, 220), n=4, k=0.12, p=4, rot=0.5, alpha=1 - 0.33 * f)
    for i in range(9):
        a, l = rnd.uniform(0, TAU), rnd.uniform(0.6, 1.0)
        r0, r1 = R * l * min(1.0, 1.25 * t + 0.05), R * l * min(1.0, 1.25 * t + 0.3)
        cv.line(cx + math.cos(a) * r0, cy + math.sin(a) * r0, cx + math.cos(a) * r1, cy + math.sin(a) * r1, 1.6 * SS * (1 - t) + 0.6,
                (255, 220, 120), alpha=1 - t ** 2)
book('fx.hit', 6, 1.0, 1.0, 84, True, hit)

for name, cols in (('red', [(227, 38, 46), (168, 20, 31), (74, 80, 96), (243, 230, 196)]), ('blue', [(47, 107, 255), (29, 73, 200), (255, 255, 255), (255, 200, 58)])):
    def poof(cv, t, f, cols=cols):
        puffs(cv, t, 11, 8, cv.w * 0.5, SMOKE, rise=0.2, cool=0.6)
        bits(cv, t, 5, 9, cv.w * 0.46, cols, cv.w * 0.035)
    book('fx.poof_' + name, 8, 1.6, 1.6, 64, False, poof, (0.5, 0.62))

def debris(cv, t, f):
    a = TAU * t
    flip = math.cos(a)
    c = (138, 90, 52) if flip > 0 else (90, 58, 34)
    cv.rect(cv.w / 2, cv.h / 2, cv.w * 0.42, cv.w * (0.03 + 0.13 * abs(flip)), a * 0.5 + 0.4, (60, 38, 24))
    cv.rect(cv.w / 2, cv.h / 2, cv.w * 0.42 - SS, cv.w * (0.03 + 0.13 * abs(flip)) - SS, a * 0.5 + 0.4, c)
book('fx.debris', 8, 0.5, 0.5, 80, False, debris)

def boom(n, seed):
    def draw(cv, t, f):
        R = cv.w / 2
        puffs(cv, t, seed, n, R, FIRE, rise=0.22, cool=1.0)
        if f < 3:
            cv.star(cv.w / 2, cv.h / 2, R * (0.55 + 0.2 * f), (255, 240, 170), n=7, k=0.35, p=2.0, rot=0.3, alpha=1 - f * 0.34)
            cv.disc(cv.w / 2, cv.h / 2, R * (0.3 + 0.1 * f), (255, 255, 240), alpha=1 - f * 0.34)
    return draw
def boom_glow(cv, t, f):
    k = math.exp(-4.0 * t) * min(1.0, (f + 1) / 2)
    cv.glow(cv.w / 2, cv.h / 2, cv.w * 0.5 * (0.5 + 0.5 * min(1, 3 * t)), (255, 140, 40), 0.9 * k, p=1.5)
    cv.glow(cv.w / 2, cv.h / 2, cv.w * 0.25, (255, 240, 180), k)
book('fx.explosion', 14, 4.5, 4.5, 50, False, boom(22, 2), (0.5, 0.56))
book('fx.explosion_glow', 14, 4.5, 4.5, 16, True, boom_glow)
book('fx.bigboom', 16, 11, 11, 27, False, boom(46, 9), (0.5, 0.56))
book('fx.bigboom_glow', 16, 11, 11, 8, True, boom_glow)

def ring(cv, t, f):
    R = cv.w / 2
    r = R * (0.12 + 0.8 * (1 - (1 - t) ** 2))
    cv.ring(cv.w / 2, cv.h / 2, r, R * 0.07 * (1 - t) + 1, (255, 235, 170), alpha=(1 - t) ** 1.2)
    cv.ring(cv.w / 2, cv.h / 2, r * 0.82, R * 0.12 * (1 - t) + 1, (255, 160, 60), alpha=0.5 * (1 - t) ** 2)
book('fx.ring', 8, 5, 5, 28, True, ring)

def dust(cv, t, f):
    R = cv.w / 2
    for i, (dx, dy, s) in enumerate(((-0.3, 0.1, 0.5), (0.25, 0.05, 0.6), (0.0, -0.15, 0.7))):
        r = R * s * (0.5 + 0.7 * t)
        cv.disc(cv.w / 2 + dx * R * (1 + t), cv.h * 0.6 + dy * R - R * 0.3 * t, r, (226, 208, 176), alpha=0.75 * (1 - t) ** 1.3, soft=r * 0.5)
book('fx.dust', 6, 0.9, 0.9, 64, False, dust, (0.5, 0.7))

def sparkle(cv, t, f):
    k = math.sin(math.pi * min(1.0, t * 1.15 + 0.08))
    cv.glow(cv.w / 2, cv.h / 2, cv.w * 0.3 * k, (255, 210, 110), 0.4 * k)
    cv.star(cv.w / 2, cv.h / 2, cv.w * 0.48 * k, (255, 240, 190), n=4, k=0.1, p=5, rot=t * 1.2, alpha=min(0.75, k))
book('fx.sparkle', 6, 0.6, 0.6, 80, True, sparkle)

for name, col, hi, up in (('add', (46, 139, 255), (191, 228, 255), 1), ('mul', (155, 77, 255), (230, 204, 255), 1), ('neg', (240, 50, 60), (255, 194, 184), -1)):
    def gp(cv, t, f, col=col, hi=hi, up=up):
        rnd = np.random.RandomState(7)
        W, H = cv.w, cv.h
        base = H * (0.8 if up > 0 else 0.3)
        cv.rect(W / 2, base, W * 0.48 * (1 - 0.3 * t), H * 0.05 * (1 - t) + 1, 0, hi, alpha=(1 - t) ** 2, soft=4)
        cv.glow(W / 2, base, W * 0.5, col, 0.7 * (1 - t) ** 1.5, sy=0.35)
        for i in range(16):
            x = W * rnd.uniform(0.06, 0.94); sp = rnd.uniform(0.5, 1.0); late = rnd.uniform(0, 0.3)
            tt = max(0.0, (t - late) / (1 - late))
            y = base - up * H * 0.7 * sp * (1 - (1 - tt) ** 2)
            cv.line(x, y, x, y + up * H * 0.16 * (1 - tt), 1.4 * SS, col, alpha=(1 - tt) * (tt > 0))
            if i % 3 == 0:
                cv.star(x, y, W * 0.035 * (1 - tt * 0.5), hi, n=4, k=0.12, p=4, alpha=(1 - tt) * (tt > 0))
    book('fx.gatepass_' + name, 10, 4, 3, 36, True, gp, (0.5, 0.8 if up > 0 else 0.3))

for name, a, b in (('a', (47, 107, 255), (20, 60, 170)), ('b', (255, 200, 58), (200, 140, 20)), ('c', (255, 255, 255), (205, 205, 215)),
                   ('d', (240, 50, 60), (160, 20, 35)), ('e', (155, 77, 255), (95, 35, 180)), ('f', (60, 200, 110), (25, 130, 70))):
    def conf(cv, t, f, a=a, b=b):
        flip = math.cos(TAU * t)
        cv.rect(cv.w / 2, cv.h / 2, cv.w * 0.36, cv.w * (0.04 + 0.22 * abs(flip)), TAU * t * 0.5 + 0.3, a if flip > 0 else b)
    book('fx.confetti_' + name, 8, 0.3, 0.3, 80, False, conf)

# ---- ambient particles, per world (tools/art/worlds.py) -----------------------------------------------------------------
def c3(h):
    return ((h >> 16) & 255, (h >> 8) & 255, h & 255)

def ambient(style, cols):
    a, b = c3(cols[0]), c3(cols[1])
    def leafy(cv, t, f):
        flip = math.cos(TAU * t)
        ang = 0.6 + 0.5 * math.sin(TAU * t)
        c, n = math.cos(ang), math.sin(ang)
        u = ((cv.X - cv.w / 2) * c + (cv.Y - cv.h / 2) * n) / (cv.w * 0.42)
        v = (-(cv.X - cv.w / 2) * n + (cv.Y - cv.h / 2) * c) / (cv.w * (0.05 + 0.17 * abs(flip)))
        shape = 1 - np.abs(u) ** (1.6 if style == 'leaf' else 2.6) - v * v
        cv.paint(np.clip(shape * 6, 0, 1), a if flip > 0 else b)
        if style == 'leaf':
            cv.paint(np.clip(shape * 6, 0, 1) * np.exp(-(v * 5) ** 2) * 0.5, b if flip > 0 else a)
    def flake(cv, t, f):
        k = 0.75 + 0.25 * math.sin(TAU * t)
        cv.disc(cv.w / 2, cv.h / 2, cv.w * 0.26 * k, b, alpha=0.9, soft=cv.w * 0.12)
        cv.disc(cv.w / 2 - cv.w * 0.04, cv.h / 2 - cv.w * 0.04, cv.w * 0.15 * k, a, soft=cv.w * 0.08)
    def wisp(cv, t, f):
        for i in range(3):
            x = cv.w * (0.25 + 0.25 * i) + cv.w * 0.08 * math.sin(TAU * t + i)
            cv.glow(x, cv.h * (0.5 + 0.1 * math.sin(TAU * t + 2 * i)), cv.w * 0.3, a if i != 1 else b, 0.45, sy=0.35)
    def glow(cv, t, f):
        k = 0.6 + 0.4 * math.sin(TAU * t)
        cv.glow(cv.w / 2, cv.h / 2, cv.w * 0.45, a, 0.55 * k)
        cv.glow(cv.w / 2, cv.h / 2, cv.w * 0.16, b, 0.9 * k)
    def fleck(cv, t, f):
        flip = math.cos(TAU * t)
        c = c3(cols[(f // 2) % len(cols)]) if False else a
        cv.rect(cv.w / 2, cv.h / 2, cv.w * 0.36, cv.w * (0.04 + 0.22 * abs(flip)), TAU * t * 0.5 + 0.3, a if flip > 0 else b)
    return {'leaf': (leafy, False, 0.35), 'petal': (leafy, False, 0.3), 'flake': (flake, False, 0.22), 'wisp': (wisp, False, 1.2),
            'glow': (glow, True, 0.45), 'fleck': (fleck, False, 0.3)}[style]

for n, w in sorted(WORLDS.items()):
    for name, style, cols, per_second in w['ambient']:
        draw, add, size = ambient(style, cols)
        book('w%d.%s' % (n, name), 8, size, size * (0.5 if style == 'wisp' else 1.0), 80, add, draw)

# ---- HUD pieces drawn flat (sizes in dp, 3 px per dp; ART_DIRECTION.md section 15) --------------------------------------
from PIL import ImageDraw
def ui_piece(kind, w_dp, h_dp, paint, px_per_dp=3, pivot=(0.5, 0.5)):
    s = px_per_dp * SS
    im = Image.new('RGBA', (int(w_dp * s), int(h_dp * s)), (0, 0, 0, 0))
    paint(ImageDraw.Draw(im), s, im)
    im.save(os.path.join(OUT, '%s.still.0.0.png' % kind))
    with open(os.path.join(OUT, '%s.still.json' % kind), 'w') as fh:
        json.dump({'ppu': s, 'ss': SS, 'frames': 1, 'elevs': {'0': {'pivot': [im.width * pivot[0], im.height * pivot[1]], 'points': {}}}}, fh)
    print('drew', kind)

INK = (22, 34, 79)
def ribbon(name, RIB, RIB_D, RIB_L, FOLD):
    def band(d, s, x0, x1, y0=4, y1=44):
        d.rectangle((x0 * s, y0 * s, x1 * s, y1 * s), fill=INK)
        d.rectangle((x0 * s, (y0 + 2) * s, x1 * s, (y1 - 2) * s), fill=RIB)
        d.rectangle((x0 * s, (y0 + 2) * s, x1 * s, (y0 + 12) * s), fill=RIB_L)
        for y in (y0 + 5, y1 - 7):
            d.rectangle((x0 * s, y * s, x1 * s, (y + 2) * s), fill=(255, 200, 58))
    def cap(left):
        def paint(d, s, im):
            # the folded tail behind (lower, darker, swallow-tailed), then the fold, then the front band's end
            pts = [(0, 14), (30, 14), (30, 56), (0, 56), (10, 35)]
            d.polygon([(x * s, y * s) for x, y in pts], fill=INK)
            d.polygon([((x + (2.6 if x < 12 else 0.0)) * s, (y + (2 if y < 20 else -2)) * s) for x, y in pts], fill=RIB_D)
            d.polygon([(22 * s, 44 * s), (30 * s, 44 * s), (30 * s, 56 * s)], fill=FOLD)
            band(d, s, 22, 44)
            d.rectangle((22 * s, 4 * s, 24 * s, 44 * s), fill=INK)
            if not left:
                im.paste(im.transpose(Image.FLIP_LEFT_RIGHT))
        return paint
    ui_piece(name + '_l', 44, 60, cap(True), pivot=(1.0, 0.4))
    ui_piece(name + '_m', 16, 60, lambda d, s, im: band(d, s, 0, 16), pivot=(0.5, 0.4))
    ui_piece(name + '_r', 44, 60, cap(False), pivot=(0.0, 0.4))
ribbon('ui.ribbon', (31, 88, 242), (20, 64, 196), (74, 128, 255), (12, 40, 130))
ribbon('ui.ribbon_red', (227, 38, 46), (168, 20, 31), (255, 96, 104), (110, 10, 22))
ribbon('ui.ribbon_gold', (240, 160, 20), (200, 120, 10), (255, 200, 80), (140, 80, 0))

def rays(cv, t, f):
    dx, dy = cv.X - cv.w / 2, cv.Y - cv.h / 2
    r = np.hypot(dx, dy) / (cv.w / 2)
    beam = np.clip(np.cos(np.arctan2(dy, dx) * 12) * 3 + 0.5, 0, 1)
    cv.paint(beam * np.clip(1 - r, 0, 1) ** 1.3 * np.clip(r * 6, 0, 1) * 0.6, (255, 255, 255))
book('ui.rays', 1, 260, 260, 1, False, rays)

# The title: two stacked lines of chunky letters (Roboto Black, the game's font): a cobalt face with a light top, a white
# keyline, a thick navy outline and a navy lip under it, gold stars at the sides. 300 x 150 dp.
from PIL import ImageFont, ImageFilter
def logo(d, s, im):
    font_path = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'assets', 'font', 'Roboto-Black.ttf')
    W, H = im.size
    def line(text, cy, size, top, bottom):
        font = ImageFont.truetype(font_path, int(size * s))
        while d.textlength(text, font=font) > W - 30 * s:
            size -= 1; font = ImageFont.truetype(font_path, int(size * s))
        cx = W / 2
        for dy in range(int(7 * s), -1, -2):          # the lip: the outline drawn again, lower
            d.text((cx, cy * s + dy), text, font=font, anchor='mm', fill=(10, 18, 52, 255), stroke_width=int(6.5 * s), stroke_fill=(10, 18, 52, 255))
        d.text((cx, cy * s), text, font=font, anchor='mm', fill=INK + (255,), stroke_width=int(6.5 * s), stroke_fill=INK + (255,))
        d.text((cx, cy * s), text, font=font, anchor='mm', fill=(255, 255, 255, 255), stroke_width=int(2.6 * s), stroke_fill=(255, 255, 255, 255))
        mask = Image.new('L', im.size, 0)
        ImageDraw.Draw(mask).text((cx, cy * s), text, font=font, anchor='mm', fill=255)
        y = np.mgrid[0:H, 0:W][0] / s
        k = np.clip((y - (cy - size * 0.36)) / (size * 0.72), 0, 1)[..., None]
        face = np.array(top, float) * (1 - k) + np.array(bottom, float) * k
        gloss = (y < cy - size * 0.02)[..., None] * 22.0                      # a hard light band over the top half
        im.paste(Image.fromarray(np.clip(face + gloss, 0, 255).astype(np.uint8), 'RGB'), (0, 0), mask)
    line('PAPER', 44, 78, (120, 170, 255), (31, 88, 242))
    line('MARCH', 106, 78, (255, 226, 120), (240, 150, 10))
    for cx, cy, r in ((22, 30, 13), (278, 30, 13), (14, 86, 8), (286, 86, 8)):
        pts = [((cx + (r if i % 2 == 0 else r * 0.45) * math.cos(-math.pi / 2 + math.pi * i / 5)) * s, (cy + (r if i % 2 == 0 else r * 0.45) * math.sin(-math.pi / 2 + math.pi * i / 5)) * s) for i in range(10)]
        d.polygon(pts, fill=(255, 200, 58, 255), outline=INK + (255,), width=int(1.5 * s))
ui_piece('ui.logo', 300, 150, logo)
