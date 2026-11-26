"""PAPERMARCH app icon: a blue paper soldier on the stone road, the red horde far up it, as Android adaptive icon layers
and legacy PNGs.

    python -I -B tools/make_icon.py

Writes android/app/src/main/res/mipmap-*/ (ic_launcher.png, ic_launcher_foreground.png, ic_launcher_background.png),
mipmap-anydpi-v26/ic_launcher.xml, and captures/icon_512.png for review.
"""
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(ROOT, "android", "app", "src", "main", "res")
S = 1024  # working size of a 108 dp layer

GRASS_A, GRASS_B = (134, 201, 90), (98, 174, 70)
STONE_A, STONE_B, MORTAR, CURB = (214, 202, 180), (176, 160, 136), (118, 103, 79), (238, 228, 208)
BLUE, BLUE_DARK, BLUE_DEEP = (59, 140, 255), (33, 96, 216), (22, 56, 138)
RED, RED_DARK = (232, 52, 60), (168, 27, 44)
GOLD, GUN, SKIN_SHADOW = (242, 194, 48), (58, 51, 68), (30, 24, 16)


def road_x(y, half_bottom, half_top):
    """The road's half width at row y (perspective: narrow at the top)."""
    t = y / S
    return half_top + (half_bottom - half_top) * t ** 1.4


def background():
    img = Image.new("RGB", (S, S))
    px = img.load()
    for y in range(S):
        t = y / (S - 1)
        c = tuple(int(GRASS_A[i] + (GRASS_B[i] - GRASS_A[i]) * t) for i in range(3))
        for x in range(S):
            px[x, y] = c
    d = ImageDraw.Draw(img)
    hb, ht = S * 0.46, S * 0.09
    # The curbs, then the road.
    d.polygon([(S / 2 - ht * 1.25, 0), (S / 2 + ht * 1.25, 0), (S / 2 + hb * 1.12, S), (S / 2 - hb * 1.12, S)], fill=CURB)
    d.polygon([(S / 2 - ht, 0), (S / 2 + ht, 0), (S / 2 + hb, S), (S / 2 - hb, S)], fill=STONE_A)
    # Paving rows: closer together toward the top, every other row shifted.
    y, row, rh = S, 0, S * 0.085
    while y > 0:
        y1 = y - rh
        h0, h1 = road_x(y, hb, ht), road_x(max(y1, 0), hb, ht)
        n = 5
        for k in range(n + 1):
            u = (k + (0.5 if row % 2 else 0.0)) / n
            if u > 1:
                continue
            xa, xb = S / 2 - h0 + 2 * h0 * u, S / 2 - h1 + 2 * h1 * u
            d.line([(xa, y), (xb, y1)], fill=MORTAR, width=max(2, int(rh * 0.06)))
        shade = STONE_B if row % 3 == 1 else STONE_A
        if row % 3 == 1:
            d.polygon([(S / 2 - h0 * 0.55, y), (S / 2 - h1 * 0.55, y1), (S / 2 - h1 * 0.15, y1), (S / 2 - h0 * 0.15, y)], fill=shade)
        d.line([(S / 2 - h0, y), (S / 2 + h0, y)], fill=MORTAR, width=max(2, int(rh * 0.07)))
        y, row, rh = y1, row + 1, max(rh * 0.82, S * 0.02)
    # The horde, far up the road: small red figures.
    for i in range(14):
        col, rk = i % 7, i // 7
        x = S / 2 - S * 0.075 + col * S * 0.025 + (rk % 2) * S * 0.012
        yy = S * 0.13 + rk * S * 0.028
        r = S * 0.011
        d.ellipse([x - r, yy - r * 2.6, x + r, yy], fill=RED)
        d.ellipse([x - r * 0.8, yy - r * 3.6, x + r * 0.8, yy - r * 2.0], fill=RED_DARK)
    return img


def half_dark(img, box, light, dark, shape):
    """A paper shape: the left side `light`, the right side `dark` (the fold)."""
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    m = Image.new("L", (S, S), 0)
    md = ImageDraw.Draw(m)
    if shape == "ellipse":
        md.ellipse(box, fill=255)
    else:
        md.rounded_rectangle(box, radius=int((box[2] - box[0]) * 0.3), fill=255)
    cx = (box[0] + box[2]) / 2 + (box[2] - box[0]) * 0.06
    left = Image.new("RGBA", (S, S), light + (255,))
    right = Image.new("RGBA", (S, S), dark + (255,))
    side = Image.new("L", (S, S), 0)
    ImageDraw.Draw(side).rectangle([cx, 0, S, S], fill=255)
    layer = Image.composite(right, left, side)
    layer.putalpha(m)
    return Image.alpha_composite(img, layer)


