#!/usr/bin/env python3
"""Do two host-stub shots show the same screen?

    python3 -I tools/same-shot.py A.pgm B.pgm

Same block signatures and tolerance as tools/check-golden.py. Exit 0 if they
match, 1 if not.
"""
import importlib.util
import os
import sys

spec = importlib.util.spec_from_file_location("cg", os.path.join(os.path.dirname(__file__), "check-golden.py"))
cg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cg)

a, b = cg.signature(sys.argv[1]), cg.signature(sys.argv[2])
ok = sum(1 for x, y in zip(a, b) if abs(x - y) <= cg.TOL) / len(a)
print(f"{'ok  ' if ok >= cg.MIN_MATCH else 'FAIL'} {os.path.basename(sys.argv[2])} vs {os.path.basename(sys.argv[1])}: "
      f"{ok:.1%} of blocks match")
sys.exit(0 if ok >= cg.MIN_MATCH else 1)
