"""PAPERMARCH app icon: a gold wisp on the twilight gradient (P6), as Android adaptive icon layers and legacy PNGs.

    python -I -B tools/make_icon.py

Writes android/app/src/main/res/mipmap-*/ (ic_launcher.png, ic_launcher_foreground.png, ic_launcher_background.png),
mipmap-anydpi-v26/ic_launcher.xml, and captures/icon_512.png for review.
"""
import math
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(ROOT, "android", "app", "src", "main", "res")
S = 1024  # working size of a 108 dp layer

SKY = [(0.0, (132, 88, 172)), (0.45, (64, 48, 128)), (1.0, (14, 14, 46))]
WISP, WISP_HI, WISP_RIM = (255, 198, 84), (255, 238, 176), (206, 112, 34)


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def background():
    img = Image.new("RGB", (S, S))
    px = img.load()
    for y in range(S):
        t = y / (S - 1)
        for (p0, c0), (p1, c1) in zip(SKY, SKY[1:]):
            if p0 <= t <= p1:
                c = lerp(c0, c1, (t - p0) / (p1 - p0))
                break
        for x in range(S):
            px[x, y] = c
    glow = Image.new("RGB", (S, S))
    ImageDraw.Draw(glow).ellipse([S * 0.18, S * 0.18, S * 0.82, S * 0.82], fill=(150, 70, 120))
    img = ImageChops.screen(img, glow.filter(ImageFilter.GaussianBlur(S * 0.12)))
    d = ImageDraw.Draw(img, "RGBA")
    rnd = 7
    for i in range(46):  # motes
        rnd = (rnd * 1103515245 + 12345) & 0x7FFFFFFF
        x = (rnd % 1000) / 1000 * S
        rnd = (rnd * 1103515245 + 12345) & 0x7FFFFFFF
        y = (rnd % 1000) / 1000 * S
        r = 3 + (rnd % 7)
        d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 220, 160, 70))
    return img


def wisp_layer():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    cx, cy, r = S * 0.5, S * 0.57, S * 0.17  # inside the 66 dp safe circle
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([cx - r * 1.9, cy - r * 2.2, cx + r * 1.9, cy + r * 1.6], fill=(255, 170, 60, 150))
    img = Image.alpha_composite(img, glow.filter(ImageFilter.GaussianBlur(S * 0.06)))
    d = ImageDraw.Draw(img)
    tip = [(cx - r * 0.70, cy - r * 0.55), (cx + r * 0.18, cy - r * 2.05), (cx + r * 0.70, cy - r * 0.55)]
    rim = r * 0.09
    d.polygon([(tip[0][0] - rim, tip[0][1]), (tip[1][0], tip[1][1] - rim * 1.6), (tip[2][0] + rim, tip[2][1])], fill=WISP_RIM)
    d.ellipse([cx - r - rim, cy - r - rim, cx + r + rim, cy + r + rim], fill=WISP_RIM)
    d.polygon(tip, fill=WISP)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=WISP)
    d.ellipse([cx - r * 0.68, cy - r * 0.74, cx + r * 0.52, cy + r * 0.42], fill=WISP_HI)
    d.ellipse([cx - r * 0.38, cy - r * 0.50, cx + r * 0.18, cy + r * 0.02], fill=(255, 255, 240))
    for ex in (-0.36, 0.36):  # happy eyes: the cheer at the win
        x0, y0 = cx + ex * r, cy + r * 0.18
        d.arc([x0 - r * 0.17, y0 - r * 0.15, x0 + r * 0.17, y0 + r * 0.17], 200, 340, fill=(120, 60, 20), width=int(r * 0.09))
    sparkle = [(0.62, 0.30, 0.035), (0.31, 0.38, 0.022), (0.70, 0.62, 0.02)]
    for sx, sy, sr in sparkle:
        x, y, q = sx * S, sy * S, sr * S
        k = 0.28
        d.polygon([(x, y - q), (x + q * k, y - q * k), (x + q, y), (x + q * k, y + q * k), (x, y + q), (x - q * k, y + q * k),
                   (x - q, y), (x - q * k, y - q * k)], fill=(255, 245, 215))
    return img


def main():
    bg = background()
    fg = wisp_layer()
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