def soldier_layer():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    cx, foot = S * 0.5, S * 0.72          # inside the 66 dp safe circle
    hw, hh = S * 0.15, S * 0.25           # the pawn's half size
    cy = foot - hh
    # A soft shadow at the feet.
    sh = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse([cx - hw * 1.2, foot - hh * 0.1, cx + hw * 1.45, foot + hh * 0.16], fill=SKIN_SHADOW + (110,))
    img = Image.alpha_composite(img, sh.filter(ImageFilter.GaussianBlur(S * 0.012)))
    # The gun behind the right shoulder, pointing up the road; a muzzle flash.
    d = ImageDraw.Draw(img)
    gx = cx + hw * 0.85
    d.rounded_rectangle([gx - S * 0.017, cy - hh * 1.25, gx + S * 0.017, cy + hh * 0.1], radius=int(S * 0.017), fill=GUN)
    fx, fy, q = gx, cy - hh * 1.32, S * 0.06
    k = 0.3
    d.polygon([(fx, fy - q), (fx + q * k, fy - q * k), (fx + q, fy), (fx + q * k, fy + q * k), (fx, fy + q), (fx - q * k, fy + q * k),
               (fx - q, fy), (fx - q * k, fy - q * k)], fill=(255, 242, 160))
    # The body (a bell) and the head, folded paper.
    img = half_dark(img, [cx - hw, cy - hh * 0.32, cx + hw, foot], BLUE, BLUE_DARK, "ellipse")
    head_r = hh * 0.40
    hy = cy - hh * 0.58
    img = half_dark(img, [cx - head_r, hy - head_r, cx + head_r, hy + head_r], BLUE, BLUE_DARK, "ellipse")
    d = ImageDraw.Draw(img)
    # The gold belt at the neck and the helmet.
    d.rectangle([cx - hw * 0.62, cy - hh * 0.30, cx + hw * 0.62, cy - hh * 0.16], fill=GOLD)
    img = half_dark(img, [cx - head_r * 1.08, hy - head_r * 1.15, cx + head_r * 1.08, hy + head_r * 0.15], BLUE_DEEP, (14, 38, 96), "ellipse")
    return img


def main():
    bg = background()
    fg = soldier_layer()
    full = Image.alpha_composite(bg.convert("RGBA"), fg)
    os.makedirs(os.path.join(ROOT, "captures"), exist_ok=True)
    full.convert("RGB").resize((512, 512), Image.LANCZOS).save(os.path.join(ROOT, "captures", "icon_512.png"))
    dens = {"mdpi": 1.0, "hdpi": 1.5, "xhdpi": 2.0, "xxhdpi": 3.0, "xxxhdpi": 4.0}
    # Legacy icon: the middle 72 of 108 dp, rounded square.
    crop = full.crop((int(S * 18 / 108), int(S * 18 / 108), int(S * 90 / 108), int(S * 90 / 108)))
    mask = Image.new("L", crop.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, crop.size[0] - 1, crop.size[1] - 1], radius=crop.size[0] // 5, fill=255)
    crop.putalpha(mask)
    for name, k in dens.items():
        out = os.path.join(RES, "mipmap-" + name)
        os.makedirs(out, exist_ok=True)
        layer = int(round(108 * k))
        bg.resize((layer, layer), Image.LANCZOS).save(os.path.join(out, "ic_launcher_background.png"))
        fg.resize((layer, layer), Image.LANCZOS).save(os.path.join(out, "ic_launcher_foreground.png"))
        crop.resize((int(round(48 * k)), int(round(48 * k))), Image.LANCZOS).save(os.path.join(out, "ic_launcher.png"))
    any_dir = os.path.join(RES, "mipmap-anydpi-v26")
    os.makedirs(any_dir, exist_ok=True)
    with open(os.path.join(any_dir, "ic_launcher.xml"), "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n'
                '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
                '    <background android:drawable="@mipmap/ic_launcher_background" />\n'
                '    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n'
                '</adaptive-icon>\n')
    print("icon: written to", RES)


if __name__ == "__main__":
    main()
