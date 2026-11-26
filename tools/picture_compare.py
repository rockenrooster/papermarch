"""picture_compare.py device.png pc.png [--max-diff N]: does a phone draw what the PC draws? (VARIETY_TECH 10, trap 1)

A phone's GPU dithers and blends a little differently, so pixels are never compared one by one. Two measures instead:
- the share of gold pixels (wisps) and of violet pixels (glooms) in the lane's area, each within 20% of the PC's
  (a phone that draws no crowd, as the OnePlus 12R once did, loses nearly all of both);
- the mean absolute difference of the two pictures shrunk to 64 px wide, under --max-diff (0..255 a channel).
Exit 0 when both hold, 1 when not, 2 on bad input.
"""
import argparse
import colorsys
import sys

from PIL import Image

LANE = (0.15, 0.20, 0.85, 0.90)  # the lane's area, as fractions of the frame: x0, y0, x1, y1


def shares(im):
    w, h = im.size
    box = (int(LANE[0] * w), int(LANE[1] * h), int(LANE[2] * w), int(LANE[3] * h))
    small = im.crop(box).resize((160, int(160 * (box[3] - box[1]) / (box[2] - box[0]))), Image.BILINEAR)
    gold = violet = 0
    px = small.get_flattened_data() if hasattr(small, "get_flattened_data") else small.getdata()
    n = 0
    for r, g, b in px:
        n += 1
        hh, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
        deg = hh * 360.0
        if 25.0 <= deg <= 58.0 and s >= 0.45 and v >= 0.55:
            gold += 1
        elif 255.0 <= deg <= 300.0 and s >= 0.35 and v >= 0.22:
            violet += 1
    return gold / n, violet / n


def mean_diff(a, b):
    size = (64, max(1, round(64 * a.height / a.width)))
    pa = a.resize(size, Image.BILINEAR)
    pb = b.resize(size, Image.BILINEAR)
    da = pa.get_flattened_data() if hasattr(pa, "get_flattened_data") else pa.getdata()
    db = pb.get_flattened_data() if hasattr(pb, "get_flattened_data") else pb.getdata()
    total = 0
    count = 0
    for p, q in zip(da, db):
        total += abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2])
        count += 3
    return total / count


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("device")
    ap.add_argument("pc")
    ap.add_argument("--max-diff", type=float, default=4.0)
    a = ap.parse_args()
    try:
        dev = Image.open(a.device).convert("RGB")
        pc = Image.open(a.pc).convert("RGB")
    except OSError as e:
        print(f"picture_compare: {e}")
        return 2
    if dev.size != pc.size:
        print(f"picture_compare: sizes differ ({dev.size} on the device, {pc.size} on the PC)")
        return 1
    gd, vd = shares(dev)
    gp, vp = shares(pc)
    diff = mean_diff(dev, pc)

    def within(x, ref):
        return abs(x - ref) <= 0.20 * ref if ref > 0.002 else x <= 0.004

    ok_g, ok_v, ok_d = within(gd, gp), within(vd, vp), diff <= a.max_diff
    print(f"gold {gd * 100:.2f}% (PC {gp * 100:.2f}%) {'ok' if ok_g else 'DIFFERS'}; "
          f"violet {vd * 100:.2f}% (PC {vp * 100:.2f}%) {'ok' if ok_v else 'DIFFERS'}; "
          f"mean difference {diff:.1f} (limit {a.max_diff:.0f}) {'ok' if ok_d else 'TOO LARGE'}")
    return 0 if ok_g and ok_v and ok_d else 1


if __name__ == "__main__":
    sys.exit(main())
