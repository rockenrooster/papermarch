"""Contact sheets and strips for review (TECH_SPEC 8.1, 8.2).

    python -I -B tools/contact.py sheet out.png a.png b.png ... [--cols 5] [--height 600]
    python -I -B tools/contact.py strip out.png base.png        (tiles base_1.png .. base_K-1.png and base.png in order)
"""
import glob
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont


def label_font(size):
    for f in (r"C:\Windows\Fonts\segoeuib.ttf", r"C:\Windows\Fonts\arialbd.ttf"):
        if os.path.exists(f):
            return ImageFont.truetype(f, size)
    return ImageFont.load_default()


def sheet(out, paths, cols=5, height=600, names=None):
    thumbs = []
    for p in paths:
        im = Image.open(p).convert("RGB")
        w = max(1, round(im.width * height / im.height))
        thumbs.append((im.resize((w, height), Image.LANCZOS), names[len(thumbs)] if names else os.path.splitext(os.path.basename(p))[0]))
    if not thumbs:
        raise SystemExit("contact: nothing to tile")
    cw = max(t.width for t, _ in thumbs)
    rows = (len(thumbs) + cols - 1) // cols
    pad, lab = 10, 30
    img = Image.new("RGB", (cols * (cw + pad) + pad, rows * (height + lab + pad) + pad), (24, 22, 40))
    d = ImageDraw.Draw(img)
    f = label_font(20)
    for i, (t, name) in enumerate(thumbs):
        x = pad + (i % cols) * (cw + pad)
        y = pad + (i // cols) * (height + lab + pad)
        img.paste(t, (x + (cw - t.width) // 2, y + lab))
        d.text((x, y + 4), name, fill=(235, 235, 245), font=f)
    img.save(out)
    print("[contact]", out, img.size)


def strip(out, base):
    stem, ext = os.path.splitext(base)
    parts = []
    for p in glob.glob(stem + "_*" + ext):
        m = re.match(re.escape(stem) + r"_(\d+)" + re.escape(ext) + "$", p)
        if m:
            parts.append((int(m.group(1)), p))
    parts.sort()
    paths = [p for _, p in parts] + [base]
    names = [f"frame -{len(paths) - 1 - i}" if i < len(paths) - 1 else "last" for i in range(len(paths))]
    sheet(out, paths, cols=min(len(paths), 8), height=520, names=names)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    mode, out = argv[0], argv[1]
    rest = argv[2:]
    cols, height = 5, 600
    files = []
    i = 0
    while i < len(rest):
        if rest[i] == "--cols":
            cols = int(rest[i + 1])
            i += 2
        elif rest[i] == "--height":
            height = int(rest[i + 1])
            i += 2
        else:
            files.append(rest[i])
            i += 1
    if mode == "sheet":
        sheet(out, files, cols, height)
    elif mode == "strip":
        strip(out, files[0])
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
