#!/bin/sh
# mmIV launcher: the firmware lists *.app files in applications/.
# Starts ScummVM with World of Xeen from the bundle folder next to this file,
# resuming the most recently written save (the backend saves to slot 99 when
# the app goes to the background, PLAN.md Phase 3).
#
# Layout (backend/pocketbook/layout.h): MMIV_LAYOUT if set, else the first
# word of mmiv/layout if that file exists (landscape, landscape-flip, or
# portrait for the touch keypad), else landscape full screen. A value that
# came from the file is read again on a restart in place (pbtk restart runs
# this launcher with our own environment).
#
# Saves: mmiv/saves, or the folder named by the first word of mmiv/savedir
# (letters, digits, - and _ only), e.g. a separate set for testing that
# leaves the player's saves and resume point alone.
# Canonical path: a restart in place runs us as .../mmiv/../mmiv.app.
DIR=$(cd "$(dirname "$0")/mmiv" 2>/dev/null && pwd -P) || DIR="$(dirname "$0")/mmiv"
BIN="$DIR/scummvm"
chmod +x "$BIN" 2>/dev/null   # FAT storage does not keep execute bits

[ "$MMIV_LAYOUT_FROM" = file ] && MMIV_LAYOUT=""
if [ -z "$MMIV_LAYOUT" ] && [ -f "$DIR/layout" ]; then
	read -r MMIV_LAYOUT _ <"$DIR/layout"
	export MMIV_LAYOUT_FROM=file
fi
export MMIV_LAYOUT="${MMIV_LAYOUT:-landscape}"

SAVES=saves
if [ -f "$DIR/savedir" ]; then
	read -r WANT _ <"$DIR/savedir"
	case "$WANT" in
	"" | *[!A-Za-z0-9_-]*) ;;
	*) SAVES=$WANT ;;
	esac
fi
mkdir -p "$DIR/$SAVES"

SLOT=""
LATEST=$(ls -t "$DIR/$SAVES"/worldofxeen.[0-9][0-9][0-9] 2>/dev/null | head -n 1)
if [ -n "$LATEST" ]; then
	N=$(expr "${LATEST##*.}" + 0)
	SLOT="--save-slot=$N"
fi
# mmiv/resume: start this slot once instead (pbtk scenarios: "open the game
# at save N"); the file is removed so the next start resumes as usual.
if [ -f "$DIR/resume" ]; then
	read -r WANT _ <"$DIR/resume"
	rm -f "$DIR/resume"
	case "$WANT" in
	"" | *[!0-9]*) ;;
	*) [ -f "$DIR/$SAVES/worldofxeen.$(printf %03d "$WANT")" ] && SLOT="--save-slot=$(expr "$WANT" + 0)" ;;
	esac
fi

cd "$DIR" || exit 1
# The light ScummVM theme reads far better on e-ink than the built-in dark one.
exec "$BIN" --config="$DIR/scummvm.ini" --savepath="$DIR/$SAVES" --path="$DIR/game" \
	--themepath="$DIR" --gui-theme=scummmodern $SLOT mm:worldofxeen
