#!/usr/bin/env python3
"""Give the reader's mmiv launchers their icons and names.

    python3 -I tools/install-icons.py READER_ROOT [--dry-run]

READER_ROOT is the mounted internal storage (/mnt/ext1 on the device), e.g.
/run/media/$USER/PB628. What it changes (PocketBook firmware 6.x convention,
MobileRead wiki "Pocketbook Configuration"):

  applications/icons/NAME.bmp, NAME_f.bmp      copied from packaging/icons/
  system/config/desktop/view.json              adds "U_NAME" app entries (path,
                                               title, icon, focused_icon) and
                                               lists them in the "@Games"
                                               group (else a group with other
                                               U_ apps, else the first)

The app path must be absolute (/mnt/ext1/applications/NAME.app): with a path
relative to the storage root the reader showed the icon but tapping it did
nothing (PB628, firmware U628.6.9). Icons as object paths do work.

Only our own keys are added; nothing else is changed. Launchers we no longer
ship (OBSOLETE) are removed: view.json entry, icons and .app file. view.json is
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
]
# Launchers we used to install: removed from view.json and the reader.
OBSOLETE = ["mmiv-keypad"]
DEVICE_ROOT = "/mnt/ext1"


def fail(msg):
    print("install-icons:", msg, file=sys.stderr)
    sys.exit(1)


def update_view(path, dry):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    try:
        view = json.loads(text)
    except ValueError as e:
        fail(f"{path} is not valid JSON ({e}); not touching it")
    # Firmware 6.x layout (its own _comment documents it): "applications" maps
    # app ids to definitions, "view" -> "groups" lists what the launcher shows.
    table = view.get("applications")
    groups = view.get("view", {}).get("groups")
    if not isinstance(table, dict) or not isinstance(groups, list) or not groups:
        fail(f"{path}: unexpected layout (no applications table or view groups); not touching it")
    home = next((g for g in groups if g.get("title") == "@Games"), None)
    home = home or next((g for g in groups if any(str(a).startswith("U_") for a in g.get("apps", []))), groups[0])
    changed = False
    for name, title in APPS:
        key = "U_" + name
        entry = {
            "path": f"{DEVICE_ROOT}/applications/{name}.app",
            "title": title,
            "icon": {"path": f"applications/icons/{name}.bmp"},
            "focused_icon": {"path": f"applications/icons/{name}_f.bmp"},
        }
        if table.get(key) != entry:
            table[key] = entry
            changed = True
        if not any(key in g.get("apps", []) for g in groups):
            home.setdefault("apps", []).append(key)
            changed = True
    for name in OBSOLETE:
        key = "U_" + name
        if key in table:
            del table[key]
            changed = True
        for g in groups:
            if key in g.get("apps", []):
                g["apps"].remove(key)
                changed = True
    if not changed:
        print("view.json: already up to date")
        return
    out = json.dumps(view, indent=4, ensure_ascii=False)
    json.loads(out)
    if dry:
        print("view.json: would update", ", ".join("U_" + n for n, _ in APPS), "in group", home.get("title"))
        return
    backup = path + ".mmiv-backup"
    if not os.path.exists(backup):
        shutil.copy2(path, backup)
    tmp = path + ".mmiv-tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(out)
    os.replace(tmp, path)
    print(f"view.json: updated our apps in {home.get('title')} (backup: {os.path.basename(backup)})")


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
    for name in OBSOLETE:
        for f in (f"icons/{name}.bmp", f"icons/{name}_f.bmp", f"{name}.app"):
            p = os.path.join(root, "applications", f)
            if os.path.isfile(p):
                print("removing obsolete", "applications/" + f)
                if not dry:
                    os.remove(p)
    update_view(view, dry)
    return 0


if __name__ == "__main__":
    sys.exit(main())
