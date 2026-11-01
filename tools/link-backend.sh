#!/usr/bin/env bash
# Put the mmIV PocketBook backend into the ScummVM submodule:
#   - symlink backend/pocketbook -> scummvm/backends/platform/pocketbook
#   - apply patches/*.patch (configure: the "pocketbook" backend case)
# Idempotent: safe to run before every build. The submodule's working tree
# becomes "modified"; that is expected and never committed upstream.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
link=$root/scummvm/backends/platform/pocketbook

if [[ ! -L $link ]]; then
	[[ -e $link ]] && { echo "$link exists and is not our symlink" >&2; exit 1; }
	ln -s ../../../backend/pocketbook "$link"
fi

cd "$root/scummvm"
for p in "$root"/patches/*.patch; do
	if patch -p1 --dry-run --reverse --force <"$p" >/dev/null 2>&1; then
		continue  # already applied
	fi
	patch -p1 --forward <"$p"
done
