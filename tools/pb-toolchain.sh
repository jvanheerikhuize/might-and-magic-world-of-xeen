#!/usr/bin/env bash
# Create wrappers for the PocketBook SDK cross tools in build/pb-bin/ and
# print the PATH to use. The SDK-B288 compiler needs the SDK's own host
# libraries (libmpfr.so.4 ...) on LD_LIBRARY_PATH; the wrappers set it for
# the tool process only, so make, sh and the host compiler are unaffected.
#
#   eval "$(tools/pb-toolchain.sh)"     # puts arm-obreey-linux-gnueabi-* on PATH
#
# SDK defaults to the tree inkquest uses; override with SDK=/path/to/SDK-B288.
set -euo pipefail

SDK=${SDK:-$HOME/opt/pocketbook-sdk/SDK_6.3.0-6.5/SDK-B288}
root=$(cd "$(dirname "$0")/.." && pwd)
bin=$root/build/pb-bin
triple=arm-obreey-linux-gnueabi

[[ -x $SDK/usr/bin/$triple-g++ ]] || { echo "SDK not found at $SDK" >&2; exit 1; }
mkdir -p "$bin"
for tool in gcc g++ cpp ar ranlib strip nm objcopy ld as readelf; do
	real=$SDK/usr/bin/$triple-$tool
	[[ -x $real ]] || continue
	cat >"$bin/$triple-$tool" <<EOF
#!/bin/sh
LD_LIBRARY_PATH="$SDK/usr/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}" exec "$real" "\$@"
EOF
	chmod +x "$bin/$triple-$tool"
done
echo "export PATH=\"$bin:\$PATH\""
