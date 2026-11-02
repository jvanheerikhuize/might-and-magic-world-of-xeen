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
tests/launcher/test-install-icons.sh

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
python3 -c "import gzip,sys; sys.exit(b\"Test Save\\0\" not in gzip.open(\"build/test-keyboard/saves/worldofxeen.001\").read(64))"
echo "ok   keyboard requested for a text field; the typed name \"Test Save\" saved slot 1"

echo "== delete a save by touch"
MMIV_SAVES_FROM=build/test-keyboard/saves MMIV_LAYOUT=landscape \
	tools/run-host-script.sh tests/host/delete-save.txt build/test-delete 0
test ! -e build/test-delete/saves/worldofxeen.001
test -f build/test-delete/saves/worldofxeen.000
echo "ok   slot 1 deleted from the load list; slot 0 kept"

echo "== dragging scrolls a list"
MMIV_LAYOUT=landscape tools/run-host-script.sh tests/host/drag-scroll.txt build/test-drag 0
# Text rows are too fine for same-shot.py's block signatures: compare pixels.
# A thumb drag that does nothing changes 0%; a scroll ~0.9%. Without moves the
# swipe is a tap that moves the selection highlight (~6%), hence the upper bound.
python3 -I tools/pixel-diff.py build/test-drag/dr01-top.pgm build/test-drag/dr02-thumb.pgm 0.3
python3 -I tools/pixel-diff.py build/test-drag/dr02-thumb.pgm build/test-drag/dr03-swipe.pgm 0.3 3
echo "ok   thumb drag and finger swipe both scroll the save list"

echo "== a burst of keys all reaches the game"
tools/run-host-script.sh tests/host/key-burst.txt build/test-burst 0
python3 -I tools/same-shot.py build/test-burst/b01-start.pgm build/test-burst/b03-full-circle.pgm

echo "== e-ink cues: automap arrow, dark position line, save list after death"
MMIV_LAYOUT=landscape tools/run-host-script.sh tests/host/eink-cues.txt build/test-cues 0
# A blinking arrow made two map shots differ; a steady one gives identical shots.
python3 -I tools/pixel-diff.py build/test-cues/c01-map.pgm build/test-cues/c02-map.pgm 0 0
# Without the cue a turn in the dark changes nothing (0%); the line ~0.3%.
python3 -I tools/pixel-diff.py build/test-cues/c03-dark.pgm build/test-cues/c04-dark-turned.pgm 0.1
grep -q 'The party died: opening the save list' build/test-cues/run.log
grep -q 'slot 5 "Alive again" saved' build/test-cues/run.log
echo "ok   steady automap arrow; position line in the dark; restore list after death"
