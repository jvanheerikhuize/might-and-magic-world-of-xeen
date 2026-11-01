#!/usr/bin/env bash
# Copy libpbagent (pbtk's in-app remote-control agent) from the pbtk toolkit
# into backend/pocketbook/pbagent/ and record which toolkit commit it came from.
#
#   tools/update-pbagent.sh            # PBTK=~/Repos/pocketbook_lux_5 by default
#
# The copy is checked in so the build and CI do not need the toolkit repo.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
pbtk=${PBTK:-$HOME/Repos/pocketbook_lux_5}
src=$pbtk/device/libpbagent
dst=$root/backend/pocketbook/pbagent

[[ -f $src/pbagent.c ]] || { echo "libpbagent not found in $src (set PBTK=/path/to/toolkit)" >&2; exit 1; }
cp "$src/pbagent.c" "$src/pbagent.h" "$dst/"
rev=$(git -C "$pbtk" rev-parse --short HEAD 2>/dev/null || echo unknown)
dirty=$(git -C "$pbtk" status --porcelain -- device/libpbagent 2>/dev/null | head -c1)
echo "$rev${dirty:+-dirty}" >"$dst/VERSION"
echo "libpbagent from $pbtk at $(cat "$dst/VERSION")"
