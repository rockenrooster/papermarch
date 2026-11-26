"""Write an optimised review copy of a PNG, at most --max-width wide and --max-height tall (1600 x 1000).

    python -I -B tools/shrink_png.py captures/shot.png [out.png] [--max-width 1600] [--max-height 1000]

Without an explicit output path the copy is written next to the source as <name>.small.png.
"""
import argparse
import sys

from PIL import Image


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("src")
    parser.add_argument("dst", nargs="?")
    parser.add_argument("--max-width", type=int, default=1600)
    parser.add_argument("--max-height", type=int, default=1000)
    args = parser.parse_args()

    dst = args.dst or (args.src[:-4] if args.src.lower().endswith(".png") else args.src) + ".small.png"
    with Image.open(args.src) as image:
        image = image.convert("RGB")
        scale = min(1.0, args.max_width / image.width, args.max_height / image.height)
        if scale < 1.0:
            image = image.resize((max(1, round(image.width * scale)), max(1, round(image.height * scale))), Image.LANCZOS)
        image.save(dst, optimize=True)
        print(f"[png] {dst} {image.width}x{image.height}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
