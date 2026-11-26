# PAPERMARCH concept frame: the look target for phase P1 (docs/PLAN.md, GAME_DESIGN 9).
# Run from the repository root:  python -I -B docs/concept/make_concept.py
# Writes concept_gameplay.png (1080 x 2400) and concept_gameplay.small.png beside this file.
# Coordinates are in a 1080 x 2400 frame (a tall phone, portrait); S is the supersampling factor.
import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

S = 2
W, H = 1080, 2400
HERE = os.path.dirname(os.path.abspath(__file__))
rng = random.Random(7)

# World 1, "Greenmeadow". The player's host is ivory with scarlet and gold; the enemy is ink navy with the
# world's accent (orange here). GAME_DESIGN 9.2 holds the same numbers.
PAL = dict(
    table_a=(0x70, 0x48, 0x2C), table_b=(0x45, 0x2A, 0x1B),
    page=(0xFF, 0xF4, 0xD6), land=(0x74, 0xC6, 0x58), land2=(0x96, 0xD8, 0x64), land3=(0xBD, 0xEA, 0x7C),
    forest=(0x23, 0x96, 0x5C), forest_l=(0x46, 0xBE, 0x74), blossom=(0xFF, 0x86, 0xAE), blossom_l=(0xFF, 0xB9, 0xD0),
    river=(0x24, 0xBE, 0xDA), river_l=(0x9A, 0xEC, 0xF4), road=(0xF9, 0xE4, 0xA8), stitch=(0xC8, 0x96, 0x4E),
    ivory=(0xFF, 0xFB, 0xEE), ivory_d=(0xE9, 0xDC, 0xC2), scarlet=(0xF0, 0x3A, 0x4C), scarlet_d=(0xB8, 0x1F, 0x3A),
    gold=(0xFF, 0xC8, 0x3A), gold_d=(0xE0, 0x90, 0x10), ink=(0x29, 0x2C, 0x55), ink_l=(0x47, 0x4D, 0x8C),
    accent=(0xFF, 0x7A, 0x2A), text=(0x33, 0x22, 0x1A), violet=(0x8B, 0x5C, 0xF2), violet_l=(0xB7, 0x9A, 0xFF),
    mint=(0xEA, 0xFF, 0xDC), green_t=(0x17, 0x82, 0x3A), white=(0xFF, 0xFF, 0xFF),
)


def p(v):
    return int(round(v * S))


def font(size):
    for name in ("ariblk.ttf", "seguibl.ttf", "arialbd.ttf"):
        try:
            return ImageFont.truetype(os.path.join(r"C:\Windows\Fonts", name), p(size))
        except OSError:
            pass
    return ImageFont.load_default()


def layer():
    return Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))


