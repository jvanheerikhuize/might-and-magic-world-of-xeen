#!/usr/bin/env bash
# Checks packaging/mmiv.app picks the right save slot, using a stub in place
# of the scummvm binary. Runs the launcher with busybox sh when available
# (the reader's shell), else sh.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
mkdir -p "$t/mmiv/saves"
cp "$root/packaging/mmiv.app" "$root/packaging/mmiv-keypad.app" "$t/"
printf '#!/bin/sh\necho "layout=$MMIV_LAYOUT $@"\n' >"$t/mmiv/scummvm"
SH=$(command -v busybox >/dev/null && echo "busybox sh" || echo sh)

fail=0
expect() {  # description, expected substring or "!--save-slot" [launcher]
	local got
	got=$(env -u MMIV_LAYOUT $SH "$t/${3:-mmiv.app}")
	if [[ $2 == "!--save-slot" ]]; then
		[[ $got != *--save-slot* ]] && echo "ok   $1" || { echo "FAIL $1: $got"; fail=1; }
	else
		[[ $got == *"$2"* ]] && echo "ok   $1" || { echo "FAIL $1: $got"; fail=1; }
	fi
}

expect "no saves: start at the title screen" "!--save-slot"
touch -d '2026-11-01 10:00' "$t/mmiv/saves/worldofxeen.000"
expect "only slot 0: resume slot 0" "--save-slot=0 "
touch -d '2026-11-01 11:00' "$t/mmiv/saves/worldofxeen.099"
expect "suspend save is newest: resume slot 99" "--save-slot=99 "
touch -d '2026-11-01 12:00' "$t/mmiv/saves/worldofxeen.003"
expect "newer manual save: resume slot 3" "--save-slot=3 "
expect "game id and paths passed" "--path=$t/mmiv/game"
expect "light GUI theme from the app folder" "--themepath=$t/mmiv --gui-theme=scummmodern"
expect "default layout: landscape" "layout=landscape "
expect "keypad launcher: portrait" "layout=portrait " mmiv-keypad.app
expect "keypad launcher resumes too" "--save-slot=3 " mmiv-keypad.app
echo "landscape-flip  # turned the other way" >"$t/mmiv/layout"
expect "layout file chooses the layout" "layout=landscape-flip "
expect "keypad launcher overrides the layout file" "layout=portrait " mmiv-keypad.app
exit $fail
