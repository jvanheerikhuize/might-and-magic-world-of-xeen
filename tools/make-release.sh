#!/usr/bin/env bash
# Package a release zip for the reader. Contains no game data.
#
#   tools/make-release.sh VERSION        e.g. tools/make-release.sh 1.0.0
#
# Needs a device build (tools/build-device.sh pocketbook). Output:
#   dist/mmiv-VERSION-pocketbook.zip
#     applications/mmiv.app              launcher
#     applications/mmiv/scummvm          ScummVM with the PocketBook backend (Xeen only)
#     applications/mmiv/mm.dat           ScummVM engine data
#     applications/mmiv/scummmodern.zip, gui-icons.dat   light GUI theme
#     applications/mmiv/game/            put xeen.cc, dark.cc, intro.cc here
#     applications/icons/mmiv.bmp, mmiv_f.bmp
#     install-icons.py                   optional: home-screen icon (run on a PC)
#     INSTALL.txt, LICENSE, SOURCE.txt
set -euo pipefail

version=${1:?usage: $0 VERSION}
root=$(cd "$(dirname "$0")/.." && pwd)
bin=$root/build/device-pocketbook/scummvm.stripped
[[ -f $bin ]] || { echo "build first: tools/build-device.sh pocketbook" >&2; exit 1; }

stage=$root/build/release/mmiv-$version
rm -rf "$root/build/release"
mkdir -p "$stage/applications/mmiv/game" "$stage/applications/icons"
cp "$root/packaging/mmiv.app" "$stage/applications/"
cp "$bin" "$stage/applications/mmiv/scummvm"
cp "$root/scummvm/dists/engine-data/mm.dat" "$stage/applications/mmiv/"
cp "$root/scummvm/gui/themes/scummmodern.zip" "$root/scummvm/gui/themes/gui-icons.dat" "$stage/applications/mmiv/"
cp "$root/packaging/icons/mmiv.bmp" "$root/packaging/icons/mmiv_f.bmp" "$stage/applications/icons/"
cp "$root/tools/install-icons.py" "$root/LICENSE" "$stage/"
cat >"$stage/applications/mmiv/game/PUT-GAME-FILES-HERE.txt" <<'EOF'
Copy XEEN.CC, DARK.CC and INTRO.CC from your own copy of
Might and Magic: World of Xeen (GOG) into this folder; see INSTALL.txt.
EOF

commit=$(git -C "$root" rev-parse HEAD)
scummvm_commit=$(git -C "$root/scummvm" rev-parse HEAD)
cat >"$stage/SOURCE.txt" <<EOF
mmIV $version - ScummVM for PocketBook e-readers, built for Might and Magic: World of Xeen.

The scummvm binary is ScummVM (GPL-3.0-or-later) with the mmIV PocketBook
backend and patches. Complete source:
  https://github.com/jvanheerikhuize/mmIV/tree/v$version   (commit $commit)
  ScummVM commit $scummvm_commit (the repo's scummvm submodule) plus patches/
Build instructions: README.md in that repository.
EOF

cat >"$stage/INSTALL.txt" <<'EOF'
mmIV - Might and Magic: World of Xeen on a PocketBook e-reader
===============================================================

Tested on a PocketBook Touch Lux 5 (PB628), firmware 6.x. Other 1024x758
PocketBooks may work.

1. Connect the reader to your computer over USB (PC link).
2. Copy the "applications" folder from this zip onto the reader's internal
   storage, merging it with the "applications" folder that is already there.
3. Copy the game files from your own World of Xeen into
   applications/mmiv/game/ on the reader. From the GOG "Might and Magic 4-5"
   installer, extracted with innoextract (innoextract setup_*.exe -d xeen):
       xeen/__support/save/XEEN.CC
       xeen/__support/save/DARK.CC
       xeen/INTRO.CC
   (XEEN.CC and DARK.CC from __support/save/, not from the top folder.)
4. Eject the reader. "mmiv" is now in the Applications list.

Optional: a home-screen icon. With the reader still connected, run on your
computer (Python 3):
    python3 install-icons.py /path/to/the/reader
It adds one entry to system/config/desktop/view.json (a backup is kept as
view.json.mmiv-backup). Restart the reader to see it.

Playing: hold the reader sideways (turned a quarter counter-clockwise, buttons
on the right) and tap the game. MENU = Escape, hold MENU = ScummVM menu
(save/load). The game saves when you leave it and resumes where you were.
More: https://github.com/jvanheerikhuize/mmIV
EOF

mkdir -p "$root/dist"
out=$root/dist/mmiv-$version-pocketbook.zip
rm -f "$out"
(cd "$root/build/release" && zip -qr "$out" "mmiv-$version")
echo "release: $out ($(du -h "$out" | cut -f1))"