def drop(base, lay, dx=5, dy=9, blur=7, alpha=105):
    """Composite a cut-paper layer over base with the soft shadow it throws on what lies under it."""
    a = lay.getchannel("A").point(lambda v: v * alpha // 255)
    m = Image.new("L", lay.size, 0)
    m.paste(a, (p(dx), p(dy)))
    m = m.filter(ImageFilter.GaussianBlur(p(blur)))
    sh = Image.new("RGBA", lay.size, (38, 20, 10, 255))
    sh.putalpha(m)
    base.alpha_composite(sh)
    base.alpha_composite(lay)


def blob(cx, cy, rx, ry, seed, wob=0.12, n=120, sq=1.0):
    r = random.Random(seed)
    ph = [r.uniform(0, 6.283) for _ in range(3)]
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        k = 1 + wob * (0.5 * math.sin(3 * t + ph[0]) + 0.3 * math.sin(5 * t + ph[1]) + 0.2 * math.sin(8 * t + ph[2]))
        c, s_ = math.cos(t), math.sin(t)  # sq < 1 squares the outline (a superellipse)
        pts.append((p(cx + rx * k * math.copysign(abs(c) ** sq, c)), p(cy + ry * k * math.copysign(abs(s_) ** sq, s_))))
    return pts


def bezier(a, b, bend, n=40):
    """A road from a to b, bowed sideways by `bend` px."""
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    dx, dy = b[0] - a[0], b[1] - a[1]
    ln = math.hypot(dx, dy) or 1
    cx, cy = mx - dy / ln * bend, my + dx / ln * bend
    out = []
    for i in range(n + 1):
        t = i / n
        out.append(((1 - t) ** 2 * a[0] + 2 * (1 - t) * t * cx + t * t * b[0],
                    (1 - t) ** 2 * a[1] + 2 * (1 - t) * t * cy + t * t * b[1]))
    return out


def star(d, cx, cy, r, fill):
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        rr = r if i % 2 == 0 else r * 0.45
        pts.append((p(cx + rr * math.cos(a)), p(cy + rr * math.sin(a))))
    d.polygon(pts, fill=fill)


# ---------------------------------------------------------------- the level in the frame (GAME_DESIGN 2)
SITES = {
    "home": (540, 1985), "a": (285, 1790), "b": (795, 1765), "c": (265, 1400), "d": (805, 1440),
    "e": (540, 1175), "f": (235, 985), "g": (845, 960), "i": (540, 850), "k": (540, 600),
}
ROADS = [("home", "a", 30), ("home", "b", -30), ("a", "c", -40), ("b", "d", 36), ("c", "e", 40), ("d", "e", -40),
         ("e", "f", 44), ("e", "g", -44), ("f", "i", 34), ("g", "i", -34), ("i", "k", 0)]


def river_y(x):
    return 1180 + 46 * math.sin(x / 150.0) + 22 * math.sin(x / 61.0 + 1.3)


def pawn(d, x, y, s, body, trim, dark):
    d.ellipse([p(x - 9 * s), p(y - 22 * s), p(x + 9 * s), p(y + 3 * s)], fill=body)
    d.pieslice([p(x - 9 * s), p(y - 22 * s), p(x + 9 * s), p(y + 3 * s)], 300, 80, fill=dark)  # the fold's shaded half
    d.ellipse([p(x - 7.5 * s), p(y - 35 * s), p(x + 7.5 * s), p(y - 20 * s)], fill=body)
    d.rounded_rectangle([p(x - 8 * s), p(y - 22.5 * s), p(x + 8 * s), p(y - 17.5 * s)], p(2.5 * s), fill=trim)


def tree(d, x, y, s, dark, light):
    d.rectangle([p(x - 4 * s), p(y - 12 * s), p(x + 4 * s), p(y)], fill=(0x8A, 0x5A, 0x36))
    for k in range(2):  # two folded cones
        top, wid, base = y - (78 - 26 * k) * s, (20 + 8 * k) * s, y - (34 - 24 * k) * s
        d.polygon([(p(x), p(top)), (p(x - wid), p(base)), (p(x), p(base))], fill=light)
        d.polygon([(p(x), p(top)), (p(x + wid), p(base)), (p(x), p(base))], fill=dark)


def flag(d, x, y, h, w, col, trim):
    d.rectangle([p(x - 2.5), p(y - h), p(x + 2.5), p(y)], fill=(0x6B, 0x45, 0x2B))
    d.polygon([(p(x + 2), p(y - h)), (p(x + w), p(y - h + w * 0.20)), (p(x + w * 0.78), p(y - h + w * 0.36)),
               (p(x + w), p(y - h + w * 0.56)), (p(x + 2), p(y - h + w * 0.62))], fill=col)
    d.line([(p(x + 2), p(y - h + w * 0.31)), (p(x + w * 0.74), p(y - h + w * 0.36))], fill=trim, width=p(4))


def tent(d, x, y, s, dark, light, accent):
    d.polygon([(p(x), p(y - 62 * s)), (p(x - 42 * s), p(y)), (p(x), p(y))], fill=light)
    d.polygon([(p(x), p(y - 62 * s)), (p(x + 42 * s), p(y)), (p(x), p(y))], fill=dark)
    d.polygon([(p(x), p(y - 30 * s)), (p(x - 9 * s), p(y)), (p(x + 9 * s), p(y))], fill=(0x14, 0x15, 0x2C))
    d.polygon([(p(x), p(y - 84 * s)), (p(x + 24 * s), p(y - 75 * s)), (p(x), p(y - 66 * s))], fill=accent)
    d.rectangle([p(x - 1.5 * s), p(y - 84 * s), p(x + 1.5 * s), p(y - 60 * s)], fill=(0x6B, 0x45, 0x2B))


def house(d, x, y, s, wall, wall_d, roof, roof_d):
    d.rectangle([p(x - 24 * s), p(y - 30 * s), p(x), p(y)], fill=wall)
    d.rectangle([p(x), p(y - 30 * s), p(x + 24 * s), p(y)], fill=wall_d)
    d.polygon([(p(x), p(y - 62 * s)), (p(x - 32 * s), p(y - 28 * s)), (p(x), p(y - 28 * s))], fill=roof)
    d.polygon([(p(x), p(y - 62 * s)), (p(x + 32 * s), p(y - 28 * s)), (p(x), p(y - 28 * s))], fill=roof_d)
    d.rectangle([p(x - 15 * s), p(y - 18 * s), p(x - 7 * s), p(y)], fill=(0x8A, 0x5A, 0x36))


def tower(d, x, y, w, h, dark, light, roof, roof_d, crenel=True):
    d.rectangle([p(x - w / 2), p(y - h), p(x), p(y)], fill=light)
    d.rectangle([p(x), p(y - h), p(x + w / 2), p(y)], fill=dark)
    if crenel:
        n = max(3, int(w // 22) | 1)
        cw = w / n
        for k in range(0, n, 2):
            x0 = x - w / 2 + k * cw
            d.rectangle([p(x0), p(y - h - 16), p(x0 + cw), p(y - h)], fill=light if x0 + cw / 2 < x else dark)
    if roof:
        d.polygon([(p(x), p(y - h - 86)), (p(x - w / 2 - 8), p(y - h - 12)), (p(x), p(y - h - 12))], fill=roof)
        d.polygon([(p(x), p(y - h - 86)), (p(x + w / 2 + 8), p(y - h - 12)), (p(x), p(y - h - 12))], fill=roof_d)


def tag(d, x, y, text, bg, fg, size=46, edge=None, lock=False, sub=None):
    f = font(size)
    tw = d.textlength(text, font=f) / S
    w = max(86, tw + 44 + (30 if lock else 0))
    h = size * 1.42
    d.polygon([(p(x - 13), p(y + h / 2 - 2)), (p(x + 13), p(y + h / 2 - 2)), (p(x), p(y + h / 2 + 20))], fill=edge or bg)
    d.rounded_rectangle([p(x - w / 2), p(y - h / 2), p(x + w / 2), p(y + h / 2)], p(h * 0.36), fill=bg,
                        outline=edge, width=p(5) if edge else 0)
    tx = x + (15 if lock else 0)
    d.text((p(tx), p(y - 2)), text, font=f, fill=fg, anchor="mm")
    if lock:
        lx, ly = x - w / 2 + 27, y + 3
        d.arc([p(lx - 9), p(ly - 22), p(lx + 9), p(ly + 2)], 180, 360, fill=fg, width=p(4.5))
        d.rounded_rectangle([p(lx - 12), p(ly - 9), p(lx + 12), p(ly + 11)], p(4), fill=fg)
    if sub:
        fs = font(22)
        sw = d.textlength(sub, font=fs) / S + 22
        d.rounded_rectangle([p(x + w / 2 - 20), p(y - h / 2 - 20), p(x + w / 2 - 20 + sw), p(y - h / 2 + 14)], p(15), fill=PAL["accent"])
        d.text((p(x + w / 2 - 20 + sw / 2), p(y - h / 2 - 4)), sub, font=fs, fill=PAL["white"], anchor="mm")


def main():
    img = Image.new("RGBA", (W * S, H * S), PAL["table_b"] + (255,))
    d = ImageDraw.Draw(img)

    # The table: warm wood, lighter in the middle, with plank lines.
    for y in range(0, H * S, 4):
        t = abs(y / (H * S) - 0.45) * 1.6
        col = tuple(int(PAL["table_a"][k] * (1 - t) + PAL["table_b"][k] * t) for k in range(3))
        d.rectangle([0, y, W * S, y + 4], fill=col)
    for x in range(0, W, 180):
        d.line([(p(x + 20), 0), (p(x + 20), H * S)], fill=(0x3C, 0x24, 0x17), width=p(2.5))

    # The page, then the land glued on it.
    lay = layer()
    ImageDraw.Draw(lay).rounded_rectangle([p(30), p(250), p(1050), p(2135)], p(34), fill=PAL["page"])
    drop(img, lay, 0, 16, 18, 150)
    lay = layer()
    ImageDraw.Draw(lay).polygon(blob(540, 1192, 486, 918, 3, wob=0.03, n=320, sq=0.62), fill=PAL["land"])
    drop(img, lay, 2, 4, 4, 70)

    # Two terraces of lighter paper.
    lay = layer()
    ld = ImageDraw.Draw(lay)
    for cx, cy, rx, ry, sd in [(300, 1560, 235, 300, 11), (800, 1590, 215, 290, 12), (540, 850, 330, 250, 13),
                               (540, 1965, 320, 130, 14), (240, 985, 150, 170, 15), (845, 960, 150, 170, 16)]:
        ld.polygon(blob(cx, cy, rx, ry, sd, wob=0.16), fill=PAL["land2"])
    drop(img, lay, 3, 6, 6, 80)
    lay = layer()
    ld = ImageDraw.Draw(lay)
    for cx, cy, rx, ry, sd in [(275, 1420, 150, 120, 21), (805, 1450, 140, 115, 22), (540, 700, 250, 200, 23),
                               (285, 1795, 120, 85, 24), (795, 1770, 120, 85, 25), (540, 1990, 180, 80, 26)]:
        ld.polygon(blob(cx, cy, rx, ry, sd, wob=0.14), fill=PAL["land3"])
    drop(img, lay, 3, 6, 6, 80)

    # The river: cut out of the land (a dark upper edge instead of a shadow under it), with paper ripples.
    pts = [(x, river_y(x)) for x in range(66, 1018, 6)]
    lay = layer()
    ld = ImageDraw.Draw(lay)
    ld.line([(p(x), p(y - 7)) for x, y in pts], fill=(0x10, 0x6E, 0x8C), width=p(96), joint="curve")
    ld.line([(p(x), p(y)) for x, y in pts], fill=PAL["river"], width=p(88), joint="curve")
    for k in range(26):
        x = rng.uniform(100, 980)
        y = river_y(x) + rng.uniform(-28, 28)
        ld.line([(p(x - 22), p(y)), (p(x + 22), p(y))], fill=PAL["river_l"], width=p(5))
    land_mask = layer()
    ImageDraw.Draw(land_mask).polygon(blob(540, 1192, 486, 918, 3, wob=0.03, n=320, sq=0.62), fill=(255, 255, 255, 255))
    lay.putalpha(Image.composite(lay.getchannel("A"), Image.new("L", lay.size, 0), land_mask.getchannel("A")))
    img.alpha_composite(lay)

    # Roads: sand-coloured strips with stitches.
    lay = layer()
    ld = ImageDraw.Draw(lay)
    paths = {}
    for a, b, bend in ROADS:
        path = bezier(SITES[a], SITES[b], bend)
        paths[(a, b)] = path
        ld.line([(p(x), p(y)) for x, y in path], fill=PAL["road"], width=p(30), joint="curve")
    for x, y in SITES.values():
        ld.ellipse([p(x - 62), p(y - 40), p(x + 62), p(y + 40)], fill=PAL["road"])
    for path in paths.values():
        for k in range(2, len(path) - 2, 3):
            ld.line([(p(path[k][0]), p(path[k][1])), (p(path[k + 1][0]), p(path[k + 1][1]))], fill=PAL["stitch"], width=p(4))
    drop(img, lay, 2, 4, 3, 85)

    # Pop-ups, far to near: the bridge, trees, tents, houses, the shrine, the keep.
    props = []
    ex, ey = SITES["e"]

    def bridge(dd):
        dd.rounded_rectangle([p(ex - 46), p(ey - 78), p(ex + 46), p(ey + 78)], p(10), fill=(0xD9, 0xA8, 0x62))
        for k in range(-3, 4):
            dd.line([(p(ex - 44), p(ey + k * 22)), (p(ex + 44), p(ey + k * 22))], fill=(0xA8, 0x74, 0x3A), width=p(3))
        for sx in (-58, 58):
            tower(dd, ex + sx, ey + 30, 34, 70, PAL["ink"], PAL["ink_l"], PAL["accent"], (0xD9, 0x5A, 0x12), crenel=False)
    props.append((ey - 200, bridge))

    clusters = [(118, 1290), (965, 1300), (112, 1640), (968, 1660), (120, 720), (962, 700), (150, 420), (930, 420),
                (150, 2010), (930, 2010), (300, 640), (780, 640), (540, 1560), (130, 1100), (955, 1095)]
    for ci, (cx, cy) in enumerate(clusters):
        for k in range(rng.randint(3, 6)):
            x, y = cx + rng.uniform(-52, 52), cy + rng.uniform(-60, 60)
            pink = (ci + k) % 5 == 0
            s = rng.uniform(0.85, 1.2)
            props.append((y, lambda dd, x=x, y=y, s=s, pink=pink: tree(
                dd, x, y, s, PAL["blossom"] if pink else PAL["forest"], PAL["blossom_l"] if pink else PAL["forest_l"])))

    for name in ("c", "f", "g", "i"):
        x, y = SITES[name]
        props.append((y - 30, lambda dd, x=x, y=y: tent(dd, x - 46, y - 34, 1.0, PAL["ink"], PAL["ink_l"], PAL["accent"])))
        props.append((y - 10, lambda dd, x=x, y=y: tent(dd, x + 44, y - 14, 0.85, PAL["ink"], PAL["ink_l"], PAL["accent"])))
    x, y = SITES["a"]
    props.append((y - 30, lambda dd, x=x, y=y: tent(dd, x - 40, y - 30, 1.0, PAL["scarlet_d"], PAL["scarlet"], PAL["gold"])))
    props.append((y - 20, lambda dd, x=x, y=y: flag(dd, x + 34, y - 20, 118, 62, PAL["scarlet"], PAL["gold"])))
    x, y = SITES["b"]
    props.append((y - 36, lambda dd, x=x, y=y: house(dd, x - 34, y - 30, 1.0, PAL["ivory"], PAL["ivory_d"], PAL["accent"], (0xD9, 0x5A, 0x12))))
    props.append((y - 12, lambda dd, x=x, y=y: house(dd, x + 34, y - 8, 0.85, PAL["ivory"], PAL["ivory_d"], PAL["gold"], PAL["gold_d"])))
    props.append((y - 30, lambda dd, x=x, y=y: flag(dd, x + 2, y - 44, 122, 60, PAL["scarlet"], PAL["gold"])))
    x, y = SITES["home"]
    props.append((y - 24, lambda dd, x=x, y=y: tent(dd, x - 70, y - 20, 1.1, PAL["scarlet_d"], PAL["scarlet"], PAL["gold"])))
    props.append((y - 24, lambda dd, x=x, y=y: tent(dd, x + 70, y - 20, 1.1, PAL["scarlet_d"], PAL["scarlet"], PAL["gold"])))

    x, y = SITES["d"]

    def shrine(dd, x=x, y=y):
        dd.polygon([(p(x - 30), p(y - 12)), (p(x - 14), p(y - 150)), (p(x), p(y - 172)), (p(x), p(y - 12))], fill=PAL["violet_l"])
        dd.polygon([(p(x + 30), p(y - 12)), (p(x + 14), p(y - 150)), (p(x), p(y - 172)), (p(x), p(y - 12))], fill=PAL["violet"])
        dd.rounded_rectangle([p(x - 48), p(y - 20), p(x + 48), p(y + 2)], p(6), fill=PAL["gold"])
        star(dd, x, y - 104, 19, PAL["gold"])
    props.append((y, shrine))

    kx, ky = SITES["k"]

    def keep(dd):
        tower(dd, kx, ky - 10, 330, 120, PAL["ink"], PAL["ink_l"], None, None)
        tower(dd, kx - 150, ky + 10, 78, 190, PAL["ink"], PAL["ink_l"], PAL["accent"], (0xD9, 0x5A, 0x12))
        tower(dd, kx + 150, ky + 10, 78, 190, PAL["ink"], PAL["ink_l"], PAL["accent"], (0xD9, 0x5A, 0x12))
        tower(dd, kx, ky + 22, 124, 236, PAL["ink"], PAL["ink_l"], PAL["scarlet"] and PAL["accent"], (0xD9, 0x5A, 0x12))
        dd.pieslice([p(kx - 30), p(ky - 64), p(kx + 30), p(ky - 4)], 180, 360, fill=(0x12, 0x13, 0x2A))
        dd.rectangle([p(kx - 30), p(ky - 34), p(kx + 30), p(ky + 22)], fill=(0x12, 0x13, 0x2A))
        for wx in (-150, 150):
            dd.rounded_rectangle([p(kx + wx - 9), p(ky - 130), p(kx + wx + 9), p(ky - 96)], p(8), fill=PAL["gold"])
        dd.rounded_rectangle([p(kx - 11), p(ky - 170), p(kx + 11), p(ky - 128)], p(9), fill=PAL["gold"])
        flag(dd, kx, ky - 296, 74, 58, PAL["accent"], PAL["gold"])
    props.append((ky + 22, keep))

    lay = layer()
    ld = ImageDraw.Draw(lay)
    for _, fn in sorted(props, key=lambda t: t[0]):
        fn(ld)
    drop(img, lay, 9, 15, 7, 120)

    # Pawns: the enemy's garrisons, then the player's host on the march from A toward C.
    pawns = []
    for name, n, spread in (("c", 5, 62), ("f", 9, 74), ("g", 12, 80), ("i", 7, 66), ("k", 18, 150)):
        x, y = SITES[name]
        for k in range(n):
            a = 2 * math.pi * k / n + 0.4
            rx = spread * (0.75 + 0.25 * ((k * 7) % 3) / 2)
            px_, py_ = x + rx * math.cos(a), y + 26 + 0.42 * rx * math.sin(a)
            pawns.append((py_, px_, 1.0, PAL["ink_l"], PAL["accent"], PAL["ink"]))
    march = bezier(SITES["a"], SITES["c"], -40, n=200)
    head = 70  # how far along the road the banner is
    for row in range(8):
        k = head - 9 - row * 11
        if k < 1:
            break
        x, y = march[k]
        nx, ny = march[k + 1][0] - march[k - 1][0], march[k + 1][1] - march[k - 1][1]
        ln = math.hypot(nx, ny) or 1
        for col in (-1, 0, 1):
            jx, jy = rng.uniform(-3, 3), rng.uniform(-3, 3)
            pawns.append((y + jy + col * nx / ln * 2, x - ny / ln * col * 21 + jx, 1.0, PAL["ivory"], PAL["scarlet"], PAL["ivory_d"]))
    hx, hy = march[head]
    pawns.append((hy, hx, 1.25, PAL["ivory"], PAL["gold"], PAL["ivory_d"]))
    x, y = SITES["home"]
    for k in range(3):
        pawns.append((y + 30, x - 26 + k * 26, 1.0, PAL["ivory"], PAL["scarlet"], PAL["ivory_d"]))
    lay = layer()
    ld = ImageDraw.Draw(lay)
    for py_, px_, s, body, trim, dark in sorted(pawns):
        pawn(ld, px_, py_, s, body, trim, dark)
    flag(ld, hx + 13, hy - 6, 150, 84, PAL["scarlet"], PAL["gold"])
    drop(img, lay, 4, 7, 3, 120)

    # Confetti left by the two wins, and the dotted "next" arrow from the host to the camp it can take.
    lay = layer()
    ld = ImageDraw.Draw(lay)
    for name in ("a", "b"):
        x, y = SITES[name]
        for k in range(26):
            cx, cy, a = x + rng.uniform(-120, 120), y + rng.uniform(-70, 90), rng.uniform(0, 3.14)
            w2, h2 = 9, 4
            cs, sn = math.cos(a), math.sin(a)
            quad = [(p(cx + cs * sx * w2 - sn * sy * h2), p(cy + sn * sx * w2 + cs * sy * h2)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
            ld.polygon(quad, fill=rng.choice([PAL["scarlet"], PAL["gold"], PAL["ivory"], PAL["blossom"]]))
    for k in range(head + 14, 178, 12):
        ld.ellipse([p(march[k][0] - 7), p(march[k][1] - 7), p(march[k][0] + 7), p(march[k][1] + 7)], fill=PAL["gold"])
    drop(img, lay, 2, 3, 2, 90)

    # Number tags. Gold: the host can take it now. Ink with a lock: too strong for now.
    lay = layer()
    ld = ImageDraw.Draw(lay)
    tag(ld, SITES["c"][0], SITES["c"][1] - 150, "14", PAL["gold"], PAL["text"], edge=PAL["white"])
    tag(ld, SITES["d"][0], SITES["d"][1] - 232, "×2", PAL["violet"], PAL["white"], edge=PAL["white"])
    tag(ld, SITES["e"][0] + 150, SITES["e"][1] - 46, "-5", PAL["ivory"], PAL["accent"], size=40)
    tag(ld, SITES["f"][0], SITES["f"][1] - 150, "30", PAL["ink"], PAL["ivory"], lock=True)
    tag(ld, SITES["g"][0], SITES["g"][1] - 150, "45", PAL["ink"], PAL["ivory"], lock=True)
    tag(ld, SITES["i"][0], SITES["i"][1] - 150, "18", PAL["ink"], PAL["ivory"], lock=True, sub="+3")
    tag(ld, kx + 238, ky - 268, "120", PAL["ink"], PAL["gold"], size=64, edge=PAL["gold"], lock=True)
    tag(ld, hx - 10, hy - 196, "24", PAL["ivory"], PAL["scarlet"], size=58, edge=PAL["scarlet"])
    drop(img, lay, 4, 8, 5, 125)

    # HUD (GAME_DESIGN 11): level chip and pause above the page; UNDO, the host plate and HINT under it.
    lay = layer()
    ld = ImageDraw.Draw(lay)
    ld.rounded_rectangle([p(44), p(96), p(330), p(196)], p(50), fill=PAL["ivory"])
    ld.text((p(187), p(144)), "LEVEL 12", font=font(40), fill=PAL["text"], anchor="mm")
    for k in range(3):
        star(ld, 392 + k * 52, 146, 22, PAL["gold"] if k < 2 else (0x8A, 0x62, 0x44))
    ld.ellipse([p(936), p(96), p(1036), p(196)], fill=PAL["ivory"])
    ld.rounded_rectangle([p(968), p(124), p(980), p(168)], p(4), fill=PAL["text"])
    ld.rounded_rectangle([p(992), p(124), p(1004), p(168)], p(4), fill=PAL["text"])
    ld.rounded_rectangle([p(590), p(106), p(900), p(186)], p(40), fill=PAL["ink"])
    ld.text((p(745), p(144)), "TAKE THE KEEP", font=font(27), fill=PAL["gold"], anchor="mm")
    ld.rounded_rectangle([p(44), p(2196), p(280), p(2330)], p(40), fill=PAL["ivory"])
    ld.text((p(162), p(2261)), "UNDO", font=font(40), fill=PAL["text"], anchor="mm")
    ld.rounded_rectangle([p(800), p(2196), p(1036), p(2330)], p(40), fill=PAL["gold"])
    ld.text((p(918), p(2261)), "HINT", font=font(40), fill=PAL["text"], anchor="mm")
    ld.rounded_rectangle([p(330), p(2170), p(750), p(2350)], p(54), fill=PAL["ivory"], outline=PAL["scarlet"], width=p(8))
    ld.text((p(540), p(2212)), "YOUR HOST", font=font(26), fill=PAL["text"], anchor="mm")
    ld.text((p(540), p(2284)), "24", font=font(88), fill=PAL["scarlet"], anchor="mm")
    drop(img, lay, 0, 8, 6, 130)

    out = img.convert("RGB").resize((W, H), Image.LANCZOS)
    # Paper grain and a soft vignette.
    arr = np.asarray(out).astype(np.float32)
    g = np.random.default_rng(5).normal(0, 3.2, (H, W, 1)).astype(np.float32)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    vig = 1.0 - 0.20 * (((xx - W / 2) / (W * 0.72)) ** 2 + ((yy - H / 2) / (H * 0.62)) ** 2)[..., None]
    out = Image.fromarray(np.clip((arr + g) * vig, 0, 255).astype(np.uint8))
    out.save(os.path.join(HERE, "concept_gameplay.png"))
    out.resize((W // 2, H // 2), Image.LANCZOS).save(os.path.join(HERE, "concept_gameplay.small.png"))
    print("wrote concept_gameplay.png", out.size)


if __name__ == "__main__":
    main()
