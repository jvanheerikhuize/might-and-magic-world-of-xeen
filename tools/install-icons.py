#!/usr/bin/env python3
"""Give the reader's mmiv launchers their icons and names.

    python3 -I tools/install-icons.py READER_ROOT [--dry-run]

READER_ROOT is the mounted internal storage (/mnt/ext1 on the device), e.g.
/run/media/$USER/PB628. What it changes (PocketBook firmware 6.x convention,
MobileRead wiki "Pocketbook Configuration"):

  applications/icons/NAME.bmp, NAME_f.bmp      copied from packaging/icons/
  system/config/desktop/view.json              adds "U_NAME" app entries (path,
                                               title, icon, focused_icon) and
                                               lists them in the group that
                                               holds the other U_ apps, else
                                               the first group
  system/language/LANG.txt                     adds "@NAME=Title" lines

Only our own keys and lines are added; nothing else is changed. view.json is
backed up once to view.json.mmiv-backup, must parse as JSON before and after,
and is left alone if anything looks unexpected. Run it again any time: it is
idempotent. The new icons appear after the reader restarts.
"""
import json
import os
import shutil
import sys

APPS = [
    ("mmiv", "Might and Magic IV"),
    ("mmiv-keypad", "Might and Magic IV (keypad)"),
]
DEVICE_ROOT = "/mnt/ext1"


def fail(msg):
    print("install-icons:", msg, file=sys.stderr)
    sys.exit(1)


def find_app_table(node):
    """The dict whose values are app definitions ({"path": ..., ...})."""
    if isinstance(node, dict):
        defs = [v for v in node.values() if isinstance(v, dict) and "path" in v]
        if len(defs) >= 2:
            return node
        for v in node.values():
            t = find_app_table(v)
            if t is not None:
                return t
    elif isinstance(node, list):
        for v in node:
            t = find_app_table(v)
            if t is not None:
                return t
    return None


def find_groups(node, out):
    """Every list called "apps" (the groups the launcher shows)."""
    if isinstance(node, dict):
        for k, v in node.items():
            if k == "apps" and isinstance(v, list) and all(isinstance(x, str) for x in v):
                out.append(v)
            else:
                find_groups(v, out)
    elif isinstance(node, list):
        for v in node:
            find_groups(v, out)
    return out


def update_view(path, dry):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    try:
        view = json.loads(text)
    except ValueError as e:
        fail(f"{path} is not valid JSON ({e}); not touching it")
    table = find_app_table(view)
    groups = find_groups(view, [])
    if table is None or not groups:
        fail(f"{path}: no app table or app groups found; not touching it")
    changed = False
    for name, title in APPS:
        key = "U_" + name
        entry = {
            "path": f"{DEVICE_ROOT}/applications/{name}.app",
            "title": title,
            "icon": f"{DEVICE_ROOT}/applications/icons/{name}.bmp",
            "focused_icon": f"{DEVICE_ROOT}/applications/icons/{name}_f.bmp",
        }
        if table.get(key) != entry:
            table[key] = entry
            changed = True
        if not any(key in g for g in groups):
            home = next((g for g in groups if any(x.startswith("U_") for x in g)), groups[0])
            home.append(key)
            changed = True
    if not changed:
        print("view.json: already up to date")
        return
    out = json.dumps(view, indent=4, ensure_ascii=False) + "\n"
    json.loads(out)
    if dry:
        print("view.json: would add", ", ".join("U_" + n for n, _ in APPS))
        return
    backup = path + ".mmiv-backup"
    if not os.path.exists(backup):
        shutil.copy2(path, backup)
    tmp = path + ".mmiv-tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(out)
    os.replace(tmp, path)
    print(f"view.json: added our apps (backup: {os.path.basename(backup)})")


def device_language(root):
    cfg = os.path.join(root, "system", "config", "global.cfg")
    try:
        with open(cfg, encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith("language="):
                    return line.split("=", 1)[1].strip() or "en"
    except OSError:
        pass
    return "en"


def update_names(root, dry):
    lang = device_language(root)
    path = os.path.join(root, "system", "language", lang + ".txt")
    existing = ""
    if os.path.exists(path):
        with open(path, encoding="utf-8", errors="replace") as f:
            existing = f.read()
    keys = {line.split("=", 1)[0] for line in existing.splitlines() if "=" in line}
    add = [f"@{n}={t}" for n, t in APPS if "@" + n not in keys]
    if not add:
        print(f"{lang}.txt: names already there")
        return
    if dry:
        print(f"{lang}.txt: would add", "; ".join(add))
        return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "a", encoding="utf-8") as f:
        if existing and not existing.endswith("\n"):
            f.write("\n")
        f.write("\n".join(add) + "\n")
    print(f"{lang}.txt: added {len(add)} name(s)")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    root, dry = sys.argv[1], "--dry-run" in sys.argv[2:]
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "packaging", "icons")
    view = os.path.join(root, "system", "config", "desktop", "view.json")
    if not os.path.isdir(os.path.join(root, "applications")):
        fail(f"{root}: no applications folder (reader not mounted?)")
    if not os.path.isfile(view):
        fail(f"{view} not found; this firmware may not support custom icons")
    icons = os.path.join(root, "applications", "icons")
    if not dry:
        os.makedirs(icons, exist_ok=True)
    for name, _ in APPS:
        for suffix in ("", "_f"):
            f = f"{name}{suffix}.bmp"
            if not dry:
                shutil.copyfile(os.path.join(src, f), os.path.join(icons, f))
    print("icons:", "would copy" if dry else "copied", 2 * len(APPS), "files to applications/icons/")
    update_view(view, dry)
    update_names(root, dry)
    return 0


if __name__ == "__main__":
    sys.exit(main())
