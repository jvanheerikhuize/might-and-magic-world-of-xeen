#!/usr/bin/env python3
"""Finish World of Xeen from a new game: all three endings, with debugger help.

    tools/speedrun.py host OUTDIR             play it on the host stub (3 runs)
    tools/speedrun.py device OUTDIR [--agent HOST:PORT]
                                              play it on the reader, watchable

The route (researched with tools/xeen-events.py):
  Clouds    Vertigo mirror, "i lost it" -> Xeen Slayer Sword (only it hurts
            Lord Xeen); kill him at Xeen's Castle 4 (5,9) -> Xeen's Scepter;
            Sixth Mirror (4,9) -> Clouds ending.
  Darkside  Soul Box (map 37); stasis console (map 87) "where no man has gone
            before"; Corak x3 -> Soul Box with Corak; Dragon Pharaoh x2
            (map 86) -> award 45; the four Sleepers (maps 25-28) -> awards
            61-64; Sheltem's throne (map 51) -> Darkside ending.
  World     Great Pyramid (map 129, 13,15) with scepter + Darkside ending
            -> World ending.
The debugger gives invincibility, strong attacks and teleports between those
squares; every quest step itself is the game's own script.

After the Clouds and Darkside endings the game opens a save dialog and then
returns to the title menu: the run saves a checkpoint there (slots 42, 43)
and resumes from it, as a player would with Load. On the reader that dialog
is invisible after the Darkside ending (docs/speedrun-2026-11-02.md), so the
checkpoint goes through the test channel instead of the dialog.

On the reader the run uses its own save folder (mmiv/savedir = speedrun) and
puts the player's scummvm.ini back afterwards (the endings record game_won
there). Debugger and keys go through the dev event (pb-platform.h kDevEvent),
taps through pbtk's agent; a ping every 15 s keeps the reader awake.
"""
from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys
import threading
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent

# --------------------------------------------------------------- the route


