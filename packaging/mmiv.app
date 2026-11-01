#!/bin/sh
# mmIV launcher: the firmware lists *.app files in applications/.
# Starts ScummVM with World of Xeen from the bundle folder next to this file,
# resuming the most recently written save (the backend saves to slot 99 when
# the app goes to the background, PLAN.md Phase 3).
#
# Layout (backend/pocketbook/layout.h): MMIV_LAYOUT if set (mmiv-keypad.app
# sets portrait), else the first word of mmiv/layout if that file exists,
# else landscape full screen.
DIR="$(dirname "$0")/mmiv"
BIN="$DIR/scummvm"
chmod +x "$BIN" 2>/dev/null   # FAT storage does not keep execute bits
mkdir -p "$DIR/saves"

if [ -z "$MMIV_LAYOUT" ] && [ -f "$DIR/layout" ]; then
	read -r MMIV_LAYOUT _ <"$DIR/layout"
fi
export MMIV_LAYOUT="${MMIV_LAYOUT:-landscape}"

SLOT=""
LATEST=$(ls -t "$DIR"/saves/worldofxeen.[0-9][0-9][0-9] 2>/dev/null | head -n 1)
if [ -n "$LATEST" ]; then
	N=$(expr "${LATEST##*.}" + 0)
	SLOT="--save-slot=$N"
fi

cd "$DIR" || exit 1
exec "$BIN" --config="$DIR/scummvm.ini" --savepath="$DIR/saves" --path="$DIR/game" $SLOT mm:worldofxeen
