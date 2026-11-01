#!/usr/bin/env python3
"""Turn host-stub shots into README screenshots.

    python3 -I tools/compose-screenshots.py OUTDIR SHOTDIR...

Each NAME.pgm (the gray frame the backend sends to the panel, 960x600 in
landscape) is placed on a black 1024x758 panel at 32,79 as on the reader and
written as OUTDIR/NAME.png. With NAME.rgb.ppm (the game's colours) next to it,
OUTDIR/NAME-compare.png shows colour and e-ink side by side. Run by
tools/make-screenshots.sh; needs Pillow.
"""
import os
import sys

from PIL import Image

PANEL = (1024, 758)
FRAME_AT = (32, 79)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for d in sys.argv[2:]:
        for f in sorted(os.listdir(d)):
            if not f.endswith(".pgm"):
                continue
            name = f[:-4]
            gray = Image.open(os.path.join(d, f)).convert("L")
            panel = Image.new("L", PANEL, 0)
            panel.paste(gray, FRAME_AT)
            panel.save(os.path.join(out, name + ".png"), optimize=True)
            rgb = os.path.join(d, name + ".rgb.ppm")
            if os.path.exists(rgb):
                colour = Image.open(rgb).convert("RGB").resize(gray.size, Image.NEAREST)
                pair = Image.new("RGB", (gray.width * 2 + 16, gray.height), (255, 255, 255))
                pair.paste(colour, (0, 0))
                pair.paste(gray.convert("RGB"), (gray.width + 16, 0))
                pair = pair.resize((pair.width // 2, pair.height // 2), Image.LANCZOS)
                pair.save(os.path.join(out, name + "-compare.png"), optimize=True)
            print("wrote", name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
