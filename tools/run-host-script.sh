#!/usr/bin/env bash
# Run the pocketbook backend's host stub headless from an input script.
#
#   tools/run-host-script.sh SCRIPT OUTDIR [SAVE_SLOT]
#
# Plays World of Xeen from ~/games/xeen (tools/prepare-gog-data.sh); with
# SAVE_SLOT it loads that save from ~/games/xeen/saves at start-up.
# OUTDIR receives NAME.pgm per "shot", present.log, run.log and NAME.png
# copies of the shots (when Pillow is available).
# Script syntax: backend/pocketbook/platform-host.cpp.
set -euo pipefail

script=$(realpath "${1:?usage: $0 SCRIPT OUTDIR [SAVE_SLOT]}")
out=${2:?usage: $0 SCRIPT OUTDIR [SAVE_SLOT]}
slot=${3:-}
root=$(cd "$(dirname "$0")/.." && pwd)
bin=$root/build/host-pocketbook/scummvm
data=$HOME/games/xeen

[[ -x $bin ]] || { echo "build first: tools/build-host.sh pocketbook" >&2; exit 1; }
mkdir -p "$out"
out=$(realpath "$out")
rm -rf "$out"/*.pgm "$out"/*.png "$out"/present.log "$out"/saves
# Work on a copy of the saves so test runs never change the real ones.
# MMIV_SAVES_FROM=DIR starts from another run's saves instead (resume tests).
mkdir -p "$out/saves"
cp "${MMIV_SAVES_FROM:-$data/saves}"/* "$out/saves/" 2>/dev/null || true

# Same GUI theme as the reader bundle (light, readable on e-ink).
args=(--config="$out/scummvm.ini" --path="$data/scummvm/world-of-xeen" --savepath="$out/saves"
	--themepath="$root/scummvm/gui/themes" --gui-theme=scummmodern)
[[ -n $slot ]] && args+=(--save-slot="$slot")

set +e
MMIV_SCRIPT=$script MMIV_OUT=$out MMIV_TIMEOUT=${MMIV_TIMEOUT:-120000} \
	MMIV_APPDIR=$root/scummvm/dists/engine-data \
	"$bin" "${args[@]}" mm:worldofxeen >"$out/run.log" 2>&1
rc=$?
set -e
echo "exit $rc; shots: $(ls "$out"/*.pgm 2>/dev/null | wc -l); $(grep '^summary' "$out/present.log" 2>/dev/null || echo 'no summary')"

if python3 -I -c 'import PIL' 2>/dev/null; then
	python3 -I - "$out" <<'EOF'
import glob, os, sys
from PIL import Image
for p in glob.glob(os.path.join(sys.argv[1], "*.pgm")):
    Image.open(p).save(p[:-4] + ".png")
EOF
fi
exit $rc
