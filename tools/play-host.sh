#!/usr/bin/env bash
# Play the PC (SDL) build and record peak memory when you quit.
#
#   tools/play-host.sh [worldofxeen|cloudsofxeen]     default: worldofxeen
#
# Saves:        ~/games/xeen/saves
# Screenshots:  ~/games/xeen/screenshots   (Alt+S in game)
# Memory:       peak RSS is printed and appended to ~/games/xeen/rss.log
set -euo pipefail

game=${1:-worldofxeen}
root=$(cd "$(dirname "$0")/.." && pwd)
bin=$root/build/host-sdl/scummvm
data=$HOME/games/xeen
case $game in
worldofxeen) path=$data/scummvm/world-of-xeen ;;
cloudsofxeen) path=$data/scummvm/clouds-of-xeen ;;
*) echo "unknown game: $game" >&2; exit 2 ;;
esac

[[ -x $bin ]] || { echo "build first: tools/build-host.sh sdl" >&2; exit 1; }
mkdir -p "$data/saves" "$data/screenshots"

"$bin" --config="$data/scummvm-host.ini" \
	--extrapath="$root/scummvm/dists/engine-data" \
	--savepath="$data/saves" --screenshotpath="$data/screenshots" \
	--scale-factor=3 --path="$path" "mm:$game" &
pid=$!

# VmHWM is the kernel's own high-water mark; keep the last value seen.
peak=0
while kill -0 "$pid" 2>/dev/null; do
	hwm=$(awk '/VmHWM/ {print $2}' "/proc/$pid/status" 2>/dev/null || true)
	[[ -n $hwm ]] && peak=$hwm
	sleep 1
done
wait "$pid" && rc=0 || rc=$?
line="$(date -Iseconds) $game rc=$rc peak_rss_kb=$peak"
echo "$line" | tee -a "$data/rss.log"
