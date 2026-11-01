#!/usr/bin/env bash
# All automated checks that need no reader:
#   1. unit tests of the backend's pure parts (gray mapping, refresh policy)
#   2. host-stub golden test: scripted play from save slot 0, shot signatures
#      compared with tests/host/golden/ (needs ~/games/xeen with a slot-0 save
#      made in Vertigo; record new goldens with RECORD=1)
# Build the host stub first: tools/build-host.sh pocketbook
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

echo "== unit"
make -s -C tests/unit

echo "== host golden: smoke-from-save"
if [[ ! -f $HOME/games/xeen/saves/worldofxeen.000 ]]; then
	echo "skipped: no save slot 0 in ~/games/xeen/saves"
	exit 0
fi
tools/run-host-script.sh tests/host/smoke-from-save.txt build/test-smoke 0
if [[ ${RECORD:-0} == 1 ]]; then
	python3 -I tools/check-golden.py build/test-smoke tests/host/golden/smoke-from-save.json --record
else
	python3 -I tools/check-golden.py build/test-smoke tests/host/golden/smoke-from-save.json
fi
