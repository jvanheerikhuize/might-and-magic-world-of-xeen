#!/usr/bin/env python3
"""Make one probe save per place in World of Xeen with the host stub.

    python3 -I tools/make-probe-saves.py OUT_DIR [--from SLOT] [SLOT ...]

Starts the host stub (tools/run-host-script.sh, build first with
tools/build-host.sh pocketbook) from save SLOT (default 0) in ~/games/xeen/saves,
opens the Xeen debugger ("debugger" script command), teleports with
"map ID X Y SIDE" and saves the result to the place's slot ("savegame").
When combat starts on arrival the save is refused; other spots are tried.

OUT_DIR receives worldofxeen.NNN per place, shots/NNN-NAME.png and places.json.
Push them to the reader into mmiv/probe-saves and write "probe-saves" to
mmiv/savedir to play them (tests/device/README.md); the player's own saves in
mmiv/saves are not touched. Saves hold game data: keep them out of the repo.
Map ids and names: tools/xeen-maps.py.
"""
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent

PLACES = [
    # slot, side (0 Clouds, 1 Darkside), map id, x, y, kind, name
    (20, 0, 28, 8, 8, "town", "Vertigo"),
    (21, 0, 29, 8, 8, "town (night)", "Nightshadow"),
    (22, 0, 32, 8, 8, "town (snow)", "Winterkill"),
    (23, 0, 1, 8, 8, "outdoors", "Clouds Area A1"),
    (24, 0, 27, 8, 8, "sky", "Clouds of Xeen"),
    (25, 0, 33, 8, 8, "mine (dark)", "Dwarf Mine 1"),
    (26, 0, 51, 8, 8, "tower", "Witch Tower 1"),
    (27, 0, 64, 8, 8, "castle", "Castle Burlock 1"),
    (28, 0, 85, 8, 8, "arena", "The Warzone"),
    (29, 0, 75, 8, 8, "endgame", "Xeen's Castle 1"),
    (30, 1, 29, 8, 8, "town", "Castleview"),
    (31, 1, 37, 8, 8, "town", "Olympus"),
    (32, 1, 1, 8, 8, "outdoors", "Darkside Area A1"),
    (33, 1, 89, 8, 8, "skyroad", "Skyroad A1"),
    (34, 1, 78, 8, 8, "dungeon (dark)", "Lost Souls Dungeon 1"),
    (35, 1, 113, 8, 8, "tower", "Darkstone Tower 1"),
    (36, 1, 25, 8, 8, "plane", "Elemental Plane of Fire"),
    (37, 1, 83, 8, 8, "pyramid", "Great Pyramid 1"),
]

# Tried in order after the place's own spot when combat starts there.
SPOTS = [None, (2, 2), (13, 13), (2, 13), (13, 2), (5, 10)]


def script(slot, side, mid, x, y, name):
    return "\n".join([
        "wait 5000", "debugger", "wait 1200", f"answer map {mid} {x} {y} {side}", "wait 1500",
        f"shot p{slot}", f"savegame {slot} {name}", "wait 500", "quit"]) + "\n"


def make(out, start_slot, only):
    (out / "shots").mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="probe-saves-"))
    report = []
    for slot, side, mid, x, y, kind, name in PLACES:
        if only and slot not in only:
            continue
        spot = None
        for alt in SPOTS:
            px, py = alt or (x, y)
            sc = work / f"{slot}.txt"
            sc.write_text(script(slot, side, mid, px, py, name))
            run = work / f"run-{slot}"
            subprocess.run([str(ROOT / "tools/run-host-script.sh"), str(sc), str(run), str(start_slot)],
                           env={**os.environ, "MMIV_LAYOUT": "landscape", "MMIV_TIMEOUT": "60000"},
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            log = (run / "run.log").read_text(errors="replace") if (run / "run.log").exists() else ""
            if f'slot {slot} "{name}" saved' in log:
                shutil.copy(run / "saves" / f"worldofxeen.{slot:03d}", out / f"worldofxeen.{slot:03d}")
                shot = run / f"p{slot}.png"
                if shot.exists():
                    shutil.copy(shot, out / "shots" / f"{slot}-{name.replace(' ', '_')}.png")
                spot = [px, py]
                break
        report.append({"slot": slot, "side": side, "map": mid, "name": name, "kind": kind, "spot": spot})
        print(slot, name, f"saved at {spot[0]},{spot[1]}" if spot else "NOT SAVED (combat at every spot)",
              flush=True)
    (out / "places.json").write_text(json.dumps(report, indent=1) + "\n")
    shutil.rmtree(work, ignore_errors=True)
    return report


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args or args[0].startswith("-"):
        sys.exit(__doc__)
    start = 0
    if "--from" in args:
        i = args.index("--from")
        start = int(args[i + 1])
        del args[i:i + 2]
    out_dir = pathlib.Path(args[0]).expanduser()
    make(out_dir, start, {int(s) for s in args[1:]})
