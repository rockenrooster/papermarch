"""greyscale.py out_dir a.png [b.png ...]: greyscale copies (Rec. 709 luma) for the P6 accessibility sheet, where gates
must be told apart by symbol and the two teams by silhouette without colour."""
import os
import sys

from PIL import Image


def main(argv):
    out = argv[0]
    os.makedirs(out, exist_ok=True)
    for p in argv[1:]:
        im = Image.open(p).convert("RGB")
        r, g, b = im.split()
        luma = Image.merge("RGB", (r, g, b)).convert("L", (0.2126, 0.7152, 0.0722, 0))
        name = os.path.splitext(os.path.basename(p))[0].replace(".small", "")
        luma.convert("RGB").save(os.path.join(out, name + "_grey.png"))
        print(os.path.join(out, name + "_grey.png"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
