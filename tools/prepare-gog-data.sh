#!/usr/bin/env bash
# Unpack the GOG "Might and Magic 4-5 - World of Xeen" installer and lay out
# the data files the way ScummVM's Xeen engine expects them.
#
#   tools/prepare-gog-data.sh <setup_might_and_magic_4-5_...exe> [out-dir]
#
# Needs: innoextract (sudo apt install innoextract). Nothing Windows runs.
# Output (default ~/games/xeen):
#   <out>/                         raw installer contents (originals, untouched)
#   <out>/scummvm/world-of-xeen/   xeen.cc dark.cc intro.cc  -> game id worldofxeen
#   <out>/scummvm/clouds-of-xeen/  xeen.cc intro.cc          -> game id cloudsofxeen
# The game data is copyrighted: keep it out of this repo.
set -euo pipefail

installer=${1:?usage: $0 <gog-installer.exe> [out-dir]}
out=${2:-$HOME/games/xeen}

command -v innoextract >/dev/null || { echo "innoextract not found: sudo apt install innoextract" >&2; exit 1; }

mkdir -p "$out"
innoextract --silent --exclude-temp --output-dir "$out" "$installer"

# ScummVM detection = MD5 of the first 5000 bytes + exact size
# (engines/mm/detection_tables.h, entry "World of Xeen", EN, ADGF_NO_FLAGS).
check() { # file md5 size
	local got_md5 got_size
	got_md5=$(head -c 5000 "$1" | md5sum | cut -c1-32)
	got_size=$(stat -c %s "$1")
	if [[ $got_md5 == "$2" && $got_size == "$3" ]]; then
		echo "ok   $1"
	else
		echo "FAIL $1 (md5 $got_md5 size $got_size; want $2 $3)" >&2
		return 1
	fi
}
check "$out/__support/save/XEEN.CC" 0cffbab533d9afe140e69ec93096f43e 13435646
check "$out/__support/save/DARK.CC" df194483ecea6abc0511637d712ced7c 11217676

world=$out/scummvm/world-of-xeen
clouds=$out/scummvm/clouds-of-xeen
mkdir -p "$world" "$clouds"
ln -f "$out/__support/save/XEEN.CC" "$world/xeen.cc"
ln -f "$out/__support/save/DARK.CC" "$world/dark.cc"
ln -f "$out/INTRO.CC" "$world/intro.cc"
ln -f "$out/__support/save/XEEN.CC" "$clouds/xeen.cc"
ln -f "$out/INTRO.CC" "$clouds/intro.cc"

echo "ScummVM data ready:"
echo "  $world   (World of Xeen: Clouds + Darkside)"
echo "  $clouds  (Clouds of Xeen only)"
echo "ScummVM also needs its own engine file mm.dat (ships with ScummVM, dists/engine-data/)."
