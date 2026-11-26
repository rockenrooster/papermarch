"""flash_audit.py name frames_dir [--fps 60]: the flashing audit of P6 (GAME_DESIGN 14).

Reads every frame of a strip (<name>_<k>.png ... and <name>.png, the last), takes each frame's mean brightness
(Rec. 709 luma of the sRGB values, 0..1), and counts brightness swings: a swing is a move of more than 0.10 (10% of
full scale) away from the brightness at the previous swing. Passes when no one-second window holds more than three.
"""
import glob
import os
import re
import sys

from PIL import Image, ImageStat


def frames(directory, name):
    parts = []
    for p in glob.glob(os.path.join(directory, name + "_*.png")):
        m = re.match(re.escape(name) + r"_(\d+)\.png$", os.path.basename(p))
        if m:
            parts.append((int(m.group(1)), p))
    parts.sort()
    paths = [p for _, p in parts]
    last = os.path.join(directory, name + ".png")
    if os.path.exists(last):
        paths.append(last)
    return paths


def brightness(path):
    im = Image.open(path).convert("RGB")
    r, g, b = ImageStat.Stat(im).mean
    return (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    name, directory = argv[0], argv[1]
    fps = int(argv[argv.index("--fps") + 1]) if "--fps" in argv else 60
    paths = frames(directory, name)
    if len(paths) < 2:
        print(f"[flash] {name}: no frames in {directory}")
        return 2
    lum = [brightness(p) for p in paths]
    swings, ref = [], lum[0]
    for i, v in enumerate(lum):
        if abs(v - ref) > 0.10:
            swings.append(i)
            ref = v
    worst = 0
    for i in range(len(lum)):
        worst = max(worst, sum(1 for s in swings if i <= s < i + fps))
    print(f"[flash] {name}: {len(lum)} frames, brightness {min(lum):.3f}-{max(lum):.3f}, {len(swings)} swings, "
          f"at most {worst} in one second -> {'PASS' if worst <= 3 else 'FAIL'}")
    return 0 if worst <= 3 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
