#!/usr/bin/env python3
"""Preview ScummVM screenshots as they would look on the PB628 e-ink panel.

    python3 -I tools/eink-preview.py [options] shot.png ...

For each screenshot it writes, to the output dir (default ~/games/xeen/eink-preview/):
  <name>.eink.png    original colour | luma mapping | paper mapping, each dithered, at 2x
  <name>.device.png  the chosen mapping, dithered, on a 758x1024 portrait canvas
                     the way the backend will draw it (game on top, keypad area below)

Colour -> gray is decided per palette colour, which is what the backend will do
with its 256-entry LUT (PLAN.md section 3):

  luma   Rec.601 luma through black/white points and gamma. Simple, but Xeen's
         green/blue/yellow value text has about the same luma as the tan
         parchment behind it and disappears.
  paper  (default) CIE L* minus a penalty for colours that are both strongly
         saturated and far in hue from the parchment, so coloured text goes
         dark while the parchment, sky and stone keep their lightness.

The screenshot may be at any integer multiple of 320x200, or the 4:3
aspect-corrected 320x240 multiples ScummVM saves; it is reduced to 320x200
with nearest-neighbour first.
"""
import argparse
import math
import os
import sys

from PIL import Image, ImageDraw

GAME_W, GAME_H = 320, 200
PANEL_W, PANEL_H = 758, 1024
LEVELS = 16
BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
PARCHMENT = (214, 154, 123)  # dominant UI panel colour in Xeen's palette


def to_game_res(img):
    return img.convert("RGB").resize((GAME_W, GAME_H), Image.NEAREST)


def srgb_to_lab(rgb):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    r, g, b = (lin(float(c)) for c in rgb)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

    def f(t):
        return t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116

    fx, fy, fz = f(x), f(y), f(z)
    return 116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)


def smoothstep(lo, hi, v):
    t = min(1.0, max(0.0, (v - lo) / (hi - lo)))
    return t * t * (3 - 2 * t)


class Mapper:
    """Colour -> 0..1 gray. Results are cached per colour (a palette has <= 256)."""

    def __init__(self, mode, a):
        self.mode, self.a, self.cache = mode, a, {}
        self.paper = srgb_to_lab(PARCHMENT)

    def __call__(self, rgb):
        v = self.cache.get(rgb)
        if v is None:
            v = self.cache[rgb] = self._map(rgb)
        return v

    def _map(self, rgb):
        a = self.a
        if self.mode == "luma":
            r, g, b = rgb
            x = (299 * r + 587 * g + 114 * b) / 1000
            x = (x - a.black) / (a.white - a.black)
            return min(1.0, max(0.0, x)) ** a.gamma
        L, A, B = srgb_to_lab(rgb)
        chroma = math.hypot(A, B)
        hue_dist = math.hypot(A - self.paper[1], B - self.paper[2])
        L -= a.k * smoothstep(a.chroma_lo, a.chroma_hi, chroma) * hue_dist
        x = (L - a.l_black) / (a.l_white - a.l_black)
        return min(1.0, max(0.0, x)) ** a.gamma


def quantize(img, mapper, dither):
    src = img.load()
    out = Image.new("L", img.size)
    dst = out.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            v = mapper(src[x, y]) * (LEVELS - 1)
            if dither:
                v += (BAYER4[y % 4][x % 4] + 0.5) / 16 - 0.5
            dst[x, y] = min(LEVELS - 1, max(0, round(v))) * 255 // (LEVELS - 1)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("shots", nargs="+")
    ap.add_argument("--mode", choices=["paper", "luma"], default="paper", help="mapping for the device mock-up")
    ap.add_argument("--gamma", type=float, default=1.0)
    ap.add_argument("--black", type=float, default=8, help="luma mode: luma treated as black")
    ap.add_argument("--white", type=float, default=230, help="luma mode: luma treated as white")
    ap.add_argument("--l-black", type=float, default=12, help="paper mode: L* treated as black")
    ap.add_argument("--l-white", type=float, default=72, help="paper mode: L* treated as white (parchment is ~69)")
    ap.add_argument("--k", type=float, default=0.9, help="paper mode: darkening per unit of hue distance")
    ap.add_argument("--chroma-lo", type=float, default=45, help="paper mode: chroma below this is never darkened")
    ap.add_argument("--chroma-hi", type=float, default=75, help="paper mode: chroma above this is fully darkened")
    ap.add_argument("--no-dither", action="store_true")
    ap.add_argument("--out", default=os.path.expanduser("~/games/xeen/eink-preview"))
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    luma, paper = Mapper("luma", args), Mapper("paper", args)
    chosen = paper if args.mode == "paper" else luma
    dither = not args.no_dither
    s = 2
    for path in args.shots:
        game = to_game_res(Image.open(path))
        tiles = [game, quantize(game, luma, dither).convert("RGB"), quantize(game, paper, dither).convert("RGB")]
        sheet = Image.new("RGB", (GAME_W * s * 3 + 20, GAME_H * s), "white")
        for i, t in enumerate(tiles):
            sheet.paste(t.resize((GAME_W * s, GAME_H * s), Image.NEAREST), (i * (GAME_W * s + 10), 0))
        name = os.path.splitext(os.path.basename(path))[0]
        sheet.save(os.path.join(args.out, name + ".eink.png"))

        frame = quantize(game, chosen, dither)
        dev = Image.new("L", (PANEL_W, PANEL_H), 255)
        dev.paste(frame.resize((GAME_W * s, GAME_H * s), Image.NEAREST), ((PANEL_W - GAME_W * s) // 2, 24))
        d = ImageDraw.Draw(dev)
        d.rectangle([16, 24 + GAME_H * s + 16, PANEL_W - 17, PANEL_H - 17], outline=0, width=2)
        d.text((24, 24 + GAME_H * s + 24), "keypad area (PLAN.md section 4)", fill=0)
        dev.save(os.path.join(args.out, name + ".device.png"))
        print(f"{path} -> {args.out}/{name}.eink.png, {name}.device.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
