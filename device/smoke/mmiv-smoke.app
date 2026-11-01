#!/bin/sh
# Launcher entry: the firmware lists *.app files in applications/.
# The FAT storage does not keep execute bits, so set them before starting.
BIN="$(dirname "$0")/mmiv-smoke/mmiv-smoke"
chmod +x "$BIN" 2>/dev/null
exec "$BIN"
