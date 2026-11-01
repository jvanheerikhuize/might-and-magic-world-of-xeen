#!/bin/sh
# mmIV in portrait with the 36-button touch keypad (game at 2x). Same game,
# saves and resume as mmiv.app; only the layout differs.
MMIV_LAYOUT=portrait
export MMIV_LAYOUT
exec sh "$(dirname "$0")/mmiv.app"
