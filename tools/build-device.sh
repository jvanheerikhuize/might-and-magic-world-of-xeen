#!/usr/bin/env bash
# Cross-build ScummVM (Xeen only) for the PocketBook Touch Lux 5.
#
#   tools/build-device.sh [backend] [--make-only]     default: pocketbook
#
# --make-only: skip configure and continue an existing build (fast rebuild).
# Output: build/device-<backend>/scummvm
# Flags match the vendor ABI proven in Phase 1 (soft-float, static C++ runtime).
set -euo pipefail

backend=${1:-pocketbook}
make_only=${2:-}
root=$(cd "$(dirname "$0")/.." && pwd)
out=$root/build/device-$backend
eval "$("$root/tools/pb-toolchain.sh")"

if [[ $backend == pocketbook ]]; then
	"$root/tools/link-backend.sh"
fi

mkdir -p "$out"
cd "$out"
if [[ $make_only == --make-only && -f config.mk ]]; then
	make -j"$(nproc)" >make.log
	arm-obreey-linux-gnueabi-strip -o scummvm.stripped scummvm
	echo "built $out/scummvm ($(stat -c %s scummvm.stripped) bytes stripped)"
	exit 0
fi
export CXXFLAGS="-fsigned-char -march=armv7-a -mfpu=neon -mfloat-abi=softfp -O2"
export LDFLAGS="-static-libstdc++ -static-libgcc -pthread"
"$root/scummvm/configure" \
	--host=arm-obreey-linux-gnueabi \
	--backend="$backend" \
	--enable-release --disable-debug \
	--disable-all-engines --enable-engine=xeen \
	--disable-mt32emu --disable-fluidsynth --disable-tts \
	--disable-cloud --disable-libcurl --disable-sdlnet \
	--disable-opengl-game --disable-taskbar --disable-system-dialogs \
	--disable-alsa --disable-seq-midi --disable-sndio --disable-timidity \
	--disable-vorbis --disable-tremor --disable-flac --disable-ogg \
	--disable-mad --disable-faad --disable-theoradec --disable-vpx \
	--disable-fluidlite --disable-sonivox --disable-bink \
	--disable-png --disable-gif --disable-jpeg --disable-freetype2 \
	--disable-fribidi --disable-zlib --disable-readline --disable-libunity \
	--disable-discord --disable-gtk --disable-enet --disable-lua \
	--disable-eventrecorder --disable-translation --disable-tinygl \
	--disable-scalers --disable-hq-scalers --disable-edge-scalers \
	>configure.log
grep -A2 'Engines (builtin)' configure.log
make -j"$(nproc)" >make.log
arm-obreey-linux-gnueabi-strip -o scummvm.stripped scummvm
echo "built $out/scummvm ($(stat -c %s scummvm.stripped) bytes stripped)"
