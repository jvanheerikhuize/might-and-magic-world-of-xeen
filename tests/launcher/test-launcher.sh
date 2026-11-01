#!/usr/bin/env bash
# Checks packaging/mmiv.app picks the right save slot, using a stub in place
# of the scummvm binary. Runs the launcher with busybox sh when available
# (the reader's shell), else sh.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
mkdir -p "$t/mmiv/saves"
cp "$root/packaging/mmiv.app" "$t/"
printf '#!/bin/sh\necho "$@"\n' >"$t/mmiv/scummvm"
SH=$(command -v busybox >/dev/null && echo "busybox sh" || echo sh)

fail=0
expect() {  # description, expected substring or "!--save-slot"
	local got
	got=$($SH "$t/mmiv.app")
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
exit $fail
