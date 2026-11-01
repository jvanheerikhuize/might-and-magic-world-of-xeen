#!/usr/bin/env bash
# Assemble the reader bundle and optionally install it.
#
#   tools/make-bundle.sh [--install]
#
# Bundle (build/bundle/applications/):
#   mmiv.app             launcher: landscape full screen (packaging/mmiv.app)
#   mmiv-keypad.app      launcher: portrait with the touch keypad
#   mmiv/scummvm         device build (tools/build-device.sh pocketbook)
#   mmiv/mm.dat          ScummVM engine data
#   mmiv/scummmodern.zip, gui-icons.dat   light GUI theme (readable on e-ink)
#   mmiv/game/           xeen.cc dark.cc intro.cc from ~/games/xeen (your GOG copy)
#   mmiv/saves/          copied from ~/games/xeen/saves on first install only
# --install copies it to the reader's internal storage (READER, default
# /run/media/$USER/PB628) without overwriting saves already on the reader,
# then installs the launcher icons (tools/install-icons.py).
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
data=$HOME/games/xeen
bin=$root/build/device-pocketbook/scummvm.stripped
out=$root/build/bundle/applications
reader=${READER:-/run/media/$USER/PB628}

[[ -f $bin ]] || { echo "build first: tools/build-device.sh pocketbook" >&2; exit 1; }
[[ -f $data/scummvm/world-of-xeen/xeen.cc ]] || { echo "game data missing: tools/prepare-gog-data.sh" >&2; exit 1; }

rm -rf "$root/build/bundle"
mkdir -p "$out/mmiv/game" "$out/mmiv/saves"
cp "$root/packaging/mmiv.app" "$root/packaging/mmiv-keypad.app" "$out/"
cp "$bin" "$out/mmiv/scummvm"
cp "$root/scummvm/dists/engine-data/mm.dat" "$out/mmiv/"
cp "$root/scummvm/gui/themes/scummmodern.zip" "$root/scummvm/gui/themes/gui-icons.dat" "$out/mmiv/"
cp -L "$data"/scummvm/world-of-xeen/*.cc "$out/mmiv/game/"
cp "$data"/saves/worldofxeen.* "$out/mmiv/saves/" 2>/dev/null || true
echo "bundle: $(du -sh "$out" | cut -f1) in $out"

if [[ ${1:-} == --install ]]; then
	[[ -d $reader/applications ]] || { echo "reader not mounted at $reader" >&2; exit 1; }
	dst=$reader/applications
	mkdir -p "$dst/mmiv/game" "$dst/mmiv/saves"
	cp "$out/mmiv.app" "$out/mmiv-keypad.app" "$dst/"
	cp "$out/mmiv/scummvm" "$out/mmiv/mm.dat" "$out/mmiv/scummmodern.zip" "$out/mmiv/gui-icons.dat" "$dst/mmiv/"
	cp "$out"/mmiv/game/* "$dst/mmiv/game/"
	# Saves: never overwrite what the reader already has.
	for s in "$out"/mmiv/saves/*; do
		[[ -e $s ]] || continue
		[[ -e $dst/mmiv/saves/$(basename "$s") ]] || cp "$s" "$dst/mmiv/saves/"
	done
	sync
	for f in mmiv.app mmiv-keypad.app mmiv/scummvm mmiv/mm.dat mmiv/scummmodern.zip mmiv/game/xeen.cc mmiv/game/dark.cc; do
		cmp -s "$out/$f" "$dst/$f" || { echo "VERIFY FAILED: $f" >&2; exit 1; }
	done
	echo "installed and verified in $dst"
	# Launcher icons and names (adds our entries to the firmware's view.json).
	python3 -I "$root/tools/install-icons.py" "$reader"
fi
