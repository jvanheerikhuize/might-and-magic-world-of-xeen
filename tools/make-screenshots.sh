#!/usr/bin/env bash
# Regenerate the README screenshots (docs/screenshots/) with the host stub.
#
#   tools/make-screenshots.sh
#
# Needs: tools/build-host.sh pocketbook, game data in ~/games/xeen
# (tools/prepare-gog-data.sh) with a save in slot 0, and Pillow.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
MMIV_LAYOUT=landscape tools/run-host-script.sh tools/screenshots/game.txt build/shots-game 0
MMIV_LAYOUT=landscape tools/run-host-script.sh tools/screenshots/title.txt build/shots-title
python3 -I tools/compose-screenshots.py build/screenshots build/shots-game build/shots-title
for n in town street character credits scummvm-menu town-compare; do
	cp "build/screenshots/$n.png" docs/screenshots/
done
echo "screenshots in docs/screenshots/"
