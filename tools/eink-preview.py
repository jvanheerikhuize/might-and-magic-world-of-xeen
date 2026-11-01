#!/usr/bin/env python3
"""Preview ScummVM screenshots as they would look on the PB628 e-ink panel.

    python3 -I tools/eink-preview.py [--gamma G] [--black B] [--white W] shot.png ...

For each screenshot it writes <name>.eink.png next to an output dir
(default ~/games/xeen/eink-preview/) with three panels at 2x, side by side:
    original colour | 16 grays, no dither | 16 grays, 4x4 Bayer dither
and <name>.device.png: the dithered frame placed on a 758x1024 portrait
canvas the way the backend will draw it (game on top, keypad area below).

The screenshot may be at any integer multiple of 320x200 (ScummVM saves the
scaled window); it is reduced to 320x200 with nearest-neighbour first.
This mirrors the planned backend pipeline in PLAN.md section 3.
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw

GAME_W, GAME_H = 320, 200
PANEL_W, PANEL_H = 758, 1024
LEVELS = 16
BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def to_game_res(img):
    img = img.convert("RGB")
    w, h = img.size
    if (w, h) == (GAME_W, GAME_H):
        return img
    if w % GAME_W == 0 and h % GAME_H == 0:
        return img.resize((GAME_W, GAME_H), Image.NEAREST)
    # Aspect-corrected screenshots (320x240 multiples): squash back.
    return img.resize((GAME_W, GAME_H), Image.NEAREST)


def curve(gamma, black, white):
    """Luma 0..255 -> adjusted 0..1 (black/white points, then gamma)."""
    lut = []
    for v in range(256):
        x = (v - black) / max(1, white - black)
        x = min(1.0, max(0.0, x))
        lut.append(x ** gamma)
    return lut


def quantize(img, lut, dither):
    src = img.load()
    out = Image.new("L", img.size)
    dst = out.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            r, g, b = src[x, y]
            luma = (299 * r + 587 * g + 114 * b) // 1000  # Rec.601, as for the palette LUT
            v = lut[luma] * (LEVELS - 1)
            if dither:
                v += (BAYER4[y % 4][x % 4] + 0.5) / 16 - 0.5
            q = min(LEVELS - 1, max(0, round(v)))
            dst[x, y] = q * 255 // (LEVELS - 1)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("shots", nargs="+")
    ap.add_argument("--gamma", type=float, default=0.8, help="<1 lifts dark tones (default 0.8)")
    ap.add_argument("--black", type=int, default=8, help="luma treated as black (default 8)")
    ap.add_argument("--white", type=int, default=230, help="luma treated as white (default 230)")
    ap.add_argument("--out", default=os.path.expanduser("~/games/xeen/eink-preview"))
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    lut = curve(args.gamma, args.black, args.white)
    for path in args.shots:
        game = to_game_res(Image.open(path))
        plain = quantize(game, lut, dither=False)
        dith = quantize(game, lut, dither=True)
        s = 2
        tiles = [game, plain.convert("RGB"), dith.convert("RGB")]
        sheet = Image.new("RGB", (GAME_W * s * 3 + 20, GAME_H * s), "white")
        for i, t in enumerate(tiles):
            sheet.paste(t.resize((GAME_W * s, GAME_H * s), Image.NEAREST), (i * (GAME_W * s + 10), 0))
        name = os.path.splitext(os.path.basename(path))[0]
        sheet.save(os.path.join(args.out, name + ".eink.png"))

        dev = Image.new("L", (PANEL_W, PANEL_H), 255)
        x0 = (PANEL_W - GAME_W * s) // 2
        dev.paste(dith.resize((GAME_W * s, GAME_H * s), Image.NEAREST), (x0, 24))
        d = ImageDraw.Draw(dev)
        d.rectangle([16, 24 + GAME_H * s + 16, PANEL_W - 17, PANEL_H - 17], outline=0, width=2)
        d.text((24, 24 + GAME_H * s + 24), "keypad area (PLAN.md section 4)", fill=0)
        dev.save(os.path.join(args.out, name + ".device.png"))
        print(f"{path} -> {args.out}/{name}.eink.png, {name}.device.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
