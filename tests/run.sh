#!/usr/bin/env bash
# All automated checks that need no reader:
#   1. unit tests of the backend's pure parts (gray mapping, refresh policy, keypad)
#   2. host-stub golden tests: scripted play from save slot 0, shot signatures
#      compared with tests/host/golden/ (needs ~/games/xeen with a slot-0 save
#      made in Vertigo; record new goldens with RECORD=1). The signatures are
#      size-independent, so landscape (3x) runs check against the same goldens
#      as portrait (2x), and touch-only play against the keyboard run.
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

golden() {  # layout script-name [golden-name]
	local layout=$1 name=$2 ref=${3:-$2} out=build/test-$2-$1
	echo "== host golden: $name ($layout)"
	MMIV_LAYOUT=$layout tools/run-host-script.sh "tests/host/$name.txt" "$out" 0
	if [[ ${RECORD:-0} == 1 && $name == "$ref" && $layout == portrait ]]; then
		python3 -I tools/check-golden.py "$out" "tests/host/golden/$ref.json" --record
	else
		python3 -I tools/check-golden.py "$out" "tests/host/golden/$ref.json"
	fi
}

golden portrait smoke-from-save
golden landscape smoke-from-save
golden landscape touch-only-landscape smoke-from-save
golden portrait keypad-and-suspend

echo "== suspend save + resume"
grep -q 'suspend: saved to slot 99' build/test-keypad-and-suspend-portrait/run.log
test -f build/test-keypad-and-suspend-portrait/saves/worldofxeen.099
MMIV_SAVES_FROM=build/test-keypad-and-suspend-portrait/saves \
	tools/run-host-script.sh tests/host/resume-99.txt build/test-resume 99
test -f build/test-resume/r01-resumed.pgm
echo "ok   slot 99 written on suspend and loads again"

echo "== rolling safety save"
MMIV_SAFETY_SAVE_MS=4000 tools/run-host-script.sh tests/host/safety-save.txt build/test-safety 0
grep -q 'safety save: saved to slot 99' build/test-safety/run.log
test -f build/test-safety/saves/worldofxeen.099
echo "ok   $(grep -c 'safety save: saved' build/test-safety/run.log) safety save(s) in ~9 s at a 4 s interval"

echo "== text input opens the keyboard"
MMIV_LAYOUT=landscape tools/run-host-script.sh tests/host/keyboard-save.txt build/test-keyboard 0
grep -q 'text input on' build/test-keyboard/present.log
grep -q 'text input off' build/test-keyboard/present.log
test -f build/test-keyboard/saves/worldofxeen.001
echo "ok   keyboard requested for a text field; the typed name saved slot 1"