def phases() -> list[tuple[str, int | None, list[str]]]:
    """[(name, slot to start from or None for a new game, commands)]."""

    def run():
        L: list[str] = []

        def w(ms):
            L.append(f"wait {ms}")

        def dbg(*cmds):
            L.append("debugger"); w(1500)
            for c in cmds:
                L.append(f"answer {c}"); w(1500)

        def keys(*names, gap=1500):
            for k in names:
                L.append(f"key {k}"); w(gap)

        def note(t):
            L.append(f"note {t}")

        def shot(n):
            L.append(f"shot {n}")

        return L, w, dbg, keys, note, shot

    out = []

    # --- A: new game -> Clouds ending -----------------------------------
    L, w, dbg, keys, note, shot = run()
    note("A1 title menu: Start a New Game, Adventurer")
    w(15000); shot("a00-title")
    keys("s", "s", "s", gap=3000); shot("a01-difficulty")
    keys("a", gap=6000); shot("a02-vertigo")
    note("A2 Vertigo mirror: 'i lost it' gives the Xeen Slayer Sword")
    dbg("invincible", "map 28 14 10 0"); w(1500)
    keys("left", gap=2500); keys("space", gap=2500); shot("a03-mirror")
    L.append("answer i lost it"); w(3000); shot("a04-sword")
    keys("esc", gap=2000)
    note("A3 Arturius: sabre and shield off, sword on")
    keys("f1", "i", "1", "r", "a", "2", "r", "w", "2", "e", gap=1800); shot("a05-equipped")
    keys("esc", "esc", "esc", gap=1500)
    note("A4 Xeen's Castle level 4: Lord Xeen")
    dbg("strength", "map 78 6 9 0"); w(1500)
    keys("left", gap=2500); keys("up", gap=4000); shot("a06-lord-xeen")
    keys(*["a"] * 6, gap=3000); shot("a07-xeen-dead")
    keys("enter", "esc", gap=2500)
    keys("f1", "i", "q", gap=2000); shot("a08-scepter"); keys("esc", "esc", "esc")
    note("A5 the Sixth Mirror")
    keys("up", "up", gap=3500); keys("space", gap=3000); shot("a09-sixth-mirror")
    keys("enter", "enter", gap=2500); keys("y", gap=5000)
    note("A6 Clouds ending (cutscene)")
    for i in range(5):
        w(40000); shot(f"a1{i}-clouds-ending")
    L.append("tapmid"); w(6000); L.append("tapmid"); w(10000); shot("a20-after")
    keys("space", gap=6000); shot("a21-save-dialog")
    L.append("savegame 42 clouds won"); w(3000)
    keys("esc", gap=5000); shot("a22-menu")
    out.append(("clouds", None, L))

    # --- B: Darkside quests -> Darkside ending --------------------------
    L, w, dbg, keys, note, shot = run()
    w(8000); shot("b00-resumed")
    note("B1 Soul Box (Darkside map 37)")
    dbg("invincible", "map 37 10 14 1"); w(1500)
    keys("right", gap=2500); keys("space", gap=2500); keys("f1", gap=2500)
    shot("b01-soul-box"); keys("esc", "esc")
    note("B2 stasis console: where no man has gone before")
    dbg("map 87 2 8 1"); w(1500)
    keys("left", gap=2500); keys("space", gap=2500); keys("enter", gap=2500)
    L.append("answer where no man has gone before"); w(2500); shot("b02-stasis-off")
    keys("enter", "enter", gap=2000)
    note("B3 Corak, three talks")
    dbg("map 87 4 8 1"); w(1500); keys("left", gap=2500)
    for i in range(3):
        keys("space", gap=3000); shot(f"b03-corak{i}"); keys(*["enter"] * 8)
    note("B4 Dragon Pharaoh, two talks")
    dbg("map 86 6 9 1"); w(1500); keys("left", gap=2500)
    for i in range(2):
        keys("space", gap=3000); shot(f"b04-pharaoh{i}"); keys(*["enter"] * 10)
    note("B5 the four Sleepers")
    for n, (m, x, y) in enumerate([(25, 3, 12), (26, 12, 12), (27, 12, 3), (28, 3, 3)]):
        dbg(f"map {m} {x} {y} 1"); w(1500)
        keys("space", gap=3000); shot(f"b05-sleeper{n}"); keys(*["enter"] * 8)
    keys("f1", "i", "q", gap=2000); shot("b06-quest"); keys("esc", "esc", "esc")
    note("B6 Sheltem's throne: Darkside ending (cutscene)")
    dbg("map 51 2 13 1"); w(1500)
    dbg("exit"); w(1000)
    keys("space", gap=5000)
    for i in range(4):
        w(40000); shot(f"b1{i}-darkside-ending")
    L.append("tapmid"); w(6000); shot("b20-page2")
    L.append("tapmid"); w(5000); shot("b21-save-dialog")
    L.append("savegame 43 darkside won"); w(3000)
    keys("esc", gap=6000); shot("b22-menu")
    out.append(("darkside", 42, L))

    # --- C: the Great Pyramid -> World ending ---------------------------
    L, w, dbg, keys, note, shot = run()
    w(8000); shot("c00-resumed")
    keys("esc", "esc", gap=2000)
    note("C1 the Great Pyramid: World ending (cutscene)")
    dbg("invincible", "map 129 13 15 1"); w(1500)
    shot("c01-pyramid")
    keys("space", gap=6000); shot("c02-pharaoh")
    L.append("tapmid"); w(5000)
    for i in range(8):
        w(30000); shot(f"c1{i}-world-ending")
    L.append("tapmid"); w(8000); shot("c20-end")
    out.append(("world", 43, L))
    return out


# ------------------------------------------------------------------ host


def host(outdir: pathlib.Path, start: str | None = None) -> int:
    saves_from = None
    names = [n for n, _, _ in phases()]
    for name, slot, cmds in phases():
        if start and names.index(name) < names.index(start):
            saves_from = outdir / name / "saves"
            continue
        d = outdir / name
        d.mkdir(parents=True, exist_ok=True)
        script = d / "script.txt"
        lines = []
        for c in cmds:
            if c == "tapmid":
                c = "tap 480 300"
            elif c.startswith("note "):
                c = "# " + c[5:]
            lines.append(c)
        script.write_text("\n".join(lines) + "\n")
        total = sum(int(c.split()[1]) for c in cmds if c.startswith("wait ")) + 30000
        env = dict(MMIV_LAYOUT="landscape", MMIV_TIMEOUT=str(total), MMIV_GDB="1")
        if saves_from:
            env["MMIV_SAVES_FROM"] = str(saves_from)
        else:
            env["MMIV_SAVES_FROM"] = str(d / "nosaves")
            (d / "nosaves").mkdir(exist_ok=True)
        args = [str(ROOT / "tools/run-host-script.sh"), str(script), str(d)]
        if slot is not None:
            args.append(str(slot))
        for attempt in range(3):
            print(f"== {name}: {len(cmds)} commands, {total // 1000} s", flush=True)
            r = subprocess.run(args, env={**os.environ, **env})
            # The debugger console can crash ScummVM's theme renderer as it
            # slides in (docs/speedrun-2026-11-02.md): run the segment again.
            if r.returncode == 0 and b"SIGFPE" not in (d / "run.log").read_bytes():
                break
            print(f"== {name}: crashed, again", flush=True)
        else:
            return 1
        saves_from = d / "saves"
    return 0


