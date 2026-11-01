#!/usr/bin/env python3
"""Compare host-stub shots against golden signatures.

    python3 -I tools/check-golden.py OUTDIR GOLDEN.json [--record]

A signature is the mean gray of each 16x16 block of the 640x400 frame
(40x25 numbers). It catches wrong screens, wrong mapping, broken input or a
blank frame, tolerates small animated sprites, and keeps game imagery out of
the repository (only these coarse numbers are committed).

A shot passes when at least MIN_MATCH of its blocks are within TOL gray
levels of the golden value.
"""
import json
import os
import sys

BLOCK = 16
TOL = 12        # gray levels (one panel level is 17)
MIN_MATCH = 0.97


def read_pgm(path):
    with open(path, "rb") as f:
        data = f.read()
    parts = data.split(maxsplit=4)  # P5, w, h, maxval, pixels
    if parts[0] != b"P5":
        raise ValueError(path + ": not a binary PGM")
    w, h = int(parts[1]), int(parts[2])
    return w, h, parts[4][: w * h]


def signature(path):
    w, h, px = read_pgm(path)
    sig = []
    for by in range(0, h, BLOCK):
        for bx in range(0, w, BLOCK):
            total = 0
            for y in range(by, by + BLOCK):
                row = px[y * w + bx : y * w + bx + BLOCK]
                total += sum(row)
            sig.append(round(total / (BLOCK * BLOCK)))
    return sig


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    out, golden = sys.argv[1], sys.argv[2]
    record = "--record" in sys.argv[3:]
    shots = sorted(f[:-4] for f in os.listdir(out) if f.endswith(".pgm"))
    if not shots:
        print("no shots in", out)
        return 1
    sigs = {s: signature(os.path.join(out, s + ".pgm")) for s in shots}
    if record:
        with open(golden, "w") as f:
            json.dump({"block": BLOCK, "shots": sigs}, f, separators=(",", ":"))
            f.write("\n")
        print("recorded", len(sigs), "shots to", golden)
        return 0
    with open(golden) as f:
        ref = json.load(f)["shots"]
    failed = 0
    for name in sorted(set(ref) | set(sigs)):
        if name not in sigs or name not in ref:
            print(f"FAIL {name}: {'missing shot' if name not in sigs else 'no golden'}")
            failed += 1
            continue
        a, b = sigs[name], ref[name]
        ok = sum(1 for x, y in zip(a, b) if abs(x - y) <= TOL) / len(b)
        status = "ok  " if ok >= MIN_MATCH else "FAIL"
        failed += status == "FAIL"
        print(f"{status} {name}: {ok:.1%} of blocks match")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
