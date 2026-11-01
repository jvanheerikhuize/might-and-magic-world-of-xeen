#!/usr/bin/env bash
# Build ScummVM (submodule) on the PC with only the Xeen engine.
#
#   tools/build-host.sh [null|sdl]     default: sdl
#
# null: no display; for detection, boot and memory checks (no SDL needed).
# sdl:  normal window to play in (needs: sudo apt install libsdl2-dev).
# Output: build/host-<backend>/scummvm
set -euo pipefail

backend=${1:-sdl}
root=$(cd "$(dirname "$0")/.." && pwd)
out=$root/build/host-$backend

[[ -x $root/scummvm/configure ]] || { echo "scummvm submodule missing: git submodule update --init --depth 1" >&2; exit 1; }

mkdir -p "$out"
cd "$out"
# Same engine/feature trimming the device build will use: Xeen only,
# no network, cloud, TTS, soft-synths or OpenGL game renderers.
"$root/scummvm/configure" \
	--backend="$backend" \
	--disable-all-engines --enable-engine=xeen \
	--disable-mt32emu --disable-fluidsynth --disable-tts \
	--disable-cloud --disable-libcurl --disable-sdlnet \
	--disable-opengl-game --disable-taskbar --disable-system-dialogs \
	>configure.log
grep -A2 'Engines (builtin)' configure.log
make -j"$(nproc)" >make.log
echo "built $out/scummvm"
echo "run:   $out/scummvm --extrapath=$root/scummvm/dists/engine-data --path=\$HOME/games/xeen/scummvm/world-of-xeen mm:worldofxeen"