# ---------------------------------------------------------------- device

DEV_EVENT, DEV_DEBUGGER, DEV_KEY, DEV_TEXT, DEV_SAVE = 0x6D6D, 1, 2, 3, 4
NAMED = {"up": (273, 0), "down": (274, 0), "right": (275, 0), "left": (276, 0),
         "esc": (27, 27), "enter": (13, 13), "space": (32, 32), "tab": (9, 9),
         "backspace": (8, 8)}


def keycode(name: str) -> tuple[int, int]:
    """As keyByName() in backend/pocketbook/platform-host.cpp."""
    if name in NAMED:
        return NAMED[name]
    if name[0] == "f" and name[1:].isdigit() and 1 <= int(name[1:]) <= 12:
        n = int(name[1:])
        return 282 + n - 1, 315 + n - 1
    if len(name) == 1:
        return ord(name.lower()), ord(name)
    raise ValueError(f"unknown key {name}")


def device(outdir: pathlib.Path, agent_addr: str | None) -> int:
    sys.path.insert(0, str(pathlib.Path.home() / "Repos/pocketbook_lux_5/host"))
    from pbtk.device import Device  # noqa: E402

    dev = Device(source="cli", agent_addr=agent_addr, view="ccw")
    ag = dev.agent
    app = "/mnt/ext1/applications/mmiv"
    outdir.mkdir(parents=True, exist_ok=True)
    stop = threading.Event()

    def keep_awake():  # the agent bans sleep while a client is connected
        while not stop.wait(15):
            try:
                ag.ping()
            except Exception as e:  # reconnects on the next request
                print("ping:", e, flush=True)

    threading.Thread(target=keep_awake, daemon=True).start()
    ini = ag.get(f"{app}/scummvm.ini")
    (outdir / "scummvm.ini.player").write_bytes(ini)
    w, h = dev._canvas_size()
    log = open(outdir / "run.log", "a")

    def say(msg):
        line = time.strftime("%H:%M:%S ") + msg
        print(line, flush=True)
        log.write(line + "\n"); log.flush()

    def key(name):
        ag.event(DEV_EVENT, DEV_KEY, keycode(name)[0])  # the reader derives the ASCII

    def restart_into(slot):
        old = ag.info().get("started", "")
        if slot is not None:
            ag.put(f"{app}/resume", f"{slot}\n".encode())
        ag.restart()
        ag.wait_restarted(old, timeout=60)

    try:
        ag.put(f"{app}/savedir", b"speedrun\n")
        ag.put(f"{app}/layout", b"landscape\n")
        for name, slot, cmds in phases():
            say(f"== {name}")
            restart_into(slot)
            for c in cmds:
                op, _, arg = c.partition(" ")
                if op == "wait":
                    time.sleep(int(arg) / 1000)
                elif op == "note":
                    say(arg)
                elif op == "shot":
                    dev.screenshot(record=False, out=outdir / f"{arg}.png")
                elif op == "key":
                    key(arg)
                elif op == "debugger":
                    ag.event(DEV_EVENT, DEV_DEBUGGER, 0)
                elif op == "answer":
                    ag.event(DEV_EVENT, DEV_TEXT, 0)
                    for ch in arg:
                        key(ch if ch != " " else "space")
                    key("enter")
                elif op == "savegame":
                    ag.event(DEV_EVENT, DEV_SAVE, int(arg.split()[0]))
                elif op == "tapmid":
                    dev.tap(h // 2 if dev.view != "none" else w // 2, w // 2 if dev.view != "none" else h // 2)
                elif op == "tap":
                    x, y = map(int, arg.split())
                    dev.tap(x, y)
                else:
                    raise ValueError(c)
        say("== done")
        return 0
    finally:
        stop.set()
        say("cleanup: player's save folder and scummvm.ini back")
        try:
            ag.put(f"{app}/savedir", b"saves\n")
            ag.put(f"{app}/scummvm.ini", ini)
            restart_into(None)
        except Exception as e:
            say(f"cleanup failed: {e}")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("where", choices=["host", "device", "print"])
    p.add_argument("outdir", type=pathlib.Path, nargs="?")
    p.add_argument("--agent")
    p.add_argument("--from", dest="start", help="host: start at this phase (clouds, darkside, world)")
    a = p.parse_args()
    if a.where == "print":
        for name, slot, cmds in phases():
            print(f"# == {name} (from {'new game' if slot is None else f'slot {slot}'})")
            print("\n".join(cmds))
        return 0
    if a.outdir is None:
        p.error("OUTDIR is required")
    return host(a.outdir, a.start) if a.where == "host" else device(a.outdir, a.agent)


if __name__ == "__main__":
    sys.exit(main())
