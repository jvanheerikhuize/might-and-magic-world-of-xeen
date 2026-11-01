#!/usr/bin/env python3
"""List World of Xeen's maps (side, id, name) from the player's own game files.

    python3 -I tools/xeen-maps.py [GAME_DIR] [--json OUT]

GAME_DIR defaults to ~/games/xeen/scummvm/world-of-xeen (tools/prepare-gog-data.sh).
The ids are what the Xeen debugger's "map ID X Y SIDE" takes (side 0 = Clouds,
1 = Darkside); tools/make-probe-saves.py uses them.

Mirrors ScummVM's MM::Shared::Xeen::BaseCCArchive (index decryption, name hash,
0x35 data XOR) and Map::getMazeName (xeen0NNN.txt / darkxNNN.txt, a 33-byte
name). In World of Xeen both sides' name files live in dark.cc; xeen.cc is
searched as well for the separate releases.
"""
import json
import pathlib
import sys


def name_id(name):
    b = name.upper().encode()
    total = b[0]
    for c in b[1:]:
        total = ((total & 0x007F) << 9 | (total & 0xFF80) >> 7) & 0xFFFF
        total = (total + c) & 0xFFFF
    return total


def read_index(data):
    count = data[0] | data[1] << 8
    raw = bytearray(data[2:2 + count * 8])
    seed = 0xAC
    for i in range(len(raw)):
        b = raw[i]
        raw[i] = (((b << 2) | (b >> 6)) + seed) & 0xFF
        seed += 0x67
    out = {}
    for k in range(count):
        e = raw[k * 8:k * 8 + 8]
        out[e[0] | e[1] << 8] = (e[2] | e[3] << 8 | e[4] << 16, e[5] | e[6] << 8)
    return out


def member(data, index, name):
    e = index.get(name_id(name))
    if not e:
        return None
    off, size = e
    return bytes(b ^ 0x35 for b in data[off:off + size])


def maps(game_dir):
    archives = []
    for cc in ("dark.cc", "xeen.cc"):
        path = pathlib.Path(game_dir) / cc
        if path.exists():
            data = path.read_bytes()
            archives.append((data, read_index(data)))
    found = []
    for side, prefix in ((0, "xeen"), (1, "dark")):
        for mid in range(1, 256):
            fname = f"{prefix}{'x' if mid >= 100 else '0'}{mid:03d}.txt"
            for data, index in archives:
                txt = member(data, index, fname)
                if txt:
                    name = txt[:32].split(b"\0")[0].decode("latin-1").strip()
                    if name:
                        found.append({"side": side, "id": mid, "name": name})
                    break
    return found


if __name__ == "__main__":
    args = sys.argv[1:]
    out = None
    if "--json" in args:
        i = args.index("--json")
        out = args[i + 1]
        del args[i:i + 2]
    game = args[0] if args else str(pathlib.Path.home() / "games/xeen/scummvm/world-of-xeen")
    result = maps(game)
    for m in result:
        print(f"{'clouds' if m['side'] == 0 else 'dark'}\t{m['id']}\t{m['name']}")
    if out:
        pathlib.Path(out).write_text(json.dumps(result, indent=1) + "\n")
