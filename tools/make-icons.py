#!/usr/bin/env python3
"""Draw the reader's app icons (original artwork, no game graphics).

    python3 -I tools/make-icons.py [OUTDIR]      default: packaging/icons

Writes, for each launcher, NAME.bmp (normal) and NAME_f.bmp (focused: shown
while tapped), 128x128, 8-bit palette, 16 gray levels (the panel's), as the
PocketBook firmware expects in applications/icons/. PNG previews go to
OUTDIR/preview/ (not installed).

Design: a heraldic shield (clouds in a dark sky over a bold "IV") with a
sword behind it; the keypad launcher adds a small keypad badge. The focused
icon is the same, inverted.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

SIZE = 128
SS = 4                      # supersampling
S = SIZE * SS
FONT = "/usr/share/fonts/truetype/noto/NotoSerif-Bold.ttf"

INK, DARK, MID, LIGHT, PAPER = 0, 68, 136, 204, 255


def shield_points(cx, top, w, h):
    """Heater shield: flat top, straight sides, curved to a point."""
    pts = [(cx - w / 2, top), (cx + w / 2, top), (cx + w / 2, top + h * 0.45)]
    steps = 24
    for i in range(1, steps + 1):  # right curve down to the point
        t = i / steps
        x = cx + w / 2 * (1 - t) ** 1.4
        y = top + h * 0.45 + h * 0.55 * (1 - (1 - t) ** 2)
        pts.append((x, y))
    for i in range(steps - 1, -1, -1):  # left curve back up
        t = i / steps
        x = cx - w / 2 * (1 - t) ** 1.4
        y = top + h * 0.45 + h * 0.55 * (1 - (1 - t) ** 2)
        pts.append((x, y))
    return pts


def cloud(d, cx, cy, r, fill, outline, width):
    """A cloud: a row of overlapping puffs on a flat base."""
    puffs = [(-1.25, 0.25, 0.55), (-0.55, -0.15, 0.75), (0.3, -0.3, 0.85), (1.1, 0.15, 0.6)]
    # outline pass, then fill pass, so inner overlaps don't show lines
    for dx, dy, pr in puffs:
        R = r * pr + width
        d.ellipse([cx + dx * r - R, cy + dy * r - R, cx + dx * r + R, cy + dy * r + R], fill=outline)
    d.rectangle([cx - 1.25 * r, cy + 0.1 * r - width, cx + 1.1 * r, cy + 0.65 * r + width], fill=outline)
    for dx, dy, pr in puffs:
        R = r * pr
        d.ellipse([cx + dx * r - R, cy + dy * r - R, cx + dx * r + R, cy + dy * r + R], fill=fill)
    d.rectangle([cx - 1.25 * r, cy + 0.1 * r, cx + 1.1 * r, cy + 0.65 * r], fill=fill)


def draw_icon(focused, keypad):
    im = Image.new("L", (S, S), PAPER)
    d = ImageDraw.Draw(im)
    fg, bg = INK, PAPER

    # Two crossed swords behind the shield: hilts above its top corners,
    # points below its lower sides.
    import math
    ox, oy = S / 2, S / 2 + 10  # where the swords cross

    def sword(angle):
        a = math.radians(angle)

        def p(t, n):  # t along the sword (towards the hilt), n across it
            return (ox - t * math.cos(a) - n * math.sin(a), oy - t * math.sin(a) + n * math.cos(a))

        d.polygon([p(-225, 0), p(-190, -15), p(236, -15), p(236, 15), p(-190, 15)], fill=fg)  # blade
        d.polygon([p(236, -48), p(260, -48), p(260, 48), p(236, 48)], fill=fg)                # crossguard
        d.polygon([p(260, -12), p(298, -12), p(298, 12), p(260, 12)], fill=fg)                # grip
        x, y = p(306, 0)
        d.ellipse([x - 21, y - 21, x + 21, y + 21], fill=fg)                                   # pommel

    sword(45)    # hilt top left
    sword(135)   # hilt top right

    # Shield
    cx, top, w, h = S / 2, 104, 280, 332
    d.polygon(shield_points(cx, top - 12, w + 24, h + 22), fill=bg)       # thin gap round it
    d.polygon(shield_points(cx, top, w, h), fill=fg)                     # outline
    inner = shield_points(cx, top + 20, w - 44, h - 38)
    d.polygon(inner, fill=bg)
    # Sky band with clouds across the top of the shield
    sky = Image.new("L", (S, S), 0)
    ImageDraw.Draw(sky).polygon(inner, fill=255)
    band = Image.new("L", (S, S), 0)
    ImageDraw.Draw(band).rectangle([0, 0, S, top + 140], fill=255)
    mask = Image.composite(sky, Image.new("L", (S, S), 0), band)
    im.paste(DARK, mask=mask)
    d = ImageDraw.Draw(im)
    cloud(d, cx - 46, top + 88, 30, PAPER, PAPER, 0)
    cloud(d, cx + 60, top + 106, 23, PAPER, PAPER, 0)
    d.rectangle([cx - w / 2, top + 140, cx + w / 2, top + 152], fill=fg)  # divider

    # "IV"
    font = ImageFont.truetype(FONT, 124)
    text = "IV"
    tb = d.textbbox((0, 0), text, font=font)
    tw = tb[2] - tb[0]
    d.text((cx - tw / 2 - tb[0], top + 166 - tb[1]), text, font=font, fill=fg)

    if keypad:  # badge: a small 3x2 keypad in the lower left corner
        bx, by, bw, bh = 20, S - 150, 150, 120
        d.rounded_rectangle([bx, by, bx + bw, by + bh], radius=18, fill=bg, outline=fg, width=12)
        kw, g = 32, 12
        for r in range(2):
            for col in range(3):
                x0 = bx + 22 + col * (kw + g)
                y0 = by + 22 + r * (kw + g)
                d.rectangle([x0, y0, x0 + kw, y0 + kw], fill=fg)

    if focused:  # tapped: inverted
        im = Image.eval(im, lambda v: 255 - v)

    small = im.resize((SIZE, SIZE), Image.LANCZOS)
    # 16 panel levels, 8-bit palette (the firmware wants palettized 8-bit BMP)
    small = small.point(lambda v: min(255, round(v / 17) * 17))
    pal = small.convert("P", palette=Image.ADAPTIVE, colors=16)
    return small, pal


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "packaging", "icons")
    os.makedirs(out, exist_ok=True)
    for name, keypad in (("mmiv", False), ("mmiv-keypad", True)):
        for focused in (False, True):
            gray, pal = draw_icon(focused, keypad)
            base = os.path.join(out, name + ("_f" if focused else ""))
            pal.save(base + ".bmp")
            os.makedirs(os.path.join(out, "preview"), exist_ok=True)
            gray.save(os.path.join(out, "preview", os.path.basename(base) + ".png"))
            print("wrote", base + ".bmp")


if __name__ == "__main__":
    main()
