#!/usr/bin/env bash
# All automated checks that need no reader:
#   1. unit tests of the backend's pure parts (gray mapping, refresh policy, keypad)
#   2. host-stub golden tests: scripted play from save slot 0, shot signatures
#      compared with tests/host/golden/ (needs ~/games/xeen with a slot-0 save
#      made in Vertigo; record new goldens with RECORD=1)
#   3. suspend/resume: the keypad test suspends (save to slot 99), then a new
#      run loads slot 99
# Build the host stub first: tools/build-host.sh pocketbook
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

echo "== unit"
make -s -C tests/unit

echo "== launcher"
tests/launcher/test-launcher.sh

if [[ ! -f $HOME/games/xeen/saves/worldofxeen.000 ]]; then
	echo "host tests skipped: no save slot 0 in ~/games/xeen/saves"
	exit 0
fi

golden() {  # script-name
	local name=$1 out=build/test-$1
	echo "== host golden: $name"
	tools/run-host-script.sh "tests/host/$name.txt" "$out" 0
	if [[ ${RECORD:-0} == 1 ]]; then
		python3 -I tools/check-golden.py "$out" "tests/host/golden/$name.json" --record
	else
		python3 -I tools/check-golden.py "$out" "tests/host/golden/$name.json"
	fi
}

golden smoke-from-save
golden keypad-and-suspend

echo "== suspend save + resume"
grep -q 'suspend: saved to slot 99' build/test-keypad-and-suspend/run.log
test -f build/test-keypad-and-suspend/saves/worldofxeen.099
MMIV_SAVES_FROM=build/test-keypad-and-suspend/saves \
	tools/run-host-script.sh tests/host/resume-99.txt build/test-resume 99
test -f build/test-resume/r01-resumed.pgm
echo "ok   slot 99 written on suspend and loads again"
