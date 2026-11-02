#!/usr/bin/env python3
"""Random gameplay scenarios for the reader: pbtk run scripts on probe saves.

    python3 -I tools/random-scenarios.py SEED COUNT STEPS OUTDIR
    pbtk --view ccw run OUTDIR/r01-27.txt --out OUT --stop-on-user

Each scenario opens a random probe save (tests/device/README.md) and plays
STEPS weighted random actions: walking, turning, the action buttons, the party
screens, bursts, long presses, taps outside the game frame. After every action
it waits and takes a shot; a screen that needs closing gets `key back` and a
second shot. There are no expect-* lines, so nothing aborts a run: compare the
shots afterwards (tools/same-shot.py, tools/pixel-diff.py) and read the log.

Left out on purpose (2026-11-02 playthroughs, docs/device-test-2026-11-02.md):
  - swipes: one on the title screen took the whole reader down;
  - probe saves 26 (the party falls to its death on the first step), 32 (a
    level-1 party dies at once on Darkside), 33 (falls off the Skyroad within
    two steps), 29 (Xeen's Castle kills it on the first step) and 37 (8,8
    is walled in);
  - apostrophes in `note` text: pbtk reads each line with shlex and stops.
Dark places cast Light first so steps show something.
Coordinates: upright landscape (pbtk view ccw), panel = frame + (32, 79).
"""
import random
import sys
import pathlib

PLACES = {20: "Vertigo", 21: "Nightshadow", 22: "Winterkill", 23: "Clouds Area A1", 24: "Clouds of Xeen",
          25: "Dwarf Mine 1", 27: "Castle Burlock 1", 30: "Castleview", 31: "Olympus",
          34: "Lost Souls Dungeon 1", 35: "Darkstone Tower 1", 36: "Elemental Plane of Fire"}
DARK = {25, 34, 35}
PORTRAITS = [110, 215, 320, 430, 535, 645]
CAST, SPELL_CAST, SPELL_NEW = "tap 845 332", "tap 769 432", "tap 850 432"
ACTIONS = [  # weight, label, commands (or a function of the RNG), needs `key back` afterwards
    (22, "forward key", ["key next"], False),
    (5, "back key", ["key prev"], False),
    (8, "forward arrow", ["tap 845 549"], False),
    (8, "turn left", ["tap 770 549"], False),
    (8, "turn right", ["tap 920 549"], False),
    (4, "strafe left", ["tap 770 611"], False),
    (4, "strafe right", ["tap 920 611"], False),
    (3, "step back arrow", ["tap 845 611"], False),
    (5, "character sheet", lambda r: [f"tap {r.choice(PORTRAITS)} 574"], True),
    (3, "quests", ["tap 920 394"], True),
    (3, "automap", ["tap 770 457"], True),
    (3, "info", ["tap 920 457"], True),
    (3, "cast", [CAST], True),
    (3, "shoot", ["tap 770 332"], False),
    (3, "bash", ["tap 770 394"], False),
    (2, "rest", ["tap 920 332"], True),
    (2, "time", ["tap 845 457"], True),
    (2, "burst 4 forward", ["key next", "wait 150", "key next", "wait 150", "key next", "wait 150", "key next"], False),
    (2, "cast new spell", [CAST, "wait 1500", SPELL_NEW], True),
    (2, "cast ready spell", [CAST, "wait 1500", SPELL_CAST], False),
    (2, "long press portrait", lambda r: [f"long {r.choice(PORTRAITS)} 574"], True),
    (2, "tap outside frame", lambda r: [r.choice(["tap 512 730", "tap 1010 400", "tap 10 300", "tap 512 30"])], False),
    (1, "short menu key", ["key menu"], True),
]


def scenario(r, seed, i, slot, steps):
    name = f"r{i:02d}-{slot}"
    place = PLACES[slot].replace("'", "")
    lines = [f"# random scenario {name}: seed {seed}, probe save {slot} ({place})",
             f"note {name}: open probe save {slot} ({place})",
             f"write /mnt/ext1/applications/mmiv/resume {slot}", "restart", "wait 7000", f"shot {name}-00-start"]
    if slot in DARK:
        lines += [f"note {name}: cast Light", CAST, "wait 1500", SPELL_CAST, "wait 2500", f"shot {name}-00-light"]
    for k in range(1, steps + 1):
        _, label, act, back = r.choices(ACTIONS, weights=[a[0] for a in ACTIONS])[0]
        lines += [f"note {name} step {k}: {label}", *(act(r) if callable(act) else act),
                  "wait 2000", f"shot {name}-{k:02d}a"]
        if back:
            lines += ["key back", "wait 1800", f"shot {name}-{k:02d}b"]
    return name, "\n".join(lines) + "\n"


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__.split("\n\n")[1])
    seed, count, steps, out = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), pathlib.Path(sys.argv[4])
    r = random.Random(seed)
    out.mkdir(parents=True, exist_ok=True)
    for i, slot in enumerate(r.sample(sorted(PLACES), min(count, len(PLACES))), 1):
        name, text = scenario(r, seed, i, slot, steps)
        (out / f"{name}.txt").write_text(text)
        print(name, PLACES[slot])


if __name__ == "__main__":
    main()
