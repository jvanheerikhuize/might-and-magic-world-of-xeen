#!/usr/bin/env python3
"""How much of two host-stub shots differs, pixel by pixel?

    python3 -I tools/pixel-diff.py A.pgm B.pgm MIN_PCT [MAX_PCT]

Exit 0 if at least MIN_PCT (and at most MAX_PCT) percent of the pixels differ
by more than one panel gray level, 1 if not. For changes the coarse block signatures of
tools/same-shot.py cannot see, such as a scrolled list of text rows.
"""
import importlib.util
import os
import sys

spec = importlib.util.spec_from_file_location("cg", os.path.join(os.path.dirname(__file__), "check-golden.py"))
cg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cg)

wa, ha, a = cg.read_pgm(sys.argv[1])
wb, hb, b = cg.read_pgm(sys.argv[2])
min_pct = float(sys.argv[3])
max_pct = float(sys.argv[4]) if len(sys.argv) > 4 else 100.0
if (wa, ha) != (wb, hb):
    print(f"FAIL size differs: {wa}x{ha} vs {wb}x{hb}")
    sys.exit(1)
pct = 100 * sum(1 for x, y in zip(a, b) if abs(x - y) > 17) / len(a)
ok = min_pct <= pct <= max_pct
print(f"{'ok  ' if ok else 'FAIL'} {os.path.basename(sys.argv[2])} vs {os.path.basename(sys.argv[1])}: "
      f"{pct:.2f}% of pixels differ (need {min_pct}-{max_pct}%)")
sys.exit(0 if ok else 1)
