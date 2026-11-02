#!/usr/bin/env python3
"""Read World of Xeen map events (maze*.evt) straight from the game's .cc files.

    python3 -I tools/xeen-events.py GAMEDIR find OPCODE [OPCODE...]
    python3 -I tools/xeen-events.py GAMEDIR map SIDE MAPID [X Y]

`find` lists every event with one of the opcodes (side 0 = xeen.cc, the
Clouds; side 1 = dark.cc, Darkside). `map` prints a map's events, or only the
script at one square, with the map's text lines resolved where an opcode
shows text. Opcode names follow COMMAND_LIST in engines/mm/xeen/scripts.cpp.
"""
import pathlib
import sys

OPS = ["DoNothing", "Display1", "DoorTextSml", "DoorTextLrg", "SignText", "NPC", "PlayFX", "Teleport",
       "If", "If", "If", "MoveObj", "TakeOrGive", "DoNothing", "Remove", "SetChar", "Spawn",
       "DoTownEvent", "Exit", "AlterMap", "GiveMulti", "ConfirmWord", "Damage", "JumpRnd",
       "AlterEvent", "CallEvent", "Return", "SetVar", "TakeOrGive", "TakeOrGive", "CutsceneEndClouds",
       "Teleport", "WhoWill", "RndDamage", "MoveWallObj", "AlterCellFlag", "AlterHed", "DisplayStat",
       "TakeOrGive", "SignTextSml", "PlayEventVoc", "DisplayBottom", "IfMapFlag", "SelectRandomChar",
       "GiveEnchanted", "ItemType", "MakeNothingHere", "CheckProtection", "ChooseNumeric",
       "DisplayBottomTwoLines", "DisplayLarge", "ExchObj", "FallToMap", "DisplayMain", "Goto",
       "ConfirmWord", "GotoRandom", "CutsceneEndDarkside", "CutsceneEndWorld", "FlipWorld", "PlayCD"]
TEXT_OPS = {1, 2, 3, 4, 5, 21, 39, 41, 49, 50, 53, 55}


def name_id(name):
    if len(name) == 4:  # a resource number given directly, e.g. "2a0c"
        try:
            return int(name, 16)
        except ValueError:
            pass
    s = name.upper().encode()
    total = s[0]
    for c in s[1:]:
        total = ((total & 0x7F) << 9 | (total & 0xFF80) >> 7) & 0xFFFF
        total = (total + c) & 0xFFFF
    return total


class CC:
    """A .cc archive; data=bytes reads a nested one (no XOR on its data)."""

    def __init__(self, path=None, data=None):
        self.data = pathlib.Path(path).read_bytes() if path else data
        self.xor = 0x35 if path else 0
        count = int.from_bytes(self.data[:2], "little")
        raw = bytearray(self.data[2:2 + count * 8])
        seed = 0xAC
        for i in range(len(raw)):
            b = raw[i]
            raw[i] = ((((b << 2) | (b >> 6)) & 0xFF) + seed) & 0xFF
            seed = (seed + 0x67) & 0xFFFF
        self.index = {}
        for i in range(count):
            e = raw[i * 8:i * 8 + 8]
            self.index[int.from_bytes(e[0:2], "little")] = (
                int.from_bytes(e[2:5], "little"), int.from_bytes(e[5:7], "little"))

    def read(self, name):
        hit = self.index.get(name_id(name))
        if not hit:
            return None
        off, size = hit
        return bytes(b ^ self.xor for b in self.data[off:off + size])

    def save_archive(self):
        """The starting game state (maze*.evt and friends), as SaveArchive::reset builds it."""
        parts = [self.read(f"{r:04x}") for r in (0x2A0C, 0x2A1C, 0x2A2C, 0x2A3C, 0x284C, 0x2A5C)]
        return CC(data=b"".join(p for p in parts if p))


def events(cc, mid):
    pre = "x" if mid >= 100 else "0"
    raw = cc.save.read(f"maze{pre}{mid:03d}.evt")
    txt = cc.read(f"aaze{pre}{mid:03d}.txt") or b""
    lines = [t.decode("latin-1") for t in txt.split(b"\0")]
    out, p = [], 0
    while raw and p < len(raw):
        n = raw[p]
        x, y, d, line, op = raw[p + 1:p + 6]
        out.append((x, y, d, line, op, list(raw[p + 6:p + 1 + n])))
        p += 1 + n
    return out, lines


def monsters(cc, mid):
    """[(x, y, sprite id)] of a map's monsters, from maze*.mob."""
    pre = "x" if mid >= 100 else "0"
    raw = cc.save.read(f"maze{pre}{mid:03d}.mob")
    if not raw:
        return []
    sprites = [b for b in raw[16:32] if b != 0xFF]
    p, out = 48, []
    for part in range(2):  # objects, then monsters
        while p + 4 <= len(raw):
            x, y, i = int.from_bytes(raw[p:p + 1], "little", signed=True), \
                int.from_bytes(raw[p + 1:p + 2], "little", signed=True), raw[p + 2]
            p += 4
            if x == -1 and y == -1 and i == 0xFF:
                break
            if part == 1:
                out.append((x, y, sprites[i] if i < len(sprites) else -1))
    return out


def show(e, lines):
    x, y, d, line, op, params = e
    name = OPS[op] if op < len(OPS) else f"op{op}"
    s = f"  ({x:3d},{y:3d}) dir {d} line {line:2d}  {op:2d} {name:20s} {params}"
    if op in TEXT_OPS and params and params[0] < len(lines):
        s += f"  \"{lines[params[0]][:70]}\""
    return s


def main():
    game = pathlib.Path(sys.argv[1])
    ccs = [CC(game / "xeen.cc"), CC(game / "dark.cc")]
    for cc in ccs:
        cc.save = cc.save_archive()
    cmd = sys.argv[2]
    if cmd == "find":
        want = {int(a) for a in sys.argv[3:]}
        for side, cc in enumerate(ccs):
            for mid in range(1, 160):
                evs, lines = events(cc, mid)
                for e in evs:
                    if e[4] in want:
                        print(f"side {side} map {mid}:")
                        print(show(e, lines))
    elif cmd == "monster":  # monster SPRITE: where it stands, on every map
        want = int(sys.argv[3])
        for side, cc in enumerate(ccs):
            for mid in range(1, 160):
                for x, y, spr in monsters(cc, mid):
                    if spr == want:
                        print(f"side {side} map {mid}: ({x},{y})")
    elif cmd == "map":
        side, mid = int(sys.argv[3]), int(sys.argv[4])
        evs, lines = events(ccs[side], mid)
        at = (int(sys.argv[5]), int(sys.argv[6])) if len(sys.argv) > 6 else None
        for e in evs:
            if at is None or (e[0], e[1]) == at:
                print(show(e, lines))


if __name__ == "__main__":
    main()
